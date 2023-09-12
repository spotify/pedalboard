#! /usr/bin/env python
#
# Copyright 2023 Spotify AB
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
from pedalboard import time_stretch


@pytest.mark.parametrize("semitones", [-1, 0, 1])
@pytest.mark.parametrize("stretch_factor", [0.75, 1, 1.25])
@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_time_stretch(semitones, stretch_factor, fundamental_hz, sample_rate):
    num_seconds = 1.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate).astype(np.float32)

    output = time_stretch(
        sine_wave, sample_rate, stretch_factor=stretch_factor, pitch_shift_in_semitones=semitones
    )

    assert np.all(np.isfinite(output))
    assert output.shape[1] == int((num_seconds * sample_rate) / stretch_factor)
    if stretch_factor != 1 or semitones != 0:
        min_samples = min(output.shape[1], sine_wave.shape[0])
        assert not np.allclose(output[:, :min_samples], sine_wave[:min_samples])
