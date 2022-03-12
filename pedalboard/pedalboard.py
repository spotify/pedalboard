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

import re
import platform
import weakref
from functools import update_wrapper
from contextlib import contextmanager
from typing import List, Optional, Dict, Tuple, Iterable, Union

from pedalboard_native import Plugin, _AudioProcessorParameter
from pedalboard_native.utils import Chain


class Pedalboard(Chain):
    """
    A container for a chain of plugins, to use for processing audio.
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


FLOAT_SUFFIXES_TO_IGNORE = set(["x", "%", "*", ",", ".", "hz"])


def strip_common_float_suffixes(s: str) -> str:
    value = s.lower().strip()
    for suffix in FLOAT_SUFFIXES_TO_IGNORE:
        if value[-len(suffix) :] == suffix:
            value = value[: -len(suffix)].strip()
    return value


def looks_like_float(s: str) -> bool:
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


def wrap_type(base_type):
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
                    "WeakTypeWrapper({}) expected to be passed a 'wrapped' keyword argument."
                    .format(base_type)
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


StringWithParameter = wrap_type(str)
FloatWithParameter = wrap_type(float)
BooleanWithParameter = wrap_type(WrappedBool)


# Some plugins, on some platforms (TAL Reverb 3 on Ubuntu, so far) seem to
# expose every possible MIDI CC value as a separate parameter
# (i.e.: MIDI CC [0-16]|[0-128], resulting in 2,048 parameters).
# This hugely delays load times and adds complexity to the interface.
# Guitar Rig also seems to expose 512 parameters, each matching "P\d\d\d"
PARAMETER_NAME_REGEXES_TO_IGNORE = set(
    [re.compile(pattern) for pattern in ["MIDI CC ", r"P\d\d\d"]]
)

TRUE_BOOLEANS = {"on", "yes", "true", "enabled"}


class AudioProcessorParameter(object):
    """
    The C++ version of this class (`_AudioProcessorParameter`) is owned
    by the ExternalPlugin object and is not guaranteed to exist at the
    same memory address every time we might need it. This Python wrapper
    looks it up dynamically.

    VSTs and Audio Units don't always consistently give parameter values,
    types, or names - all we get is APIs that map float values on [0, 1]
    to strings, which may represent floats, strings, integers, enums, etc.

    AudioProcessorParameter exposes some attributes that can be
    implemented by plugins to give hints (i.e.: num_steps, allowed_values,
    is_discrete, etc) but not all plugins implement them properly.

    This method assigns additional properties to AudioProcessorParameter;
    we could do this in C++, but doing this in Python uses much less code
    and is (roughly) as performant as it should only run once per parameter.
    """

    def __init__(self, plugin, parameter_name, search_steps: int = 1000):
        self.__plugin = plugin
        self.__parameter_name = parameter_name

        self.ranges: Dict[Tuple[float, float], Union[str, float, bool]] = {}

        with self.__get_cpp_parameter() as cpp_parameter:
            start_of_range = 0
            text_value = None
            for x in range(0, search_steps + 1):
                raw_value = x / search_steps
                x_text_value = cpp_parameter.get_text_for_raw_value(raw_value)
                if text_value is None:
                    text_value = x_text_value
                elif x_text_value != text_value:
                    # End current range and start a new one
                    self.ranges[(start_of_range, raw_value)] = text_value
                    text_value = x_text_value
                    start_of_range = raw_value
            self.ranges[(start_of_range, 1)] = text_value

            self.python_name = to_python_parameter_name(cpp_parameter)

        self.min_value = None
        self.max_value = None
        self.step_size = None
        self.approximate_step_size = None
        self.type = str

        if all(looks_like_float(v) for v in self.ranges.values()):
            self.type = float
            self.ranges = {k: float(strip_common_float_suffixes(v)) for k, v in self.ranges.items()}
            self.min_value = min(self.ranges.values())
            self.max_value = max(self.ranges.values())

            sorted_values = sorted(self.ranges.values())
            first_derivative_steps = set(
                [round(abs(b - a), 8) for a, b in zip(sorted_values, sorted_values[1:])]
            )
            if len(first_derivative_steps) == 1:
                self.step_size = next(iter(first_derivative_steps))
            elif first_derivative_steps:
                self.approximate_step_size = sum(first_derivative_steps) / len(
                    first_derivative_steps
                )
        elif len(self.ranges) == 2 and (TRUE_BOOLEANS & {v.lower() for v in self.ranges.values()}):
            self.type = bool
            self.ranges = {k: v.lower() in TRUE_BOOLEANS for k, v in self.ranges.items()}
            self.min_value = False
            self.max_value = True
            self.step_size = 1

        self.valid_values = list(self.ranges.values())
        self.range = self.min_value, self.max_value, self.step_size
        self._value_to_raw_value_ranges = {value: _range for _range, value in self.ranges.items()}

    @contextmanager
    def __get_cpp_parameter(self):
        # Re-fetch the parameter by name every time, as it may have changed.
        _parameter = self.__plugin._get_parameter(self.__parameter_name)
        if _parameter and _parameter.name == self.__parameter_name:
            yield _parameter
            return
        raise RuntimeError(
            "Parameter {} on plugin {} is no longer available. This could indicate that the plugin"
            " has changed parameters.".format(self.__parameter_name, self.__plugin)
        )

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
            return super().__getattr__(name)
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

    def get_raw_value_for(self, new_value) -> float:
        if self.type is float:
            try:
                new_value = float(new_value)
            except ValueError:
                raise ValueError(
                    "Value received for parameter '{}' ({}) must be a number".format(
                        self.python_name, new_value
                    )
                )
            if new_value < self.min_value or new_value > self.max_value:
                raise ValueError(
                    "Value received for parameter '{}' ({}) is out of range [{}{}, {}{}]".format(
                        self.python_name,
                        new_value,
                        self.min_value,
                        self.label,
                        self.max_value,
                        self.label,
                    )
                )
            plugin_reported_raw_value = self.get_raw_value_for_text(str(new_value))

            closest_diff = None
            closest_range_value = None
            for value, raw_value_range in self._value_to_raw_value_ranges.items():
                diff = new_value - value
                if closest_diff is None or abs(diff) < abs(closest_diff):
                    closest_range_value = raw_value_range
                    closest_diff = diff

            expected_low, expected_high = closest_range_value
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

        elif self.type is str:
            if isinstance(new_value, (str, int, float, bool)):
                new_value = str(new_value)
            else:
                raise ValueError(
                    "Value received for parameter '{}' ({}) should be a string (or string-like),"
                    " but got an object of type: {}".format(
                        self.python_name, new_value, type(new_value)
                    )
                )
            if new_value not in self.valid_values:
                raise ValueError(
                    "Value received for parameter '{}' ({}) not in list of valid values: {}".format(
                        self.python_name, new_value, self.valid_values
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
                        self.python_name, new_value, type(new_value)
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
    name = [
        c if (c.isalpha() or c.isnumeric()) and c.isprintable() and ord(c) < 128 else "_"
        for c in name
    ]
    # Remove any double-underscores:
    name = [a for a, b in zip(name, name[1:]) if a != b or b != "_"] + [name[-1]]
    # Remove any leading or trailing underscores:
    name = "".join(name).strip("_")
    return name


class ExternalPlugin(object):
    def __set_initial_parameter_values__(
        self, parameter_values: Dict[str, Union[str, int, float, bool]] = {}
    ):
        parameters = self.parameters
        for key, value in parameter_values.items():
            if key not in parameters:
                raise AttributeError(
                    'Parameter named "{}" not found. Valid options: {}'.format(
                        key, ", ".join(self._parameter_weakrefs.keys())
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
        for cpp_parameter in self._parameters:
            if any([regex.match(cpp_parameter.name) for regex in PARAMETER_NAME_REGEXES_TO_IGNORE]):
                continue
            if cpp_parameter.name not in self.__python_parameter_cache__:
                self.__python_parameter_cache__[cpp_parameter.name] = AudioProcessorParameter(
                    self, cpp_parameter.name
                )
            parameter = self.__python_parameter_cache__[cpp_parameter.name]
            if parameter.python_name:
                parameters[parameter.python_name] = parameter
                self.__python_to_cpp_names__[parameter.python_name] = cpp_parameter.name
        return parameters

    def _get_parameter_by_python_name(self, python_name: str) -> AudioProcessorParameter:
        if not hasattr(self, "__python_parameter_cache__"):
            self.__python_parameter_cache__ = {}
        if not hasattr(self, "__python_to_cpp_names__"):
            self.__python_to_cpp_names__ = {}

        cpp_name = self.__python_to_cpp_names__.get(python_name)
        if not cpp_name:
            return self._get_parameters().get(python_name)

        cpp_parameter = self._get_parameter(cpp_name)
        if not cpp_parameter:
            return None

        if cpp_parameter.name not in self.__python_parameter_cache__:
            self.__python_parameter_cache__[cpp_parameter.name] = AudioProcessorParameter(
                self, cpp_parameter.name
            )
        return self.__python_parameter_cache__[cpp_parameter.name]

    def __dir__(self):
        parameter_names = []
        for parameter in self._parameters:
            name = to_python_parameter_name(parameter)
            if name:
                parameter_names.append(name)
        return super().__dir__() + parameter_names

    def __getattr__(self, name: str):
        if not name.startswith("_"):
            parameter = self._get_parameter_by_python_name(name)
            if parameter:
                string_value = parameter.string_value
                if parameter.type is float:
                    return FloatWithParameter(
                        float(strip_common_float_suffixes(string_value)), wrapped=parameter
                    )
                elif parameter.type is bool:
                    return BooleanWithParameter(parameter.raw_value >= 0.5, wrapped=parameter)
                elif parameter.type is str:
                    return StringWithParameter(str(string_value), wrapped=parameter)
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


try:
    from pedalboard_native import _VST3Plugin

    class VST3Plugin(_VST3Plugin, ExternalPlugin):
        def __init__(
            self,
            path_to_plugin_file: str,
            parameter_values: Dict[str, Union[str, int, float, bool]] = {},
            plugin_name: Optional[str] = None,
        ):
            if not isinstance(parameter_values, dict):
                raise TypeError(
                    "Expected a dictionary to be passed to parameter_values, but received a"
                    f" {type(parameter_values).__name__}. (If passing a plugin name, pass"
                    ' "plugin_name=..." as a keyword argument instead.)'
                )
            super().__init__(path_to_plugin_file, plugin_name)
            self.__set_initial_parameter_values__(parameter_values)


except ImportError:
    # We may be on a system that doesn't have native VST3Plugin support.
    pass

try:
    from pedalboard_native import _AudioUnitPlugin

    class AudioUnitPlugin(_AudioUnitPlugin, ExternalPlugin):
        def __init__(
            self,
            path_to_plugin_file: str,
            parameter_values: Dict[str, Union[str, int, float, bool]] = {},
            plugin_name: Optional[str] = None,
        ):
            if not isinstance(parameter_values, dict):
                raise TypeError(
                    "Expected a dictionary to be passed to parameter_values, but received a"
                    f" {type(parameter_values).__name__}. (If passing a plugin name, pass"
                    ' "plugin_name=..." as a keyword argument instead.)'
                )
            super().__init__(path_to_plugin_file, plugin_name)
            self.__set_initial_parameter_values__(parameter_values)


except ImportError:
    # We may be on a system that doesn't have native AudioUnitPlugin support.
    # (i.e.: any platform that's not macOS.)
    pass


AVAILABLE_PLUGIN_CLASSES = list(ExternalPlugin.__subclasses__())


def load_plugin(*args, **kwargs):
    if not AVAILABLE_PLUGIN_CLASSES:
        raise ImportError(
            "Pedalboard found no supported external plugin types in this installation ({}).".format(
                platform.system()
            )
        )
    exceptions = []
    for plugin_class in AVAILABLE_PLUGIN_CLASSES:
        try:
            return plugin_class(*args, **kwargs)
        except ImportError as e:
            exceptions.append(e)
        except Exception:
            raise
    else:
        tried_plugins = ", ".join([c.__name__ for c in AVAILABLE_PLUGIN_CLASSES])
        # Good error messages are important, okay?
        if len(AVAILABLE_PLUGIN_CLASSES) > 2:
            tried_plugins = ", or ".join(tried_plugins.rsplit(", ", 1))
        else:
            tried_plugins = " or ".join(tried_plugins.rsplit(", ", 1))
        raise ImportError(
            "Failed to load plugin as {}. Errors were:\n\t{}".format(
                tried_plugins,
                "\n\t".join(
                    [
                        "{}: {}".format(klass.__name__, exception)
                        for klass, exception in zip(AVAILABLE_PLUGIN_CLASSES, exceptions)
                    ]
                ),
            )
        )


if AVAILABLE_PLUGIN_CLASSES:
    update_wrapper(load_plugin, AVAILABLE_PLUGIN_CLASSES[0].__init__)
