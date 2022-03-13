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


import pytest
import numpy as np
from pedalboard import PythonPlugin
from .utils import generate_sine_at


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_python_plugin(sample_rate: float):
    sine = generate_sine_at(sample_rate).astype(np.float32)
    plugin = PythonPlugin(np.sqrt)
    np.testing.assert_allclose(np.sqrt(sine), plugin(sine, sample_rate))


@pytest.mark.parametrize("power", [1, 2, 3])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_python_plugin_with_lambda(sample_rate: float, power: float, num_channels: int):
    sine = generate_sine_at(sample_rate, num_channels=num_channels).astype(np.float32)
    plugin = PythonPlugin(lambda signal: np.power(signal, power))
    np.testing.assert_allclose(np.power(sine, power), plugin(sine, sample_rate))


class PowPlugin(object):
    def __init__(self, power: float):
        self.power = power
        self.was_prepared = False
        self.was_reset = False

    def prepare(self, sample_rate: float, num_channels: int, maximum_block_size: int):
        self.sample_rate = sample_rate
        self.num_channels = num_channels
        self.maximum_block_size = maximum_block_size
        self.was_prepared = True

    def reset(self):
        self.was_reset = True

    def process(self, signal: np.ndarray) -> np.ndarray:
        return np.power(signal, self.power)


@pytest.mark.parametrize("power", [1, 2, 3])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_python_plugin_with_object(sample_rate: float, power: float, num_channels: int):
    sine = generate_sine_at(sample_rate, num_channels=num_channels).astype(np.float32)

    python_object = PowPlugin(power=power)
    plugin = PythonPlugin(python_object)
    np.testing.assert_allclose(np.power(sine, power), plugin(sine, sample_rate))
    assert plugin.wrapped is python_object
    assert python_object.was_prepared
    assert python_object.was_reset
    assert python_object.sample_rate == sample_rate
    assert python_object.num_channels == num_channels


class UnpreparedPlugin(object):
    def prepare(self, wrong_signature: float):
        pass

    def reset(self):
        self.was_reset = True

    def process(self, signal: np.ndarray) -> np.ndarray:
        return np.power(signal, self.power)


def test_python_plugin_with_wrong_prepare_signature():
    with pytest.raises(RuntimeError):
        plugin = PythonPlugin(UnpreparedPlugin())
        plugin(np.random.rand(1, 10), 44100)


class BadResetPlugin(object):
    def reset(self):
        raise ValueError("Oh no!")

    def process(self, signal: np.ndarray) -> np.ndarray:
        return np.power(signal, self.power)


def test_python_plugin_with_exception_in_reset():
    plugin = PythonPlugin(BadResetPlugin())
    with pytest.raises(RuntimeError):
        plugin(np.random.rand(1, 10), 44100)


class BadProcessPlugin(object):
    def process(self, signal: np.ndarray) -> np.ndarray:
        raise ValueError("Too much data")


def test_python_plugin_with_exception_in_process():
    plugin = PythonPlugin(BadProcessPlugin())
    with pytest.raises(RuntimeError):
        plugin(np.random.rand(1, 10), 44100)


def test_python_plugin_returns_more_channels_than_provided():
    plugin = PythonPlugin(lambda x: np.concatenate([x, x]))
    with pytest.raises(ValueError) as e:
        plugin(np.random.rand(1, 10), 44100)
    assert "a different number of channels" in e.value.args[0]


def test_python_plugin_returns_more_samples_than_provided():
    plugin = PythonPlugin(lambda x: np.concatenate([x[0], x[0]]))
    with pytest.raises(ValueError) as e:
        plugin(np.random.rand(1, 10), 44100)
    assert "more samples" in e.value.args[0]


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_python_plugin_returns_fewer_samples_than_provided(sample_rate: float, num_channels: int):
    sine = generate_sine_at(sample_rate, num_channels=num_channels).astype(np.float32)

    last_buffer = [np.array([], dtype=np.float32).reshape(num_channels, 0)]

    def add_latency(signal: np.ndarray) -> np.ndarray:
        """
        Totally dumb plugin that introduces one buffer's worth of latency.
        """
        to_return = last_buffer[0]
        last_buffer[0] = signal
        return to_return[:, : signal.shape[1]]

    plugin = PythonPlugin(add_latency)
    np.testing.assert_allclose(sine, plugin(sine, 44100))
