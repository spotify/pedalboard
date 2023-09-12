from __future__ import annotations
import pedalboard_native.utils

import typing

original_overload = typing.overload
__OVERLOADED_DOCSTRINGS = {}

def patch_overload(func):
    original_overload(func)
    if func.__doc__:
        __OVERLOADED_DOCSTRINGS[func.__qualname__] = func.__doc__
    else:
        func.__doc__ = __OVERLOADED_DOCSTRINGS.get(func.__qualname__)
    if func.__doc__:
        # Work around the fact that pybind11-stubgen generates
        # duplicate docstrings sometimes, once for each overload:
        docstring = func.__doc__
        if docstring[len(docstring) // 2 :].strip() == docstring[: -len(docstring) // 2].strip():
            func.__doc__ = docstring[len(docstring) // 2 :].strip()
    return func

typing.overload = patch_overload

from typing_extensions import Literal
from enum import Enum
import numpy
import pedalboard_native

_Shape = typing.Tuple[int, ...]

__all__ = ["Chain", "Mix", "time_stretch"]

class Chain(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    Run zero or more plugins as a plugin. Useful when used with the Mix plugin.
    """

    @typing.overload
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
    pass

class Mix(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    A utility plugin that allows running other plugins in parallel. All plugins provided will be mixed equally.
    """

    @typing.overload
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
    pass

def time_stretch(
    input_audio: numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]],
    samplerate: float,
    stretch_factor: float = 1.0,
    pitch_shift_in_semitones: float = 0.0,
) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
    """
    Time-stretch (and optionally pitch-shift) a buffer of audio, changing its length.

    Using a higher ``stretch_factor`` will shorten the audio - i.e., a ``stretch_factor``
    of ``2.0`` will double the *speed* of the audio and halve the *length* fo the audio,
    without changing the pitch of the audio.

    This function allows for changing the pitch of the audio during the time stretching
    operation. The ``stretch_factor`` and ``pitch_shift_in_semitones`` arguments are
    independent and do not affect each other (i.e.: you can change one, the other, or both
    without worrying about how they interact).

    .. warning::
        This is a function, not a :py:class:`Plugin` instance, and cannot be
        used in :py:class:`Pedalboard` objects, as it changes the duration of
        the audio stream.
    """
