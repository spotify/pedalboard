#! /usr/bin/env python
#
# Copyright 2022 Spotify AB
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
from pedalboard import Resample
from pedalboard_native._internal import ResampleWithLatency
from .utils import generate_sine_at

TOLERANCE_PER_QUALITY = {
    Resample.Quality.ZeroOrderHold: 0.65,
    Resample.Quality.Linear: 0.35,
    Resample.Quality.CatmullRom: 0.16,
    Resample.Quality.Lagrange: 0.16,
    Resample.Quality.WindowedSinc: 0.151,
}

DURATIONS = [0.345678]


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 32, 8192, 1_000_000])
@pytest.mark.parametrize("duration", DURATIONS)
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_resample(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    buffer_size: int,
    duration: float,
    num_channels: int,
    quality: Resample.Quality,
    plugin_class,
):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_channels=num_channels)
    plugin = plugin_class(target_sample_rate, quality=quality)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output, atol=TOLERANCE_PER_QUALITY[quality])


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [1234.56, 8000, 11025, 48000])
@pytest.mark.parametrize("target_sample_rate", [1234.56, 8000, 11025, 48000])
@pytest.mark.parametrize("duration", DURATIONS)
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_resample_invariant_to_buffer_size(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    duration: float,
    num_channels: int,
    quality: Resample.Quality,
    plugin_class,
):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_channels=num_channels)
    plugin = plugin_class(target_sample_rate, quality=quality)

    buffer_sizes = [1, 7000, 8192, 1_000_000]
    outputs = [
        plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in buffer_sizes
    ]

    for a, b in zip(outputs, outputs[1:]):
        np.testing.assert_allclose(a, b)


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate_multiple", [1, 2, 3, 4, 20])
@pytest.mark.parametrize("sample_rate", [8000, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 1_000_000])
@pytest.mark.parametrize("duration", DURATIONS)
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_identical_noise_with_zero_order_hold(
    fundamental_hz: float,
    sample_rate_multiple: float,
    sample_rate: float,
    buffer_size: int,
    duration: float,
    num_channels: int,
    plugin_class,
):
    noise = np.random.rand(int(duration * sample_rate))
    if num_channels == 2:
        noise = np.stack([noise, noise])

    plugin = plugin_class(
        sample_rate * sample_rate_multiple, quality=Resample.Quality.ZeroOrderHold
    )
    output = plugin.process(noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(noise, output)


@pytest.mark.parametrize("fundamental_hz", [10])
@pytest.mark.parametrize("sample_rate", [100, 384_123.45])
@pytest.mark.parametrize("target_sample_rate", [100, 384_123.45])
@pytest.mark.parametrize("buffer_size", [1, 1_000_000])
@pytest.mark.parametrize("duration", DURATIONS)
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_extreme_resampling(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    buffer_size: int,
    duration: float,
    num_channels: int,
    quality: Resample.Quality,
    plugin_class,
):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_channels=num_channels)
    plugin = plugin_class(target_sample_rate, quality=quality)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output, atol=TOLERANCE_PER_QUALITY[quality])


@pytest.mark.parametrize("num_channels", [1, 2])
def test_quality_can_change(
    num_channels: int,
    fundamental_hz: float = 440,
    sample_rate: float = 8000,
    buffer_size: int = 96000,
    duration: float = DURATIONS[0],
):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_channels=num_channels)

    plugin = Resample(sample_rate)
    output1 = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output1, atol=TOLERANCE_PER_QUALITY[plugin.quality])
    original_quality = plugin.quality

    plugin.quality = Resample.Quality.ZeroOrderHold
    output2 = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output2, atol=TOLERANCE_PER_QUALITY[plugin.quality])

    plugin.quality = original_quality
    output3 = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output1, output3)
