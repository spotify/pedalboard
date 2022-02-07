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
from pedalboard_native._internal import ForceMonoTestPlugin


NUM_SECONDS = 1.0


@pytest.mark.parametrize("sample_rate", [22050, 44100])
@pytest.mark.parametrize("buffer_size", [1, 16, 128, 8192])
def test_force_mono(sample_rate, buffer_size):
    stereo_noise = np.stack(
        [
            np.random.rand(int(NUM_SECONDS * sample_rate)),
            np.random.rand(int(NUM_SECONDS * sample_rate)),
        ]
    )
    output = ForceMonoTestPlugin().process(stereo_noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output[0], output[1])

    expected_mono = (stereo_noise[0] + stereo_noise[1]) / 2
    np.testing.assert_allclose(output, np.stack([expected_mono, expected_mono]), atol=1e-7)


@pytest.mark.parametrize("sample_rate", [22050, 44100])
@pytest.mark.parametrize("buffer_size", [1, 16, 128, 8192])
def test_force_mono_on_already_mono(sample_rate, buffer_size):
    mono_noise = np.random.rand(int(NUM_SECONDS * sample_rate))
    output = ForceMonoTestPlugin().process(mono_noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, mono_noise)
