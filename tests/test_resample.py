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
from pedalboard import Pedalboard, Resample


@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [22050, 44100, 48000, 8000])
@pytest.mark.parametrize("buffer_size", [1, 32, 128, 8192, 96000])
@pytest.mark.parametrize("duration", [0.5, 2.0, 3.14159])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_resample(fundamental_hz, sample_rate, target_sample_rate, buffer_size, duration, num_channels):
    samples = np.arange(duration * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    plugin = Resample(target_sample_rate)
    output = plugin.process(sine_wave, sample_rate, buffer_size=buffer_size)

    try:
        np.testing.assert_allclose(sine_wave, output, atol=0.15)
    except AssertionError:
        import matplotlib.pyplot as plt
        
        for cut in (buffer_size * 2, len(sine_wave) // 200, len(sine_wave)):
            fig, ax = plt.subplots(3)
            ax[0].plot(sine_wave[:cut])
            ax[0].set_title("Input")
            ax[1].plot(output[:cut])
            ax[1].set_title("Output")
            ax[2].plot(np.abs(sine_wave - output)[:cut])
            ax[2].set_title("Diff")
            ax[2].set_ylim(0, 1)
            fig.suptitle(f"fundamental_hz={fundamental_hz}, sample_rate={sample_rate}, target_sample_rate={target_sample_rate}")
            plt.savefig(f"{fundamental_hz}-{sample_rate}-{target_sample_rate}-{buffer_size}-{cut}.png", dpi=300)
            plt.clf()

        import soundfile as sf
        sf.write(f"{fundamental_hz}-{sample_rate}-{target_sample_rate}-{buffer_size}.wav", output, sample_rate)        
        raise
