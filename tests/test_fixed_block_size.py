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
from pedalboard_native._internal import FixedSizeBlockTestPlugin
from .utils import generate_sine_at


@pytest.mark.parametrize("sample_rate", [22050, 44100])
@pytest.mark.parametrize("buffer_size", [1, 64, 65, 128, 8192, 8193])
@pytest.mark.parametrize("fixed_buffer_size", [1, 64, 65, 128, 8192, 8193])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_fixed_size_blocks_plugin(sample_rate, buffer_size, fixed_buffer_size, num_channels):
    signal = generate_sine_at(sample_rate, num_seconds=1.0, num_channels=num_channels)

    plugin = FixedSizeBlockTestPlugin(fixed_buffer_size)
    output = plugin.process(signal, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(signal, output)
