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

from pedalboard_native._internal import MonoToMultiTestPlugin  # type: ignore

NUM_SECONDS = 1.0


@pytest.mark.parametrize("sample_rate", [22050, 44100])
@pytest.mark.parametrize("buffer_size", [1, 16, 128, 8192])
def test_mono_to_multi_stereo(sample_rate, buffer_size):
    """Each stereo channel should be processed independently."""
    stereo_noise = np.stack(
        [
            np.random.rand(int(NUM_SECONDS * sample_rate)),
            np.random.rand(int(NUM_SECONDS * sample_rate)),
        ]
    ).astype(np.float32)
    output = MonoToMultiTestPlugin().process(
        stereo_noise, sample_rate, buffer_size=buffer_size
    )
    assert output.shape == stereo_noise.shape
    assert not np.allclose(output[0], output[1]), (
        "Channels should differ when inputs differ"
    )


@pytest.mark.parametrize("sample_rate", [22050, 44100])
@pytest.mark.parametrize("buffer_size", [1, 16, 128, 8192])
def test_mono_to_multi_on_mono(sample_rate, buffer_size):
    """Mono input should pass through normally."""
    mono_noise = np.random.rand(int(NUM_SECONDS * sample_rate)).astype(np.float32)
    output = MonoToMultiTestPlugin().process(
        mono_noise, sample_rate, buffer_size=buffer_size
    )
    assert output.shape == mono_noise.shape


@pytest.mark.parametrize("sample_rate", [22050, 44100])
def test_mono_to_multi_identical_channels_produce_identical_output(sample_rate):
    """If both channels have the same input, both outputs should match."""
    mono = np.random.rand(int(NUM_SECONDS * sample_rate)).astype(np.float32)
    stereo = np.stack([mono, mono])
    output = MonoToMultiTestPlugin().process(stereo, sample_rate)
    np.testing.assert_allclose(output[0], output[1])
