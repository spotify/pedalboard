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
from pedalboard import GSMCompressor, Resample

# GSM is a _very_ lossy codec:
GSM_ABSOLUTE_TOLERANCE = 0.75

# Passing in a full-scale sine wave seems to often make GSM clip:
SINE_WAVE_VOLUME = 0.9


def generate_sine_at(
    sample_rate: float,
    fundamental_hz: float = 440.0,
    num_seconds: float = 3.0,
    num_channels: int = 1,
) -> np.ndarray:
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = int(sample_rate * 0.1)
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels == 2:
        return np.stack([sine_wave, sine_wave])
    return sine_wave


@pytest.mark.parametrize("fundamental_hz", [440.0])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 32000, 32001, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 32, 160, 8192])
@pytest.mark.parametrize("duration", [1.0])
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
@pytest.mark.parametrize("num_channels", [1, 2])
def test_gsm_compressor(
    fundamental_hz: float,
    sample_rate: float,
    buffer_size: int,
    duration: float,
    quality: Resample.Quality,
    num_channels: int,
):
    signal = (
        generate_sine_at(sample_rate, fundamental_hz, duration, num_channels) * SINE_WAVE_VOLUME
    )
    output = GSMCompressor(quality=quality)(signal, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(signal, output, atol=GSM_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 32000, 32001, 44100, 48000])
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
@pytest.mark.parametrize("num_channels", [1, 2])
def test_gsm_compressor_invariant_to_buffer_size(
    sample_rate: float,
    quality: Resample.Quality,
    num_channels: int,
):
    fundamental_hz = 400.0
    duration = 3.0
    signal = generate_sine_at(sample_rate, fundamental_hz, duration, num_channels)

    compressed = [
        GSMCompressor(quality=quality)(signal, sample_rate, buffer_size=buffer_size)
        for buffer_size in (1, 32, 8192)
    ]
    for a, b in zip(compressed, compressed[1:]):
        np.testing.assert_allclose(a, b)
