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


import numpy as np
import pytest

from pedalboard_native._internal import PrimeWithSilenceTestPlugin

NUM_SECONDS = 1
MAX_SAMPLE_RATE = 48000
NOISE = np.random.rand(int(NUM_SECONDS * MAX_SAMPLE_RATE)).astype(np.float32)


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [16, 40, 128, 160, 8192, 8193])
@pytest.mark.parametrize("silent_samples_to_add", [1, 16, 40, 128, 160, 8192, 8193])
def test_prime_with_silence(sample_rate, buffer_size, silent_samples_to_add):
    noise = NOISE[: int(sample_rate * NUM_SECONDS)]
    plugin = PrimeWithSilenceTestPlugin(silent_samples_to_add)
    output = plugin.process(noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, noise)
