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

from pedalboard_native._internal import AddLatency

MAX_SAMPLE_RATE = 96000
NUM_SECONDS = 3.0
NOISE = np.random.rand(int(NUM_SECONDS * MAX_SAMPLE_RATE)).astype(np.float32)


@pytest.mark.parametrize("sample_rate", [22050, 44100, MAX_SAMPLE_RATE])
@pytest.mark.parametrize("buffer_size", [128, 8192, 65536])
@pytest.mark.parametrize("latency_seconds", [0.25, 1, 2])
def test_latency_compensation(sample_rate, buffer_size, latency_seconds):
    noise = NOISE[: int(sample_rate * NUM_SECONDS)]
    plugin = AddLatency(int(latency_seconds * sample_rate))
    output = plugin.process(noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, noise)
