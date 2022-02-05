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


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 22050, 44100, 48000, 1234.56])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 96000])
@pytest.mark.parametrize("duration", [0.5, 1.23456])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_resample(
    fundamental_hz, sample_rate, target_sample_rate, buffer_size, duration, num_channels
):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = int(sample_rate * 0.1)
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    if num_channels == 2:
        np.testing.assert_allclose(sine_wave[0], sine_wave[1])

    plugin = Resample(target_sample_rate)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
