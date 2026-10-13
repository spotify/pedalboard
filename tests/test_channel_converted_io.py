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

from io import BytesIO

import numpy as np
import pytest

from pedalboard.io import AudioFile, ChannelConvertedReadableAudioFile

from .utils import generate_sine_at


def test_mono_constructor():
    """Test that .mono() returns a ChannelConvertedReadableAudioFile."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.mono() as m:
            assert isinstance(m, ChannelConvertedReadableAudioFile)
            assert m.num_channels == 1
        assert m.closed
        assert not f.closed
    assert f.closed


def test_stereo_constructor():
    """Test that .stereo() returns a ChannelConvertedReadableAudioFile."""
    mono = np.random.rand(1, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.stereo() as s:
            assert isinstance(s, ChannelConvertedReadableAudioFile)
            assert s.num_channels == 2
        assert s.closed
        assert not f.closed
    assert f.closed


def test_with_channels_constructor():
    """Test that .with_channels() returns a ChannelConvertedReadableAudioFile."""
    mono = np.random.rand(1, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.with_channels(4) as c:
            assert isinstance(c, ChannelConvertedReadableAudioFile)
            assert c.num_channels == 4
        assert c.closed
        assert not f.closed
    assert f.closed


def test_mono_does_nothing_if_already_mono():
    """Test that .mono() returns self if already mono."""
    mono = np.random.rand(1, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.mono() as m:
            assert m is f


def test_stereo_does_nothing_if_already_stereo():
    """Test that .stereo() returns self if already stereo."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.stereo() as s:
            assert s is f


def test_with_channels_does_nothing_if_same():
    """Test that .with_channels(n) returns self if already n channels."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())) as f:
        with f.with_channels(2) as c:
            assert c is f


def test_read_zero_raises():
    """Test that read() without arguments raises ValueError."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        with pytest.raises(ValueError):
            f.read()


@pytest.mark.parametrize("source_channels", [2, 4, 6, 8])
def test_stereo_to_mono_averages_channels(source_channels: int):
    """Test that converting to mono averages all channels."""
    # Create audio where each channel has a different constant value
    num_samples = 44100
    audio = np.zeros((source_channels, num_samples), dtype=np.float32)
    for c in range(source_channels):
        audio[c, :] = float(c + 1) / source_channels  # 0.5, 1.0 for stereo

    expected_mono_value = np.mean([float(c + 1) / source_channels for c in range(source_channels)])

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, source_channels, bit_depth=32) as f:
        f.write(audio)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        mono = f.read(f.frames)
        assert mono.shape == (1, num_samples)
        np.testing.assert_allclose(mono[0, 1000], expected_mono_value, rtol=1e-5)


def test_mono_to_stereo_duplicates():
    """Test that converting mono to stereo duplicates the channel."""
    mono_value = 0.5
    mono = np.full((1, 44100), mono_value, dtype=np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with AudioFile(BytesIO(buf.getvalue())).stereo() as f:
        stereo = f.read(f.frames)
        assert stereo.shape == (2, 44100)
        np.testing.assert_allclose(stereo[0, :], mono_value, rtol=1e-5)
        np.testing.assert_allclose(stereo[1, :], mono_value, rtol=1e-5)


@pytest.mark.parametrize("target_channels", [2, 4, 6, 8])
def test_mono_to_multichannel_duplicates(target_channels: int):
    """Test that converting mono to multiple channels duplicates to all."""
    mono_value = 0.75
    mono = np.full((1, 44100), mono_value, dtype=np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with AudioFile(BytesIO(buf.getvalue())).with_channels(target_channels) as f:
        audio = f.read(f.frames)
        assert audio.shape == (target_channels, 44100)
        for c in range(target_channels):
            np.testing.assert_allclose(audio[c, :], mono_value, rtol=1e-5)


def test_stereo_to_multichannel_raises():
    """Test that converting stereo to multichannel raises an error."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with pytest.raises(ValueError, match="not supported"):
        AudioFile(BytesIO(buf.getvalue())).with_channels(6)


def test_multichannel_to_stereo_raises():
    """Test that converting multichannel to stereo raises an error."""
    surround = np.random.rand(6, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 6, bit_depth=32) as f:
        f.write(surround)

    with pytest.raises(ValueError, match="not supported"):
        AudioFile(BytesIO(buf.getvalue())).stereo()


def test_invalid_channel_count_raises():
    """Test that requesting 0 or negative channels raises an error."""
    mono = np.random.rand(1, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 1, bit_depth=32) as f:
        f.write(mono)

    with pytest.raises(ValueError):
        AudioFile(BytesIO(buf.getvalue())).with_channels(0)

    with pytest.raises(ValueError):
        AudioFile(BytesIO(buf.getvalue())).with_channels(-1)


@pytest.mark.parametrize("chunk_size", [100, 1000, 10000])
def test_tell_after_read(chunk_size: int):
    """Test that tell() returns correct position after reads."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        for i in range(0, f.frames, chunk_size):
            assert f.tell() == i
            if f.read(chunk_size).shape[-1] < chunk_size:
                break


@pytest.mark.parametrize("offset", [0, 100, 1000, 22050])
def test_seek(offset: int):
    """Test that seek() works correctly."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        # Read from start to offset position
        if offset > 0:
            f.read(offset)
        expected = f.read(1000)

        # Seek back and verify
        f.seek(offset)
        assert f.tell() == offset
        actual = f.read(1000)

        np.testing.assert_allclose(expected, actual)


def test_properties_accessible():
    """Test that all standard properties are accessible."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        assert f.samplerate == 44100
        assert f.num_channels == 1
        assert f.frames == 44100
        assert f.duration == 1.0
        assert f.exact_duration_known is True
        assert f.file_dtype == "float32"
        assert f.closed is False
        assert f.seekable() is True

    assert f.closed is True


def test_repr():
    """Test that __repr__ returns expected format."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        repr_str = repr(f)
        assert "ChannelConvertedReadableAudioFile" in repr_str
        assert "samplerate=44100" in repr_str
        assert "num_channels=1" in repr_str


# Chaining tests


def test_chain_resampled_to_then_mono():
    """Test chaining .resampled_to().mono()."""
    stereo = generate_sine_at(44100, 440, num_seconds=1, num_channels=2).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).resampled_to(22050).mono() as f:
        assert f.samplerate == 22050
        assert f.num_channels == 1
        audio = f.read(f.frames)
        assert audio.shape[0] == 1
        # Allow for some variation due to resampling
        assert abs(audio.shape[1] - 22050) <= 10


def test_chain_mono_then_resampled_to():
    """Test chaining .mono().resampled_to()."""
    stereo = generate_sine_at(44100, 440, num_seconds=1, num_channels=2).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono().resampled_to(22050) as f:
        assert f.samplerate == 22050
        assert f.num_channels == 1
        audio = f.read(f.frames)
        assert audio.shape[0] == 1
        # Allow for some variation due to resampling
        assert abs(audio.shape[1] - 22050) <= 10


def test_chain_mono_resampled_stereo():
    """Test triple chaining: .mono().resampled_to().stereo()."""
    stereo = generate_sine_at(44100, 440, num_seconds=1, num_channels=2).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    with AudioFile(BytesIO(buf.getvalue())).mono().resampled_to(22050).stereo() as f:
        assert f.samplerate == 22050
        assert f.num_channels == 2
        audio = f.read(f.frames)
        assert audio.shape[0] == 2
        # Both channels should be identical (duplicated from mono)
        np.testing.assert_allclose(audio[0], audio[1])


def test_multichannel_to_stereo_via_mono():
    """Test the recommended workaround: multichannel -> mono -> stereo."""
    surround = np.random.rand(6, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 6, bit_depth=32) as f:
        f.write(surround)

    # Direct conversion should fail
    with pytest.raises(ValueError):
        AudioFile(BytesIO(buf.getvalue())).stereo()

    # But via mono should work
    with AudioFile(BytesIO(buf.getvalue())).mono().stereo() as f:
        assert f.num_channels == 2
        audio = f.read(f.frames)
        assert audio.shape == (2, 44100)


@pytest.mark.parametrize("chunk_size", [100, 1000, 10000, 44100])
def test_read_in_chunks(chunk_size: int):
    """Test reading in chunks produces same result as reading all at once."""
    stereo = np.random.rand(2, 44100).astype(np.float32)

    buf = BytesIO()
    buf.name = "test.wav"
    with AudioFile(buf, "w", 44100, 2, bit_depth=32) as f:
        f.write(stereo)

    # Read all at once
    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        all_at_once = f.read(f.frames)

    # Read in chunks
    with AudioFile(BytesIO(buf.getvalue())).mono() as f:
        chunks = []
        while f.tell() < f.frames:
            chunk = f.read(chunk_size)
            chunks.append(chunk)
        in_chunks = np.concatenate(chunks, axis=1)

    np.testing.assert_allclose(all_at_once, in_chunks)
