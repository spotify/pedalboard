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
from pedalboard import MP3Compressor


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
def test_mp3_compressor(vbr_quality: float, sample_rate: int):
    num_seconds = 10.0
    fundamental_hz = 440
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    stereo_sine_wave = np.stack([sine_wave, sine_wave])

    compressed = MP3Compressor(vbr_quality)(stereo_sine_wave, sample_rate)
    np.testing.assert_allclose(stereo_sine_wave, compressed, atol=0.25)


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
def test_mp3_compressor_invariant_to_buffer_size(vbr_quality: float, sample_rate: int):
    num_seconds = 10.0
    fundamental_hz = 440
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    stereo_sine_wave = np.stack([sine_wave, sine_wave])

    compressed_different_buffer_sizes = [
        MP3Compressor(vbr_quality)(stereo_sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in (32, 128, 1024, 1152, 8192, 65536)
    ]

    # These should all be the same, so any two should be the same:
    for a, b in zip(compressed_different_buffer_sizes, compressed_different_buffer_sizes[:1]):
        np.testing.assert_allclose(a, b, atol=0.25)
