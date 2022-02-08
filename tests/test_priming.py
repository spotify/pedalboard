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
from pedalboard_native._internal import PrimeWithSilenceTestPlugin


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [16, 40, 128, 160, 8192, 8193])
@pytest.mark.parametrize("silent_samples_to_add", [1, 16, 40, 128, 160, 8192, 8193])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_prime_with_silence(sample_rate, buffer_size, silent_samples_to_add, num_channels):
    num_seconds = 5.0
    noise = np.random.rand(int(num_seconds * sample_rate))
    if num_channels == 2:
        noise = np.stack([noise, noise])
    plugin = PrimeWithSilenceTestPlugin(silent_samples_to_add)
    output = plugin.process(noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, noise)
