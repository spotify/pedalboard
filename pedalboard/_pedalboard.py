#! /usr/bin/env python
#
# Copyright 2021 Spotify AB
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

import platform
import re
import weakref
from contextlib import contextmanager
from typing import (
    Dict,
    Iterable,
    List,
    Optional,
    Set,
    Tuple,
    Type,
    Union,
    cast,
    no_type_check,
)

from pedalboard_native import (  # type: ignore
    ExternalPlugin,  # type: ignore
    Plugin,
    _AudioProcessorParameter,
)
from pedalboard_native.utils import Chain  # type: ignore


class Pedalboard(Chain):
    """
    A container for a series of :class:`Plugin` objects, to use for processing audio, like a
    `guitar pedalboard <https://en.wikipedia.org/wiki/Guitar_pedalboard>`_.

    :class:`Pedalboard` objects act like regular Python ``List`` objects,
    but come with an additional :py:meth:`process` method (also aliased to :py:meth:`__call__`),
    allowing audio to be passed through the entire :class:`Pedalboard` object for processing::

        my_pedalboard = Pedalboard()
        my_pedalboard.append(Reverb())
        output_audio = my_pedalboard(input_audio)

    .. warning::
        :class:`Pedalboard` objects may only contain effects plugins (i.e.: those for which
        :attr:`is_effect` is ``True``), and cannot contain instrument plugins (i.e.: those
        for which :attr:`is_instrument` is ``True``).
    """

    def __init__(self, plugins: Optional[List[Plugin]] = None):
        super().__init__(plugins or [])

    def __repr__(self) -> str:
        return "<{} with {} plugin{}: {}>".format(
            self.__class__.__name__,
            len(self),
            "" if len(self) == 1 else "s",
            list(self),
        )


# Stored in a trie to speed up these matches, as they happen millions of times in test:
FLOAT_SUFFIXES_TO_IGNORE = ["x", "%", "*", ",", ".", "hz", "ms", "db", "sec", "dbtp", "seconds"]


class _SuffixTrie:
    def __init__(self, char: Optional[str]):
        self.char = char
        self.children: Dict[str, Optional["_SuffixTrie"]] = {}
        self.is_match = False

    def __repr__(self) -> str:
        return f"<_SuffixTrie {self.char} {self.is_match} {self.children}>"


FLOAT_SUFFIX_TRIE = _SuffixTrie("")
for suffix in FLOAT_SUFFIXES_TO_IGNORE:
    node = FLOAT_SUFFIX_TRIE
    for char in suffix[::-1]:
        if char not in node.children:
            node.children[char] = _SuffixTrie(char)
        node = cast(_SuffixTrie, node.children[char])
    node.is_match = True


def strip_common_float_suffixes(
    s: Union[float, str, bool], strip_si_prefixes: bool = True
) -> Union[float, str, bool]:
    if not isinstance(s, str) or (hasattr(s, "type") and s.type is not str):  # type: ignore
        return s

    s = s.strip()
    s_lower = s.lower()

    # Handle certain plugins that report "Hz" and "kHz" suffixes:
    if strip_si_prefixes and s_lower.endswith("khz") and len(s) >= 3:
        try:
            return str(float(s[:-3]) * 1000)
        except ValueError:
            return s

    while True:
        node = FLOAT_SUFFIX_TRIE
        for i in reversed(range(len(s_lower))):
            char = s_lower[i]
            if char in node.children:
                node = cast(_SuffixTrie, node.children[char])
                if node.is_match:
                    s = s[:i].strip()
                    s_lower = s_lower[: len(s)]
                    break
            else:
                return s
        else:
            return s


def looks_like_float(s: Union[float, str]) -> bool:
    if isinstance(s, float):
        return True

    try:
        float(strip_common_float_suffixes(s))
        return True
    except ValueError:
        return False


class ReadOnlyDictWrapper(dict):
    def __setitem__(self, name, value):
        raise TypeError(
            "The .parameters dictionary on a Plugin instance returns "
            "a read-only dictionary of its parameters. To change a "
            "parameter, set the parameter on the plugin directly as "
            f"an attribute. (`my_plugin.{name} = {value}`)"
        )


# This is so nasty that I had to disable MyPy. Sorry.
@no_type_check
def wrap_type(base_type):
    @no_type_check
    class WeakTypeWrapper(base_type):
        """
        A wrapper around `base_type` that allows adding additional
        accessors through a weak reference. Useful for syntax convenience.
        """

        def __new__(cls, value, *args, **kwargs):
            try:
                return base_type.__new__(cls, value)
            except TypeError:
                return base_type.__new__(cls)

        def __init__(self, *args, **kwargs):
            if "wrapped" in kwargs:
                self._wrapped = weakref.ref(kwargs["wrapped"])
                del kwargs["wrapped"]
            else:
                raise ValueError(
                    "WeakTypeWrapper({}) expected to be passed a 'wrapped' kwarg.".format(base_type)
                )
            try:
                super().__init__(*args, **kwargs)
            except TypeError:
                pass

        def __getattr__(self, name):
            wrapped = self._wrapped()
            if hasattr(wrapped, name):
                return getattr(wrapped, name)
            if hasattr(super(), "__getattr__"):
                return super().__getattr__(name)
            raise AttributeError("'{}' has no attribute '{}'".format(base_type.__name__, name))

        def __dir__(self) -> Iterable[str]:
            wrapped = self._wrapped()
            if wrapped:
                return list(dir(wrapped)) + list(super().__dir__())
            return super().__dir__()

    return WeakTypeWrapper


@no_type_check
class WrappedBool(object):
    def __init__(self, value):
        if not isinstance(value, bool):
            raise TypeError(f"WrappedBool should be passed a boolean, got {type(value)}")
        self.__value = value

    def __repr__(self):
        return repr(self.__value)

    def __eq__(self, o: object) -> bool:
        return self.__value == o

    def __hash__(self) -> int:
        return hash(self.__value)

    def __bool__(self):
        return bool(self.__value)

    def __str__(self):
        return str(self.__value)

    def __getattr__(self, attr: str):
        return getattr(self.__value, attr)

    def __hasattr__(self, attr: str):
        return hasattr(self.__value, attr)


StringWithParameter: Type[str] = wrap_type(str)
FloatWithParameter: Type[float] = wrap_type(float)
BooleanWithParameter: Type[bool] = wrap_type(WrappedBool)


# Some plugins, on some platforms (TAL Reverb 3 on Ubuntu, so far) seem to
# expose every possible MIDI CC value as a separate parameter
# (i.e.: MIDI CC [0-16]|[0-128], resulting in 2,048 parameters).
# This hugely delays load times and adds complexity to the interface.
# Guitar Rig also seems to expose 512 parameters, each matching "P\d\d\d"
PARAMETER_NAME_REGEXES_TO_IGNORE = set(
    [re.compile(pattern) for pattern in ["MIDI CC ", r"P\d\d\d"]]
)

TRUE_BOOLEANS: Set[str] = {"on", "yes", "true", "enabled"}


def get_text_for_raw_value(
    cpp_parameter: _AudioProcessorParameter, raw_value: float, slow: bool = False
) -> Optional[str]:
    # Some plugins don't respond properly to get_text_for_raw_value,
    # but do respond when a parameter's raw value is changed.
    # This helper method works around this issue.
    if slow:
        original_value = cpp_parameter.raw_value
        try:
            cpp_parameter.raw_value = raw_value
            return cpp_parameter.string_value
        finally:
            cpp_parameter.raw_value = original_value
    else:
        return cpp_parameter.get_text_for_raw_value(raw_value)


class AudioProcessorParameter(object):
    """
    A wrapper around various different parameters exposed by
    :class:`VST3Plugin` or :class:`AudioUnitPlugin` instances.

    :class:`AudioProcessorParameter` objects are rarely used directly,
    and usually used via their implicit interface::

       my_plugin = load_plugin("My Cool Audio Effect.vst3")
       # Print all of the parameter names:
       print(my_plugin.parameters.keys())
       # ["mix", "delay_time_ms", "foobar"]
       # Access each parameter as if it were just a Python attribute:
       my_plugin.mix = 0.5
       my_plugin.delay_time_ms = 400

    .. note::
        :class:`AudioProcessorParameter` tries to guess the range of
        valid parameter values, as well as the type/unit of the parameter,
        when instantiated. This guess may not always be accurate.
        Raw control over the underlying parameter's value can be had by
        accessing the :py:attr:`raw_value` attribute, which is always bounded
        on [0, 1] and is passed directly to the underlying plugin object.
    """

    def __init__(self, plugin: "ExternalPlugin", parameter_name: str, search_steps: int = 1000):
        self.__plugin = plugin
        self.__parameter_name = parameter_name

        self.ranges: Dict[Tuple[float, float], Union[str, float, bool]] = {}

        with self.__get_cpp_parameter() as cpp_parameter:
            text_value: Optional[str] = None
            start_of_range: float = 0

            for fetch_slow in (False, True):
                start_of_range: float = 0
                text_value: Optional[str] = None

                self.ranges = {}
                for x in range(0, search_steps + 1):
                    raw_value = x / search_steps
                    x_text_value = get_text_for_raw_value(cpp_parameter, raw_value, fetch_slow)
                    if text_value is None:
                        text_value = x_text_value
                    elif x_text_value != text_value:
                        # End current range and start a new one
                        self.ranges[(start_of_range, raw_value)] = text_value
                        text_value = x_text_value
                        start_of_range = raw_value
                results_look_incorrect = not self.ranges or (
                    len(self.ranges) == 1 and all(looks_like_float(v) for v in self.ranges.values())
                )
                if not results_look_incorrect:
                    break
            if text_value is None:
                raise NotImplementedError(
                    f"Plugin parameter '{parameter_name}' failed to return a valid string for its"
                    " value."
                )
            self.ranges[(start_of_range, 1)] = text_value

            self.python_name = to_python_parameter_name(cpp_parameter)

        self.min_value = None
        self.max_value = None
        self.step_size = None
        self.approximate_step_size = None
        self.type: Type = str

        if all(looks_like_float(v) for v in self.ranges.values()):
            self.type = float
            float_ranges = {
                k: float(strip_common_float_suffixes(v)) for k, v in self.ranges.items()
            }
            self.min_value = min(float_ranges.values())
            self.max_value = max(float_ranges.values())

            if not self.label:
                # Try to infer the label from the string values:
                a_value = next(iter(self.ranges.values()))
                if isinstance(a_value, str):
                    stripped_value = strip_common_float_suffixes(a_value, strip_si_prefixes=False)
                    if (
                        stripped_value != a_value
                        and isinstance(stripped_value, str)
                        and stripped_value in a_value
                    ):
                        # We probably have a label! Iterate through all values just to make sure:
                        all_possible_labels = set()
                        for value in self.ranges.values():
                            if not isinstance(value, str):
                                continue
                            stripped_value = strip_common_float_suffixes(
                                value, strip_si_prefixes=False
                            )
                            if not isinstance(stripped_value, str):
                                continue
                            all_possible_labels.add(value.replace(stripped_value, "").strip())
                        if len(all_possible_labels) == 1:
                            self._label = next(iter(all_possible_labels))

            sorted_values = sorted(float_ranges.values())
            first_derivative_steps = set(
                [round(abs(b - a), 8) for a, b in zip(sorted_values, sorted_values[1:])]
            )
            if len(first_derivative_steps) == 1:
                self.step_size = next(iter(first_derivative_steps))
            elif first_derivative_steps:
                self.approximate_step_size = sum(first_derivative_steps) / len(
                    first_derivative_steps
                )
            self.ranges = dict(float_ranges)
        elif len(self.ranges) == 2 and (
            TRUE_BOOLEANS & {(v.lower() if isinstance(v, str) else v) for v in self.ranges.values()}
        ):
            self.type = bool
            self.ranges = {
                k: (v.lower() if isinstance(v, str) else v) in TRUE_BOOLEANS
                for k, v in self.ranges.items()
            }
            self.min_value = False
            self.max_value = True
            self.step_size = 1

        self.valid_values = list(self.ranges.values())
        self.range = self.min_value, self.max_value, self.step_size
        self._value_to_raw_value_ranges = {value: _range for _range, value in self.ranges.items()}

    @contextmanager
    def __get_cpp_parameter(self):
        """
        The C++ version of this class (`_AudioProcessorParameter`) is owned
        by the ExternalPlugin object and is not guaranteed to exist at the
        same memory address every time we might need it. This Python wrapper
        looks it up dynamically.
        """
        _parameter = self.__plugin._get_parameter(self.__parameter_name)  # type: ignore
        if _parameter and _parameter.name == self.__parameter_name:
            yield _parameter
            return
        raise RuntimeError(
            "Parameter {} on plugin {} is no longer available. This could indicate that the plugin"
            " has changed parameters.".format(self.__parameter_name, self.__plugin)
        )

    @property
    def label(self) -> Optional[str]:
        """
        The units used by this parameter (Hz, dB, etc).

        May be ``None`` if the plugin does not expose units for this
        parameter or if automatic unit detection fails.
        """
        if hasattr(self, "_label") and self._label:
            return self._label
        with self.__get_cpp_parameter() as parameter:
            if parameter.label:
                return parameter.label
        return None

    @property
    def units(self) -> Optional[str]:
        """
        Alias for "label" - the units used by this parameter (Hz, dB, etc).

        May be ``None`` if the plugin does not expose units for this
        parameter or if automatic unit detection fails.
        """
        return self.label

    def __repr__(self):
        with self.__get_cpp_parameter() as parameter:
            cpp_repr_value = repr(parameter)
            cpp_repr_value = cpp_repr_value.rstrip(">")
            if self.type is float:
                if self.step_size:
                    return "{} value={} range=({}, {}, {})>".format(
                        cpp_repr_value,
                        self.string_value,
                        self.min_value,
                        self.max_value,
                        self.step_size,
                    )
                elif self.approximate_step_size:
                    return "{} value={} range=({}, {}, ~{})>".format(
                        cpp_repr_value,
                        self.string_value,
                        self.min_value,
                        self.max_value,
                        self.approximate_step_size,
                    )
                else:
                    return "{} value={} range=({}, {}, ?)>".format(
                        cpp_repr_value, self.string_value, self.min_value, self.max_value
                    )
            elif self.type is str:
                return '{} value="{}" ({} valid string value{})>'.format(
                    cpp_repr_value,
                    self.string_value,
                    len(self.valid_values),
                    "" if len(self.valid_values) == 1 else "s",
                )
            elif self.type is bool:
                return '{} value={} boolean ("{}" and "{}")>'.format(
                    cpp_repr_value, self.string_value, self.valid_values[0], self.valid_values[1]
                )
            else:
                raise ValueError(
                    f"Parameter {self.python_name} has an unknown type. (Found '{self.type}')"
                )

    def __getattr__(self, name: str):
        if not name.startswith("_"):
            try:
                with self.__get_cpp_parameter() as parameter:
                    return getattr(parameter, name)
            except RuntimeError:
                pass
        if hasattr(super(), "__getattr__"):
            return super().__getattr__(name)  # type: ignore
        raise AttributeError("'{}' has no attribute '{}'".format(self.__class__.__name__, name))

    def __setattr__(self, name: str, value):
        if not name.startswith("_"):
            try:
                with self.__get_cpp_parameter() as parameter:
                    if hasattr(parameter, name):
                        return setattr(parameter, name, value)
            except RuntimeError:
                pass
        return super().__setattr__(name, value)

    def get_raw_value_for(self, new_value: Union[float, str, bool]) -> float:
        if self.type is float:
            to_float_value: Union[str, float, bool]
            if isinstance(new_value, str) and self.label and new_value.endswith(self.label):
                to_float_value = new_value[: -len(self.label)]
            else:
                to_float_value = new_value

            try:
                new_value = float(to_float_value)
            except ValueError:
                if self.label:
                    raise ValueError(
                        "Value received for parameter '{}' ({}) must be a number or a string (with"
                        " the optional suffix '{}')".format(
                            self.python_name, repr(new_value), self.label
                        )
                    )
                raise ValueError(
                    "Value received for parameter '{}' ({}) must be a number or a string".format(
                        self.python_name, repr(new_value)
                    )
                )
            if (self.min_value is not None and new_value < self.min_value) or (
                self.max_value is not None and new_value > self.max_value
            ):
                raise ValueError(
                    "Value received for parameter '{}' ({}) is out of range [{}{}, {}{}]".format(
                        self.python_name,
                        repr(new_value),
                        self.min_value,
                        self.label,
                        self.max_value,
                        self.label,
                    )
                )
            plugin_reported_raw_value = self.get_raw_value_for_text(str(new_value))

            closest_diff = None
            closest_range_value: Optional[Tuple[float, float]] = None
            for value, raw_value_range in self._value_to_raw_value_ranges.items():
                if not isinstance(value, float):
                    continue
                diff = new_value - value
                if closest_diff is None or abs(diff) < abs(closest_diff):
                    closest_range_value = raw_value_range
                    closest_diff = diff

            if closest_range_value is not None:
                expected_low, expected_high = closest_range_value
                if (
                    plugin_reported_raw_value < expected_low
                    or plugin_reported_raw_value > expected_high
                ):
                    # The plugin might have bad code in it when trying
                    # to parse one of the string values it gave to us.
                    # Let's use the range we had before:
                    return expected_low
            return plugin_reported_raw_value

        elif self.type is str:
            if isinstance(new_value, (str, int, float, bool)):
                new_value = str(new_value)
            else:
                raise ValueError(
                    "Value received for parameter '{}' ({}) should be a string (or string-like),"
                    " but got an object of type: {}".format(
                        self.python_name, repr(new_value), type(new_value)
                    )
                )
            if new_value not in self.valid_values:
                raise ValueError(
                    "Value received for parameter '{}' ({}) not in list of valid values: {}".format(
                        self.python_name, repr(new_value), self.valid_values
                    )
                )
            plugin_reported_raw_value = self.get_raw_value_for_text(new_value)
            expected_low, expected_high = self._value_to_raw_value_ranges[new_value]
            if (
                plugin_reported_raw_value < expected_low
                or plugin_reported_raw_value > expected_high
            ):
                # The plugin might have bad code in it when trying
                # to parse one of the string values it gave to us.
                # Let's use the range we had before:
                return expected_low
            else:
                return plugin_reported_raw_value
        elif self.type is bool:
            if not isinstance(new_value, (bool, WrappedBool)):
                raise ValueError(
                    "Value received for parameter '{}' ({}) should be a boolean,"
                    " but got an object of type: {}".format(
                        self.python_name, repr(new_value), type(new_value)
                    )
                )
            return 1.0 if new_value else 0.0
        else:
            raise ValueError(
                "Parameter has invalid type: {}. This should not be possible!".format(self.type)
            )


def to_python_parameter_name(parameter: _AudioProcessorParameter) -> Optional[str]:
    if not parameter.name and not parameter.label:
        return None

    name = parameter.name
    if parameter.label and not parameter.label.startswith(":"):
        name = "{} {}".format(name, parameter.label)
    return normalize_python_parameter_name(name)


def normalize_python_parameter_name(name: str) -> str:
    name = name.lower().strip()
    # Special case: some plugins expose parameters with "#"/"♯" or "b"/"♭" in their names.
    name = name.replace("#", "_sharp").replace("♯", "_sharp").replace("♭", "_flat")
    # Replace all non-alphanumeric characters with underscores
    name_chars = [
        c if (c.isalpha() or c.isnumeric()) and c.isprintable() and ord(c) < 128 else "_"
        for c in name
    ]
    if not name_chars:  # Unhandled/Weird param name
        return ""
    # Remove any double-underscores:
    name_chars = [a for a, b in zip(name_chars, name_chars[1:]) if a != b or b != "_"] + [
        name_chars[-1]
    ]
    # Remove any leading or trailing underscores:
    name = "".join(name_chars).strip("_")
    return name


class _PythonExternalPluginMixin:
    def __set_initial_parameter_values__(
        self, parameter_values: Optional[Dict[str, Union[str, int, float, bool]]]
    ):
        if parameter_values is None:
            parameter_values = {}
        if not isinstance(parameter_values, dict):
            raise TypeError(
                "Expected a dictionary to be passed to parameter_values, but received a"
                f" {type(parameter_values).__name__}. (If passing a plugin name, pass"
                ' "plugin_name=..." as a keyword argument instead.)'
            )
        parameters = self.parameters
        for key, value in parameter_values.items():
            if key not in parameters:
                raise AttributeError(
                    'Parameter named "{}" not found. Valid options: {}'.format(
                        key,
                        ", ".join(
                            cast(
                                Dict[str, AudioProcessorParameter], self._parameter_weakrefs
                            ).keys()
                        ),
                    )
                )
            setattr(self, key, value)

    @property
    def parameters(self) -> Dict[str, AudioProcessorParameter]:
        # Return a read-only version of this dictionary,
        # to prevent people from trying to assign to it.
        return ReadOnlyDictWrapper(self._get_parameters())

    def _get_parameters(self):
        if not hasattr(self, "__python_parameter_cache__"):
            self.__python_parameter_cache__ = {}
        if not hasattr(self, "__python_to_cpp_names__"):
            self.__python_to_cpp_names__ = {}

        parameters = {}
        for cpp_parameter in self._parameters:  # type: ignore
            if any([regex.match(cpp_parameter.name) for regex in PARAMETER_NAME_REGEXES_TO_IGNORE]):
                continue
            if cpp_parameter.name not in self.__python_parameter_cache__:
                self.__python_parameter_cache__[cpp_parameter.name] = AudioProcessorParameter(
                    self,  # type: ignore
                    cpp_parameter.name,
                )
            parameter = self.__python_parameter_cache__[cpp_parameter.name]
            if parameter.python_name:
                parameters[parameter.python_name] = parameter
                self.__python_to_cpp_names__[parameter.python_name] = cpp_parameter.name
        return parameters

    def _get_parameter_by_python_name(self, python_name: str) -> Optional[AudioProcessorParameter]:
        if not hasattr(self, "__python_parameter_cache__"):
            self.__python_parameter_cache__ = {}
        if not hasattr(self, "__python_to_cpp_names__"):
            self.__python_to_cpp_names__ = {}

        cpp_name = self.__python_to_cpp_names__.get(python_name)
        if not cpp_name:
            return self._get_parameters().get(python_name)

        cpp_parameter = self._get_parameter(cpp_name)  # type: ignore
        if not cpp_parameter:
            return None

        if cpp_parameter.name not in self.__python_parameter_cache__:
            self.__python_parameter_cache__[cpp_parameter.name] = AudioProcessorParameter(
                cast(ExternalPlugin, self), cpp_parameter.name
            )
        return self.__python_parameter_cache__[cpp_parameter.name]

    def __dir__(self):
        parameter_names = []
        for parameter in self._parameters:  # type: ignore
            name = to_python_parameter_name(parameter)
            if name:
                parameter_names.append(name)
        return super().__dir__() + parameter_names  # type: ignore

    def __getattr__(self, name: str):
        if not name.startswith("_"):
            parameter = self._get_parameter_by_python_name(name)
            if parameter:
                string_value = parameter.string_value
                if parameter.type is float:
                    return FloatWithParameter(
                        float(strip_common_float_suffixes(string_value)),
                        wrapped=parameter,  # type: ignore
                    )  # type: ignore
                elif parameter.type is bool:
                    return BooleanWithParameter(
                        parameter.raw_value >= 0.5,
                        wrapped=parameter,  # type: ignore
                    )  # type: ignore
                elif parameter.type is str:
                    return StringWithParameter(str(string_value), wrapped=parameter)  # type: ignore
                else:
                    raise ValueError(
                        f"Parameter {parameter.python_name} has an unknown type. (Found"
                        f" '{parameter.type}')"
                    )
        return getattr(super(), name)

    def __setattr__(self, name: str, value):
        if not name.startswith("__"):
            parameter = self._get_parameter_by_python_name(name)
            if parameter:
                parameter.raw_value = parameter.get_raw_value_for(value)
                return
        super().__setattr__(name, value)


ExternalPlugin.__bases__ = ExternalPlugin.__bases__ + (_PythonExternalPluginMixin,)


_AVAILABLE_PLUGIN_CLASSES: List[Type[ExternalPlugin]] = []

try:
    from pedalboard_native import VST3Plugin  # type: ignore

    _AVAILABLE_PLUGIN_CLASSES.append(VST3Plugin)
except ImportError:
    # We may be on a system that doesn't have native VST3Plugin support.
    pass

try:
    from pedalboard_native import AudioUnitPlugin  # type: ignore

    _AVAILABLE_PLUGIN_CLASSES.append(AudioUnitPlugin)
except ImportError:
    # We may be on a system that doesn't have native AudioUnitPlugin support.
    # (i.e.: any platform that's not macOS.)
    pass


def load_plugin(
    path_to_plugin_file: str,
    parameter_values: Dict[str, Union[str, int, float, bool]] = {},
    plugin_name: Union[str, None] = None,
    initialization_timeout: float = 10.0,
) -> ExternalPlugin:
    """
    Load an audio plugin.

    Two plugin formats are supported:
     - VST3® format is supported on macOS, Windows, and Linux
     - Audio Units are supported on macOS

    Args:
        path_to_plugin_file (``str``): The path of a VST3® or Audio Unit plugin file or bundle.

        parameter_values (``Dict[str, Union[str, int, float, bool]]``):
            An optional dictionary of initial values to provide to the plugin
            after loading. Keys in this dictionary are expected to match the
            parameter names reported by the plugin, but normalized to strings
            that can be used as Python identifiers. (These are the same
            identifiers that are used as keys in the ``.parameters`` dictionary
            of a loaded plugin.)

        plugin_name (``Optional[str]``):
            An optional plugin name that can be used to load a specific plugin
            from a multi-plugin package. If a package is loaded but a
            ``plugin_name`` is not provided, an exception will be thrown.

        initialization_timeout (``float``):
            The number of seconds that Pedalboard will spend trying to load this plugin.
            Some plugins load resources asynchronously in the background on startup;
            using larger values for this parameter can give these plugins time to
            load properly.

            *Introduced in v0.7.6.*

    Returns:
        an instance of :class:`pedalboard.VST3Plugin` or :class:`pedalboard.AudioUnitPlugin`

    Throws:
        ``ImportError``: if the plugin cannot be found or loaded

        ``RuntimeError``: if the plugin file contains more than one plugin,
        but no ``plugin_name`` was provided
    """
    if not _AVAILABLE_PLUGIN_CLASSES:
        raise ImportError(
            "Pedalboard found no supported external plugin types in this installation ({}).".format(
                platform.system()
            )
        )
    exceptions = []
    for plugin_class in _AVAILABLE_PLUGIN_CLASSES:
        try:
            return plugin_class(  # type: ignore
                path_to_plugin_file=path_to_plugin_file,  # type: ignore
                parameter_values=parameter_values,  # type: ignore
                plugin_name=plugin_name,  # type: ignore
                initialization_timeout=initialization_timeout,  # type: ignore
            )  # type: ignore
        except ImportError as e:
            exceptions.append(e)
        except Exception:
            raise
    else:
        tried_plugins = ", ".join([c.__name__ for c in _AVAILABLE_PLUGIN_CLASSES])
        # Good error messages are important, okay?
        if len(_AVAILABLE_PLUGIN_CLASSES) > 2:
            tried_plugins = ", or ".join(tried_plugins.rsplit(", ", 1))
        else:
            tried_plugins = " or ".join(tried_plugins.rsplit(", ", 1))
        raise ImportError(
            "Failed to load plugin as {}. Errors were:\n\t{}".format(
                tried_plugins,
                "\n\t".join(
                    [
                        "{}: {}".format(klass.__name__, exception)
                        for klass, exception in zip(_AVAILABLE_PLUGIN_CLASSES, exceptions)
                    ]
                ),
            )
        )
