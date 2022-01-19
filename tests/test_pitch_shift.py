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
from pedalboard import PitchShift


@pytest.mark.parametrize("scale", [0.5, 1.0, 2.0])
@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_pitch_shift(scale, fundamental_hz, sample_rate):
    num_seconds = 10.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    plugin = PitchShift(scale)
    output = plugin.process(sine_wave, sample_rate)

    assert np.all(np.isfinite(output))
