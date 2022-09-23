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
from pedalboard import Clipping
from .utils import generate_sine_at, db_to_gain


@pytest.mark.parametrize("threshold_db", list(np.arange(0.0, -40, -0.5)))
@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_bitcrush(
    threshold_db: float, fundamental_hz: float, sample_rate: float, num_channels: int
):
    sine_wave = generate_sine_at(
        sample_rate, fundamental_hz, num_seconds=0.1, num_channels=num_channels
    )

    plugin = Clipping(threshold_db)
    output = plugin.process(sine_wave, sample_rate)

    assert np.all(np.isfinite(output))
    _min, _max = -db_to_gain(threshold_db), db_to_gain(threshold_db)

    expected_output = np.clip(sine_wave, _min, _max)
    np.testing.assert_allclose(output, expected_output, atol=0.01)
