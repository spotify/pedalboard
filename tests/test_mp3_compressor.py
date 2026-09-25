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


# CBR Mode Tests
@pytest.mark.parametrize("bitrate", [32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_cbr(bitrate: int, sample_rate: int, num_channels: int):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)
    compressor = MP3Compressor()
    compressor.bitrate = bitrate
    compressed = compressor(sine_wave, sample_rate)
    np.testing.assert_allclose(sine_wave, compressed, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("bitrate", [128, 192, 320])
@pytest.mark.parametrize("sample_rate", [44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_cbr_invariant_to_buffer_size(
    bitrate: int, sample_rate: int, num_channels: int
):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)

    compressor = MP3Compressor()
    compressor.bitrate = bitrate
    compressed_different_buffer_sizes = [
        compressor(sine_wave, sample_rate, buffer_size=buffer_size)
        for buffer_size in (1, 128, 1152, 65536)
    ]

    # These should all be the same, so any two should be the same:
    for a, b in zip(compressed_different_buffer_sizes, compressed_different_buffer_sizes[:1]):
        np.testing.assert_allclose(a, b, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("invalid_bitrate", [100, 150, 250, 400, 50])
def test_mp3_compressor_cbr_invalid_bitrate(invalid_bitrate: int):
    compressor = MP3Compressor()
    with pytest.raises(ValueError, match="Invalid CBR bitrate"):
        compressor.bitrate = invalid_bitrate


def test_mp3_compressor_mode_switching():
    """Test switching between VBR and CBR modes via property setters."""
    from pedalboard import MP3CompressorMode

    # Start with VBR
    compressor = MP3Compressor(vbr_quality=3.0)
    assert compressor.mode == MP3CompressorMode.VBR
    assert compressor.vbr_quality == 3.0

    # Switch to CBR
    compressor.bitrate = 192
    assert compressor.mode == MP3CompressorMode.CBR
    assert compressor.bitrate == 192

    # Switch back to VBR
    compressor.vbr_quality = 1.0
    assert compressor.mode == MP3CompressorMode.VBR
    assert compressor.vbr_quality == 1.0

    # Test multiple switches
    compressor.bitrate = 256
    assert compressor.mode == MP3CompressorMode.CBR
    compressor.bitrate = 128
    assert compressor.mode == MP3CompressorMode.CBR
    assert compressor.bitrate == 128


def test_mp3_compressor_cbr_constructor():
    """Test CBR constructor by setting bitrate after creation."""
    compressor = MP3Compressor(vbr_quality=2.0)
    compressor.bitrate = 128
    assert compressor.mode == MP3CompressorMode.CBR
    assert compressor.bitrate == 128


def test_mp3_compressor_repr():
    """Test string representation for both modes."""
    # VBR mode
    vbr_compressor = MP3Compressor(vbr_quality=2.5)
    repr_str = repr(vbr_compressor)
    assert "vbr_quality=2.5" in repr_str
    assert "MP3Compressor" in repr_str

    # CBR mode
    cbr_compressor = MP3Compressor()
    cbr_compressor.bitrate = 192
    repr_str = repr(cbr_compressor)
    assert "bitrate=192" in repr_str
    assert "MP3Compressor" in repr_str


@pytest.mark.parametrize("bitrate", [128, 256])
@pytest.mark.parametrize(
    "sample_rate", [48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000]
)
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_cbr_arbitrary_sample_rate(
    bitrate: int, sample_rate: int, num_channels: int
):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)
    compressor = MP3Compressor()
    compressor.bitrate = bitrate
    compressed = compressor(sine_wave, sample_rate)
    np.testing.assert_allclose(sine_wave, compressed, atol=MP3_ABSOLUTE_TOLERANCE)


@pytest.mark.parametrize("sample_rate", [96000, 6000, 44101])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_compressor_cbr_fails_on_invalid_sample_rate(sample_rate: int, num_channels: int):
    sine_wave = generate_sine_at(sample_rate, num_channels=num_channels)
    compressor = MP3Compressor()
    compressor.bitrate = 128

    with pytest.raises(ValueError):
        compressor(sine_wave, sample_rate)


def test_mp3_compressor_default_mode():
    """Test that default constructor creates VBR mode."""
    compressor = MP3Compressor()
    assert compressor.mode == MP3CompressorMode.VBR
    assert compressor.vbr_quality == 2.0


@pytest.mark.parametrize("bitrate", [32, 320])  # Test extreme valid bitrates
def test_mp3_compressor_cbr_extreme_bitrates(bitrate: int):
    """Test CBR mode with extreme but valid bitrates."""
    compressor = MP3Compressor()
    compressor.bitrate = bitrate
    assert compressor.mode == MP3CompressorMode.CBR
    assert compressor.bitrate == bitrate
