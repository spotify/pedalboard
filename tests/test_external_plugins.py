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


import os
import math
import platform
from glob import glob

from pedalboard.pedalboard import WrappedBool
import pytest
import pedalboard
import numpy as np


TEST_PLUGIN_BASE_PATH = os.path.join(os.path.abspath(os.path.dirname(__file__)), "plugins")

AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT = [
    os.path.basename(filename)
    for filename in glob(os.path.join(TEST_PLUGIN_BASE_PATH, platform.system(), "*"))
]

# Disable Audio Unit tests on GitHub Actions, as the
# action container fails to load Audio Units:
if os.getenv("CIBW_TEST_REQUIRES") or os.getenv("CI"):
    AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT = [
        f for f in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT if "component" not in f
    ]


def get_parameters(plugin_filename: str):
    try:
        return load_test_plugin(plugin_filename).parameters
    except ImportError:
        return {}


TEST_PLUGIN_CACHE = {}


def load_test_plugin(plugin_filename: str, *args, **kwargs):
    """
    Load a plugin file from disk, or use an existing instance to save
    on test runtime if we already have one in memory.

    The plugin filename is used here rather than the fully-qualified path
    to reduce line length in the pytest output.
    """

    key = repr((plugin_filename, args, tuple(kwargs.items())))
    if key not in TEST_PLUGIN_CACHE:
        TEST_PLUGIN_CACHE[key] = pedalboard.load_plugin(
            os.path.join(TEST_PLUGIN_BASE_PATH, platform.system(), plugin_filename), *args, **kwargs
        )
    return TEST_PLUGIN_CACHE[key]


def test_at_least_one_plugin_is_available_for_testing():
    assert AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_at_least_one_parameter(plugin_filename: str):
    """
    Many tests below are parametrized on the parameters of the plugin;
    if our parameter parsing code fails, those tests won't fail as
    there will just be no code to run. This works around that problem.
    """

    assert get_parameters(plugin_filename)


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_initial_parameters(plugin_filename: str):
    parameters = {
        k: v.min_value for k, v in get_parameters(plugin_filename).items() if v.type == float
    }

    # Reload the plugin, but set the initial parameters in the load call.
    plugin = load_test_plugin(plugin_filename, parameters)

    for name, value in parameters.items():
        assert getattr(plugin, name) == value


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == float]
    ],
)
def test_initial_parameter_validation(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)
    with pytest.raises(ValueError):
        load_test_plugin(
            plugin_filename, {parameter_name: getattr(plugin, parameter_name).max_value + 100}
        )


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_initial_parameter_validation_missing(plugin_filename: str):
    with pytest.raises(AttributeError):
        load_test_plugin(plugin_filename, {"missing_parameter": 123})


@pytest.mark.parametrize("loader", [pedalboard.load_plugin] + pedalboard.AVAILABLE_PLUGIN_CLASSES)
def test_import_error_on_missing_path(loader):
    with pytest.raises(ImportError):
        loader("./")


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
@pytest.mark.parametrize(
    "num_channels,sample_rate",
    [(1, 48000), (2, 48000), (1, 44100), (2, 44100), (1, 22050), (2, 22050)],
)
def test_plugin_accepts_variable_channel_count(
    plugin_filename: str, num_channels: int, sample_rate: float
):
    plugin = load_test_plugin(plugin_filename)
    noise = np.random.rand(num_channels, sample_rate)
    effected = plugin(noise, sample_rate)
    assert effected.shape == noise.shape


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_all_parameters_are_accessible_as_properties(plugin_filename: str):
    plugin = load_test_plugin(plugin_filename)
    assert plugin.parameters
    for parameter_name in plugin.parameters.keys():
        assert hasattr(plugin, parameter_name)


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_all_parameters_have_accessors(plugin_filename: str):
    plugin = load_test_plugin(plugin_filename)
    assert plugin.parameters
    for parameter_name, parameter in plugin.parameters.items():
        assert parameter_name in dir(plugin)
        parameter_value = getattr(plugin, parameter_name)
        assert hasattr(parameter_value, "raw_value")
        assert repr(parameter)
        assert isinstance(parameter_value, (float, WrappedBool, str))
        assert parameter_value.raw_value == parameter.raw_value
        assert parameter_value.range is not None


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_attributes_proxy(plugin_filename: str):
    plugin = load_test_plugin(plugin_filename)

    # Should be disallowed:
    with pytest.raises(AttributeError):
        plugin.parameters = "123"

    # Should be allowed:
    plugin.new_parameter = "123"


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == bool]
    ],
)
def test_bool_parameters(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)

    parameter_value = getattr(plugin, parameter_name)
    assert repr(parameter_value) in ("True", "False")
    assert parameter_value in (True, False)
    # Flip the parameter and ensure that it does change:
    setattr(plugin, parameter_name, not parameter_value)
    assert getattr(plugin, parameter_name) != parameter_value

    # Ensure that if we access an attribute that we're not adding to the value,
    # we fall back to the underlying type (bool) or we raise an exception if not:
    assert hasattr(parameter_value, "bit_length")
    with pytest.raises(AttributeError):
        parameter_value.something_that_doesnt_exist


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == bool]
    ],
)
def test_bool_parameter_valdation(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)
    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, 123.4)


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == float]
    ],
)
def test_float_parameters(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)
    parameter_value = getattr(plugin, parameter_name)
    assert repr(parameter_value) == repr(float(parameter_value))
    assert isinstance(parameter_value, float)
    # Change the parameter and ensure that it does change:
    _min, _max, _ = parameter_value.range
    step_size = parameter_value.step_size or parameter_value.approximate_step_size
    for new_value in np.arange(_min, _max, step_size):
        setattr(plugin, parameter_name, new_value)
        assert math.isclose(new_value, getattr(plugin, parameter_name), abs_tol=step_size * 2)

    # Ensure that if we access an attribute that we're not adding to the value,
    # we fall back to the underlying type (float) or we raise an exception if not:
    assert parameter_value.real == float(parameter_value)
    with pytest.raises(AttributeError):
        parameter_value.something_that_doesnt_exist


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == float]
    ],
)
def test_float_parameter_valdation(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)

    parameter = getattr(plugin, parameter_name)

    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, "not a float")

    max_range = parameter.max_value
    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, max_range + 100)

    min_range = parameter.min_value
    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, min_range - 100)


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == str]
    ],
)
def test_str_parameters(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)
    parameter_value = getattr(plugin, parameter_name)
    assert repr(parameter_value) == repr(parameter_value)
    assert isinstance(parameter_value, str)
    # Change the parameter and ensure that it does change:
    for new_value in parameter_value.valid_values:
        setattr(plugin, parameter_name, new_value)
        assert getattr(plugin, parameter_name) == new_value

    # Ensure that if we access an attribute that we're not adding to the value,
    # we fall back to the underlying type (str) or we raise an exception if not:
    assert parameter_value.lower()
    with pytest.raises(AttributeError):
        parameter_value.something_that_doesnt_exist


@pytest.mark.parametrize(
    "plugin_filename,parameter_name",
    [
        (path, parameter)
        for path in AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT
        for parameter in [k for k, v in get_parameters(path).items() if v.type == str]
    ],
)
def test_string_parameter_valdation(plugin_filename: str, parameter_name: str):
    plugin = load_test_plugin(plugin_filename)
    some_other_object = object()

    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, some_other_object)

    with pytest.raises(ValueError):
        setattr(plugin, parameter_name, "some value not present")


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_plugin_state_cleared_between_invocations(plugin_filename: str):
    plugin = load_test_plugin(plugin_filename)
    sr = 44100
    noise = np.random.rand(sr)
    silence = np.zeros_like(noise)

    # Passing zero inputs should return silence
    effected_silence = plugin(silence, sr)
    plugin_noise_floor = np.amax(np.abs(effected_silence))

    effected = plugin(noise, sr)
    assert np.amax(np.abs(effected)) > plugin_noise_floor * 10

    # Calling plugin again shouldn't allow any audio tails
    # to carry over from previous calls.
    assert np.allclose(plugin(silence, sr), effected_silence)


@pytest.mark.parametrize("plugin_filename", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_plugin_state_not_cleared_between_invocations(plugin_filename: str):
    plugin = load_test_plugin(plugin_filename)
    sr = 44100
    noise = np.random.rand(sr)
    silence = np.zeros_like(noise)

    # Passing zero inputs should return silence
    effected_silence = plugin(silence, sr, reset=False)
    plugin_noise_floor = np.amax(np.abs(effected_silence))

    effected = plugin(noise, sr, reset=False)
    assert np.amax(np.abs(effected)) > plugin_noise_floor * 10

    # Calling plugin again shouldn't allow any audio tails
    # to carry over from previous calls.
    assert np.allclose(plugin(silence, sr), effected_silence)


@pytest.mark.parametrize("value", (True, False))
def test_wrapped_bool(value: bool):
    wrapped = WrappedBool(value)
    assert wrapped == value
    assert repr(wrapped) == repr(value)
    assert hash(wrapped) == hash(value)
    assert bool(wrapped) == bool(value)
    assert str(wrapped) == str(value)
    for attr in dir(value):
        assert hasattr(wrapped, attr)


def test_wrapped_bool_requires_bool():
    with pytest.raises(TypeError):
        assert WrappedBool(None)
