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

# For the range of VBR quality levels supported by LAME,
# all output samples will match all input samples within
# this range. Not bad for a 25-year-old codec!
MP3_ABSOLUTE_TOLERANCE = 0.25


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor(vbr_quality: float, sample_rate: int, num_channels: int):
    num_seconds = 10.0
    fundamental_hz = 440
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    compressed = MP3Compressor(vbr_quality)(sine_wave, sample_rate)
    np.testing.assert_allclose(sine_wave, compressed, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_invariant_to_buffer_size(vbr_quality: float, sample_rate: int, num_channels: int):
    num_seconds = 10.0
    fundamental_hz = 440
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    if num_channels == 2:
        sine_wave = np.stack([sine_wave, sine_wave])

    compressed_different_buffer_sizes = [
        MP3Compressor(vbr_quality)(sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in (32, 128, 1024, 1152, 8192, 65536)
    ]

    # These should all be the same, so any two should be the same:
    for a, b in zip(compressed_different_buffer_sizes, compressed_different_buffer_sizes[:1]):
        np.testing.assert_allclose(a, b, atol=MP3_ABSOLUTE_TOLERANCE)
