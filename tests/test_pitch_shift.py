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


import numpy as np
import pytest

from pedalboard import Pedalboard, PitchShift


@pytest.mark.parametrize("semitones", [-12, 0, 12])
@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_pitch_shift(semitones, fundamental_hz, sample_rate):
    num_seconds = 1.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    plugin = PitchShift(semitones)
    output = plugin.process(sine_wave, sample_rate)

    assert np.all(np.isfinite(output))


@pytest.mark.parametrize("semitones", [-73, 73])
def test_pitch_shift_extremes_throws_errors(semitones):
    with pytest.raises(ValueError):
        PitchShift(semitones)


@pytest.mark.parametrize("semitones", [-72, 72])
@pytest.mark.parametrize("sample_rate", [48000])
@pytest.mark.parametrize("duration", [0.25])
def test_pitch_shift_extremes(semitones, sample_rate, duration):
    noise = np.random.rand(int(duration * sample_rate)).astype(np.float32)
    output = PitchShift(semitones).process(noise, sample_rate)
    assert np.all(np.isfinite(output))


@pytest.mark.parametrize("fundamental_hz", [440.0, 880.0])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [32, 512, 513, 1024, 1029, 2048, 8192])
def test_pitch_shift_latency_compensation(fundamental_hz, sample_rate, buffer_size):
    num_seconds = 2.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    plugin = Pedalboard([PitchShift(0)])
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(sine_wave, output, atol=1e-6)
