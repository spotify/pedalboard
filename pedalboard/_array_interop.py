#! /usr/bin/env python
#
# Copyright 2026 Spotify AB
#
# Licensed under the GNU Public License, Version 3.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.gnu.org/licenses/gpl-3.0.html
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""Shared implementation of Pedalboard's ML framework modules.

``pedalboard.torch``, ``pedalboard.jax`` and ``pedalboard.tensorflow`` each call
:func:`make_namespace` with a function that converts NumPy arrays to that framework's
array type. Each module provides only Pedalboard's audio entry points:

* every plugin class, as a real subclass of the original that overrides only
  ``process`` and ``__call__``;
* ``AudioFile``, which wraps whatever ``pedalboard.io.AudioFile(...)`` returns and
  converts audio in ``read``, ``read_raw``, ``write`` and ``encode``;
* ``process``, ``time_stretch`` and ``load_plugin``.

Audio arguments may be any CPU array supporting DLPack. Returned NumPy arrays are
converted to the framework's array type. Nothing else is converted.
"""

from __future__ import annotations

import functools
from typing import Any, Callable, Dict

import numpy as np

import pedalboard
import pedalboard.io
from pedalboard._pedalboard import _load_plugin

FromNumpy = Callable[[np.ndarray], Any]

# DLPack device types whose memory NumPy can read: kDLCPU and kDLCUDAHost (pinned memory).
_CPU_DEVICE_TYPES = (1, 3)

# Plugin base classes, which can't process audio by themselves, aren't provided.
_PLUGIN_BASE_CLASSES = (pedalboard.Plugin, pedalboard.PluginContainer, pedalboard.ExternalPlugin)

# The PluginContainer methods that change which plugins a board holds.
_CONTAINER_MUTATORS = ("__init__", "append", "insert", "remove", "__setitem__", "__delitem__")


def _to_pedalboard(value):
    """Convert an argument for Pedalboard: DLPack arrays become NumPy (zero-copy) and
    framework audio files become the Pedalboard file they wrap. Anything else is unchanged."""
    if isinstance(value, _FrameworkAudioFile):
        return value._file
    if isinstance(value, np.ndarray) or not hasattr(value, "__dlpack__"):
        return value
    if getattr(value, "requires_grad", False):
        value = value.detach()  # torch refuses to export tensors that require grad
    device_type, _ = value.__dlpack_device__()
    if device_type not in _CPU_DEVICE_TYPES:
        raise ValueError(
            f"Pedalboard processes audio on the CPU, but this {type(value).__name__} is on "
            "another device. Move it to the CPU first (e.g. with .cpu() in PyTorch)."
        )
    try:
        return np.from_dlpack(value)
    except (BufferError, RuntimeError, TypeError) as e:
        raise TypeError(f"Pedalboard can't read audio from this {type(value).__name__}: {e}") from e


def _converting(function, to_framework, to_pedalboard=_to_pedalboard):
    """Wrap ``function`` to convert its arguments for Pedalboard and its result to the framework."""

    @functools.wraps(function)
    def wrapper(*args, **kwargs):
        args = [to_pedalboard(a) for a in args]
        kwargs = {k: to_pedalboard(v) for k, v in kwargs.items()}
        return to_framework(function(*args, **kwargs))

    return wrapper


class _FrameworkAudioFile:
    """Wraps whatever ``pedalboard.io.AudioFile(...)`` returns (a readable, writeable or
    resampled file). Audio methods are converted; metadata reads are passed through.

    The native file owns construction, codecs and file state. Conversion functions are
    injected into each adapter and carried over when resampling creates another file.
    """

    def __init__(self, native_file, to_numpy, from_numpy):
        self._file = native_file
        self._to_numpy = to_numpy
        self._from_numpy = from_numpy

    def _wrapping(self, native_file):
        audio_file = object.__new__(type(self))
        _FrameworkAudioFile.__init__(audio_file, native_file, self._to_numpy, self._from_numpy)
        return audio_file

    def _convert(self, method_name, *args, **kwargs):
        method = getattr(self._file, method_name)
        return _converting(method, self._from_numpy, self._to_numpy)(*args, **kwargs)

    def read(self, *args, **kwargs):
        return self._convert("read", *args, **kwargs)

    def read_raw(self, *args, **kwargs):
        return self._convert("read_raw", *args, **kwargs)

    def write(self, *args, **kwargs):
        return self._convert("write", *args, **kwargs)

    def resampled_to(self, *args, **kwargs):
        resampled = self._file.resampled_to(*args, **kwargs)
        return self if resampled is self._file else self._wrapping(resampled)

    def __getattr__(self, name):
        return getattr(self._file, name)

    def __dir__(self):
        return dir(self._file)

    def __repr__(self):
        return repr(self._file)

    def __enter__(self):
        self._file.__enter__()
        return self

    def __exit__(self, *exc_info):
        return self._file.__exit__(*exc_info)


def _keeps_children_alive(method):
    """Make a board hold Python references to its plugins after ``method`` changes them.

    pybind11 only remembers a plugin's Python subclass (e.g. ``pedalboard.torch.Gain``)
    while that Python object is alive. Without these references, a plugin whose only
    reference is its board would come back out of the board as a plain Pedalboard plugin.
    """

    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
        result = method(self, *args, **kwargs)
        self.__dict__["_framework_plugins"] = list(pedalboard.PluginContainer.__iter__(self))
        return result

    return wrapper


def _subclass(base: type, name: str, module_name: str, attributes: Dict[str, Any]) -> type:
    defaults = {"__module__": module_name, "__qualname__": name, "__doc__": base.__doc__}
    return type(name, (base,), {**defaults, **attributes})


def make_namespace(module_name: str, from_numpy: FromNumpy) -> Dict[str, Any]:
    """Build the public contents of a framework module (e.g. ``pedalboard.torch``)."""

    def to_framework(value):
        if not isinstance(value, np.ndarray):
            return value
        if not value.flags.writeable:
            value = value.copy()  # read-only arrays can't be exported via DLPack
        return from_numpy(value)

    convert = functools.partial(_converting, to_framework=to_framework)
    namespace: Dict[str, Any] = {}

    for plugin_class in vars(pedalboard).values():
        if not isinstance(plugin_class, type) or not issubclass(plugin_class, pedalboard.Plugin):
            continue
        if plugin_class in _PLUGIN_BASE_CLASSES:
            continue
        attributes = {
            "process": convert(plugin_class.process),
            "__call__": convert(plugin_class.__call__),
        }
        if issubclass(plugin_class, pedalboard.PluginContainer):
            for method_name in _CONTAINER_MUTATORS:
                attributes[method_name] = _keeps_children_alive(getattr(plugin_class, method_name))
        name = plugin_class.__name__
        namespace[name] = _subclass(plugin_class, name, module_name, attributes)

    class AudioFile(_FrameworkAudioFile):
        __doc__ = pedalboard.io.AudioFile.__doc__
        __module__ = module_name
        __qualname__ = "AudioFile"

        def __init__(self, *args, **kwargs):
            super().__init__(pedalboard.io.AudioFile(*args, **kwargs), _to_pedalboard, to_framework)

        encode = staticmethod(convert(pedalboard.io.AudioFile.encode))

    namespace["AudioFile"] = AudioFile
    namespace["process"] = convert(pedalboard.process)
    namespace["time_stretch"] = convert(pedalboard.time_stretch)

    external_plugin_classes = [namespace[c.__name__] for c in pedalboard._AVAILABLE_PLUGIN_CLASSES]
    namespace["load_plugin"] = functools.wraps(pedalboard.load_plugin)(
        functools.partial(_load_plugin, external_plugin_classes)
    )

    namespace["__all__"] = sorted(namespace)
    return namespace
