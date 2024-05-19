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

from pedalboard import MP3Compressor
from tests.utils import generate_sine_at

# For the range of VBR quality levels supported by LAME,
# all output samples will match all input samples within
# this range. Not bad for a 25-year-old codec!
MP3_ABSOLUTE_TOLERANCE = 0.25


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor(vbr_quality: float, sample_rate: int, num_channels: int):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)
    compressed = MP3Compressor(vbr_quality)(sine_wave, sample_rate)
    np.testing.assert_allclose(sine_wave, compressed, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("vbr_quality", [0.0, 0.5, 1, 9, 9.5])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_invariant_to_buffer_size(
    vbr_quality: float, sample_rate: int, num_channels: int
):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)

    compressor = MP3Compressor(vbr_quality)
    compressed_different_buffer_sizes = [
        compressor(sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in (1, 128, 1152, 65536)
    ]

    # These should all be the same, so any two should be the same:
    for a, b in zip(compressed_different_buffer_sizes, compressed_different_buffer_sizes[:1]):
        np.testing.assert_allclose(a, b, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("vbr_quality", [2])
@pytest.mark.parametrize(
    "sample_rate", [48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000]
)
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_arbitrary_sample_rate(
    vbr_quality: float, sample_rate: int, num_channels: int
):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)
    compressed = MP3Compressor(vbr_quality)(sine_wave, sample_rate)
    np.testing.assert_allclose(sine_wave, compressed, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("sample_rate", [96000, 6000, 44101])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_fails_on_invalid_sample_rate(sample_rate: int, num_channels: int):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)

    with pytest.raises(ValueError):
        MP3Compressor(1)(sine_wave, sample_rate)
