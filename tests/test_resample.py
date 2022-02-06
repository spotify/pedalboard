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
from pedalboard import Pedalboard, Resample
from pedalboard_native._internal import ResampleWithLatency


TOLERANCE_PER_QUALITY = {
    Resample.Quality.ZeroOrderHold: 0.65,
    Resample.Quality.Linear: 0.35,
    Resample.Quality.CatmullRom: 0.15,
    Resample.Quality.Lagrange: 0.14,
    Resample.Quality.WindowedSinc: 0.12,
}


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 22050, 44100, 48000, 1234.56])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 96000])
@pytest.mark.parametrize("duration", [0.5, 1.23456])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_resample(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    buffer_size: int,
    duration: float,
    num_channels: int,
    plugin_class,
):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = int(sample_rate * 0.1)
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = plugin_class(target_sample_rate)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)

    np.testing.assert_allclose(sine_wave, output, atol=0.16)


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 22050, 44100, 48000, 1234.56])
@pytest.mark.parametrize("duration", [0.5, 1.23456])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_resample_invariant_to_buffer_size(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    duration: float,
    num_channels: int,
    plugin_class,
):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = int(sample_rate * 0.1)
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = plugin_class(target_sample_rate)

    buffer_sizes = [1, 32, 128, 8192, 96000]
    outputs = [
        plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in buffer_sizes
    ]

    np.testing.assert_allclose(a, b, atol=1e-3)


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate_multiple", [1, 2, 3, 4, 5])
@pytest.mark.parametrize("sample_rate", [8000, 22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 96000])
@pytest.mark.parametrize("duration", [0.5])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_identical_with_zero_order_hold(
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
    np.testing.assert_allclose(noise, output, atol=1e-9)


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate_multiple", [1 / 2, 1, 2])
@pytest.mark.parametrize("sample_rate", [8000, 22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 96000])
@pytest.mark.parametrize("duration", [0.5])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize(
    "quality",
    [
        Resample.Quality.ZeroOrderHold,
        Resample.Quality.Linear,
        Resample.Quality.Lagrange,
        Resample.Quality.CatmullRom,
        Resample.Quality.WindowedSinc,
    ],
)
@pytest.mark.parametrize("plugin_class", [Resample, ResampleWithLatency])
def test_all_quality_levels(
    fundamental_hz: float,
    sample_rate_multiple: float,
    sample_rate: float,
    buffer_size: int,
    duration: float,
    num_channels: int,
    quality: Resample.Quality,
    plugin_class,
):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = plugin_class(sample_rate * sample_rate_multiple, quality=quality)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output, atol=0.35)


@pytest.mark.parametrize("fundamental_hz", [10])
@pytest.mark.parametrize("sample_rate", [100, 384_123.45])
@pytest.mark.parametrize("target_sample_rate", [100, 384_123.45])
@pytest.mark.parametrize("buffer_size", [1, 1_000_000])
@pytest.mark.parametrize("duration", [1.0])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize(
    "quality",
    [
        Resample.Quality.ZeroOrderHold,
        Resample.Quality.Linear,
        Resample.Quality.Lagrange,
        Resample.Quality.CatmullRom,
        Resample.Quality.WindowedSinc,
    ],
)
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
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = int(sample_rate * 0.1)
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = plugin_class(target_sample_rate, quality=quality)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)

    np.testing.assert_allclose(sine_wave, output, atol=TOLERANCE_PER_QUALITY[quality])


@pytest.mark.parametrize("num_channels", [1, 2])
def test_quality_can_change(
    num_channels: int,
    fundamental_hz: float = 440,
    sample_rate: float = 8000,
    buffer_size: int = 96000,
    duration: float = 0.5,
):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = Resample(sample_rate)
    output1 = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output1, atol=0.35)

    plugin.quality = Resample.Quality.ZeroOrderHold
    output2 = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output2, atol=0.75)
