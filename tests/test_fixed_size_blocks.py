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


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [1, 16, 40, 128, 160, 8192, 8193])
@pytest.mark.parametrize("fixed_buffer_size", [1, 16, 40, 128, 160, 8192, 8193])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_fixed_size_blocks_plugin(sample_rate, buffer_size, fixed_buffer_size, num_channels):
    num_seconds = 5.0
    noise = np.random.rand(int(num_seconds * sample_rate))
    if num_channels == 2:
        noise = np.stack([noise, noise])
    plugin = FixedSizeBlockTestPlugin(fixed_buffer_size)
    output = plugin.process(noise, sample_rate, buffer_size=buffer_size)
    try:
        np.testing.assert_allclose(noise, output)
    except AssertionError:
        import matplotlib.pyplot as plt

        if num_channels == 2:
            noise = noise[0]
            output = output[0]

        for cut in (buffer_size * 2, len(noise) // 200, len(noise)):
            fig, ax = plt.subplots(3)
            ax[0].plot(noise[:cut])
            ax[0].set_title("Input")
            ax[1].plot(output[:cut])
            ax[1].set_title("Output")
            ax[2].plot(np.abs(noise - output)[:cut])
            ax[2].set_title("Diff")
            ax[2].set_ylim(0, 1)
            plt.savefig(f"{sample_rate}-{buffer_size}-{cut}.png", dpi=300)
            plt.clf()

        raise
