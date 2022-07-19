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


import io
import os
import glob
import wave
import pathlib
import shutil
import pytest
import platform
import pedalboard
from typing import Optional

import numpy as np

from .utils import generate_sine_at

EXPECTED_DURATION_SECONDS = 5
EXPECT_LENGTH_TO_BE_EXACT = {"wav", "aiff", "caf", "ogg", "m4a", "mp4"}

TEST_AUDIO_FILES = {
    44100: glob.glob(os.path.join(os.path.dirname(__file__), "audio", "correct", "*44100*")),
    48000: glob.glob(os.path.join(os.path.dirname(__file__), "audio", "correct", "*48000*")),
}

FILENAMES_AND_SAMPLERATES = [
    (filename, samplerate)
    for samplerate, filenames in TEST_AUDIO_FILES.items()
    for filename in filenames
    # On some platforms, not all extensions will be available.
    if any(filename.endswith(extension) for extension in pedalboard.io.get_supported_read_formats())
]

UNSUPPORTED_FILENAMES = [
    filename
    for filename in sum(TEST_AUDIO_FILES.values(), [])
    if not any(
        filename.endswith(extension) for extension in pedalboard.io.get_supported_read_formats()
    )
]


def get_tolerance_for_format_and_bit_depth(extension: str, input_format, file_dtype: str) -> float:
    if not extension.startswith("."):
        extension = "." + extension
    if extension in {".wav", ".aiff", ".flac"}:
        file_bit_depth = int(file_dtype.replace("float", "").replace("int", ""))
        if np.issubdtype(input_format, np.signedinteger):
            input_bit_depth = np.dtype(input_format).itemsize * 8
            return 4 / (2 ** min(file_bit_depth, input_bit_depth))
        return 4 / (2**file_bit_depth)

    # These formats offset the waveform substantially, and these tests don't do any realignment.
    if extension in {".m4a", ".ac3", ".adts", ".mp4", ".mp2", ".mp3"}:
        return 3.0

    return 0.12


def test_read_constructor_dispatch():
    filename, _samplerate = FILENAMES_AND_SAMPLERATES[0]

    # Support reading a file with just its filename:
    assert isinstance(pedalboard.io.AudioFile(filename), pedalboard.io.ReadableAudioFile)

    # Support reading a file with just its filename and an explicit "r" (read) flag:
    assert isinstance(pedalboard.io.AudioFile(filename, "r"), pedalboard.io.ReadableAudioFile)

    # Support reading a file by using the appropriate subclass constructor just its filename:
    assert isinstance(pedalboard.io.ReadableAudioFile(filename), pedalboard.io.ReadableAudioFile)

    # Don't support reading a file by passing a mode to the
    # subclass constructor (which would be redundant):
    with pytest.raises(TypeError) as e:
        pedalboard.io.ReadableAudioFile(filename, "r")
    assert "incompatible function arguments" in str(e)


def test_write_constructor_dispatch(tmp_path: pathlib.Path):
    filename = str(tmp_path / "temp.wav")

    # Don't support writing to a file with just its filename and write args:
    with pytest.raises(TypeError):
        pedalboard.io.AudioFile(filename, 44100, 1)

    # Support writing to a file with just its filename and an explicit "w" (write) flag:
    assert isinstance(
        pedalboard.io.AudioFile(filename, "w", 44100, 1), pedalboard.io.WriteableAudioFile
    )

    # Support writing to a file by using the appropriate subclass constructor just its filename:
    assert isinstance(
        pedalboard.io.WriteableAudioFile(filename, 44100, 1), pedalboard.io.WriteableAudioFile
    )

    # Don't support writing to a file by passing a mode
    # to the subclass constructor (which would be redundant):
    with pytest.raises(TypeError) as e:
        pedalboard.io.WriteableAudioFile(filename, "w", 44100, 1)
    assert "incompatible function arguments" in str(e)

    # Support writing to a file by omitting num_channels to WriteableAudioFile:
    assert isinstance(
        pedalboard.io.WriteableAudioFile(filename, samplerate=44100),
        pedalboard.io.WriteableAudioFile,
    )

    # but not if samplerate is missing:
    with pytest.raises(TypeError) as e:
        pedalboard.io.WriteableAudioFile(filename, num_channels=1)
    assert "samplerate" in str(e)

    # ... or to regular AudioFile with a "w" flag:
    assert isinstance(
        pedalboard.io.AudioFile(filename, "w", samplerate=44100),
        pedalboard.io.WriteableAudioFile,
    )

    # but not if samplerate is missing:
    with pytest.raises(TypeError) as e:
        pedalboard.io.AudioFile(filename, "w", num_channels=1)
    assert "samplerate" in str(e)


@pytest.mark.parametrize("extension", [".mp3", ".wav", ".ogg", ".flac"])
def test_basic_formats_available_on_all_platforms(extension: str):
    assert extension in pedalboard.io.get_supported_read_formats()


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_basic_read(audio_filename: str, samplerate: float):
    af = pedalboard.io.AudioFile(audio_filename)
    assert af.samplerate == samplerate
    assert af.num_channels == 1
    if any(ext in audio_filename for ext in EXPECT_LENGTH_TO_BE_EXACT):
        assert af.frames == int(samplerate * EXPECTED_DURATION_SECONDS)
    else:
        assert af.frames >= int(samplerate * EXPECTED_DURATION_SECONDS)

    samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
    assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

    # File should no longer be useful:
    af.read(1).nbytes == 0

    # Seeking back to the start of the file should work:
    assert af.seekable()
    af.seek(0)
    samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
    assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

    # Seeking to an arbitrary point should also work
    af.seek(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
    samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
    assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS / 2))

    assert f"samplerate={int(af.samplerate)}" in repr(af)
    assert f"num_channels={af.num_channels}" in repr(af)
    assert f"file_dtype={af.file_dtype}" in repr(af)

    af.seek(0)
    actual = af.read(af.frames)
    expected = generate_sine_at(
        af.samplerate, num_channels=af.num_channels, num_seconds=af.duration
    )
    # Crop the ends of the file, as lossy formats sometimes don't encode the whole file:
    actual = actual[:, : len(expected)]
    tolerance = get_tolerance_for_format_and_bit_depth(
        audio_filename.split(".")[-1], np.int16, af.file_dtype
    )
    np.testing.assert_allclose(np.squeeze(expected), np.squeeze(actual), atol=tolerance)

    af.close()

    with pytest.raises(RuntimeError):
        af.num_channels

    with pytest.raises(RuntimeError):
        af.read(1)


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_read_raw(audio_filename: str, samplerate: float):
    with pedalboard.io.AudioFile(audio_filename) as af:
        num_samples = int(samplerate * EXPECTED_DURATION_SECONDS)
        raw_samples = af.read_raw(num_samples)
        assert raw_samples.shape == (1, num_samples)
        assert af.file_dtype in str(raw_samples.dtype)


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_use_reader_as_context_manager(audio_filename: str, samplerate: float):
    with pedalboard.io.AudioFile(audio_filename) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == 1
        if any(ext in audio_filename for ext in EXPECT_LENGTH_TO_BE_EXACT):
            assert af.frames == int(samplerate * EXPECTED_DURATION_SECONDS)
        else:
            assert af.frames >= int(samplerate * EXPECTED_DURATION_SECONDS)

        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

        # File should no longer be useful:
        af.read(1).nbytes == 0

        # Seeking back to the start of the file should work:
        assert af.seekable()
        af.seek(0)
        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

        # Seeking to an arbitrary point should also work
        af.seek(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS / 2))

        assert f"samplerate={int(af.samplerate)}" in repr(af)
        assert f"num_channels={af.num_channels}" in repr(af)

    with pytest.raises(RuntimeError):
        af.num_channels

    with pytest.raises(RuntimeError):
        af.read(1)


def test_context_manager_allows_exceptions():
    with pytest.raises(AssertionError):
        with pedalboard.io.AudioFile(FILENAMES_AND_SAMPLERATES[0][0]) as af:
            assert False

    assert af.closed


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_read_okay_without_extension(
    tmp_path: pathlib.Path, audio_filename: str, samplerate: float
):
    dest_path = str(tmp_path / "no_extension")
    shutil.copyfile(audio_filename, dest_path)
    try:
        with pedalboard.io.AudioFile(dest_path) as af:
            assert af.samplerate == samplerate
            assert af.num_channels == 1
    except Exception:
        if ".mp3" in audio_filename:
            # Skip this test - due to a bug in JUCE's MP3 reader on Linux/Windows,
            # we throw an exception when trying to read MP3 files without a known
            # extension.
            pass
        else:
            raise


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_read_from_seekable_stream(audio_filename: str, samplerate: float):
    with open(audio_filename, "rb") as f:
        stream = io.BytesIO(f.read())

    try:
        af = pedalboard.io.AudioFile(stream)
    except Exception:
        if ".mp3" in audio_filename:
            # Skip this test - due to a bug in JUCE's MP3 reader on Linux/Windows,
            # we throw an exception when trying to read MP3 files without a known
            # extension.
            return
        else:
            raise

    with af:
        assert af.samplerate == samplerate
        assert af.num_channels == 1
        if any(ext in audio_filename for ext in EXPECT_LENGTH_TO_BE_EXACT):
            assert af.frames == int(samplerate * EXPECTED_DURATION_SECONDS)
        else:
            assert af.frames >= int(samplerate * EXPECTED_DURATION_SECONDS)

        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

        # File should no longer be useful:
        af.read(1).nbytes == 0

        # Seeking back to the start of the file should work:
        assert af.seekable()
        af.seek(0)
        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS))

        # Seeking to an arbitrary point should also work
        af.seek(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
        samples = af.read(int(samplerate * EXPECTED_DURATION_SECONDS / 2))
        assert samples.shape == (1, int(samplerate * EXPECTED_DURATION_SECONDS / 2))

        assert f"samplerate={int(af.samplerate)}" in repr(af)
        assert f"num_channels={af.num_channels}" in repr(af)
        assert repr(stream) in repr(af)

    with pytest.raises(RuntimeError):
        af.num_channels

    with pytest.raises(RuntimeError):
        af.read(1)


@pytest.mark.parametrize(
    "mp3_filename",
    [f for f in sum(TEST_AUDIO_FILES.values(), []) if f.endswith("mp3")],
)
def test_read_mp3_from_named_stream(mp3_filename: str):
    with pedalboard.io.AudioFile(open(mp3_filename, "rb")) as af:
        assert af is not None


def test_file_like_exceptions_propagate():
    audio_filename = FILENAMES_AND_SAMPLERATES[0][0]
    stream = open(audio_filename, "rb")
    stream_read = stream.read

    should_throw = [False]

    def eventually_throw_exception(*args, **kwargs):
        if should_throw[0]:
            raise ValueError("Some kinda error!")
        return stream_read(*args, **kwargs)

    stream.read = eventually_throw_exception

    with pedalboard.io.AudioFile(stream) as af:
        assert af.read(1).nbytes > 0
        should_throw[0] = True
        with pytest.raises(ValueError) as e:
            for _ in range(af.frames - 1):
                af.read(1)
        assert "Some kinda error!" in str(e)


def test_file_like_must_be_seekable():
    audio_filename = FILENAMES_AND_SAMPLERATES[0][0]

    with open(audio_filename, "rb") as f:
        stream = io.BytesIO(f.read())
    stream.seekable = lambda: False

    with pytest.raises(ValueError) as e:
        with pedalboard.io.AudioFile(stream):
            pass

    assert "seekable" in str(e)


def test_no_crash_if_type_error_on_file_like():
    audio_filename = FILENAMES_AND_SAMPLERATES[0][0]

    with open(audio_filename, "rb") as f:
        stream = io.BytesIO(f.read())

    # Seekable should be a method, not a property:
    stream.seekable = False

    with pytest.raises(TypeError) as e:
        with pedalboard.io.AudioFile(stream):
            pass

    assert "bool" in str(e)


def test_write_fails_without_extension(tmp_path: pathlib.Path):
    dest_path = str(tmp_path / "no_extension")
    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(dest_path, "w", 44100, 1)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
def test_write_to_stream_supports_format(extension: str):
    assert pedalboard.io.AudioFile(io.BytesIO(), "w", 44100, 2, format=extension) is not None
    assert pedalboard.io.AudioFile(io.BytesIO(), "w", 44100, 2, format=extension[1:]) is not None
    assert pedalboard.io.WriteableAudioFile(io.BytesIO(), 44100, 2, format=extension) is not None
    assert (
        pedalboard.io.WriteableAudioFile(io.BytesIO(), 44100, 2, format=extension[1:]) is not None
    )

    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(io.BytesIO(), "w", 44100, 2, format="txt")


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
def test_write_to_stream_prefers_format_over_stream_name(extension: str):
    stream = io.BytesIO()
    stream.name = "foo.txt"
    assert pedalboard.io.AudioFile(stream, "w", 44100, 2, format=extension) is not None
    assert pedalboard.io.AudioFile(stream, "w", 44100, 2, format=extension[1:]) is not None
    assert pedalboard.io.WriteableAudioFile(stream, 44100, 2, format=extension) is not None
    assert pedalboard.io.WriteableAudioFile(stream, 44100, 2, format=extension[1:]) is not None

    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(stream, "w", 44100, 2)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
def test_read_from_non_bytes_stream(extension: str):
    stream = io.StringIO()
    stream.name = f"foo{extension}"

    with pytest.raises(TypeError) as e:
        pedalboard.io.AudioFile(stream, "r")

    assert "expected to return bytes" in str(e)
    assert "returned str" in str(e)

    with pytest.raises(TypeError) as e:
        pedalboard.io.ReadableAudioFile(stream)

    assert "expected to return bytes" in str(e)
    assert "returned str" in str(e)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
def test_write_to_non_bytes_stream(extension: str):
    stream = io.StringIO()

    try:
        stream.write(b"")
    except TypeError as e:
        expected_message = e.args[0]

    with pytest.raises(TypeError) as e:
        with pedalboard.io.AudioFile(stream, "w", 44100, 2, format=extension) as af:
            af.write(np.random.rand(1, 2))

    assert expected_message in str(e)

    with pytest.raises(TypeError) as e:
        with pedalboard.io.WriteableAudioFile(stream, 44100, 2, format=extension) as af:
            af.write(np.random.rand(1, 2))

    assert expected_message in str(e)


def test_fails_gracefully():
    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(__file__)

    with pytest.raises(ValueError):
        with pedalboard.io.AudioFile(__file__):
            pass


@pytest.mark.parametrize("audio_filename", UNSUPPORTED_FILENAMES)
def test_fails_on_unsupported_format(audio_filename: str):
    with pytest.raises(ValueError):
        af = pedalboard.io.AudioFile(audio_filename)
        assert not af


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize(
    "samplerate", [8000, 11025, 12000, 16000, 22050, 32000, 44100, 48000, 88200, 96000]
)
@pytest.mark.parametrize("num_channels", [1, 2, 3])
@pytest.mark.parametrize("transposed", [False, True])
@pytest.mark.parametrize("input_format", [np.float32, np.float64, np.int8, np.int16, np.int32])
def test_basic_write(
    tmp_path: pathlib.Path,
    extension: str,
    samplerate: float,
    num_channels: int,
    transposed: bool,
    input_format,
):
    if extension == ".mp3":
        if samplerate not in {32000, 44100, 48000}:
            return
        if num_channels > 2:
            return
    filename = str(tmp_path / f"test{extension}")
    original_audio = generate_sine_at(samplerate, num_channels=num_channels)

    write_bit_depth = 16

    # Not all formats support full 32-bit depth:
    if extension in {".wav"} and np.issubdtype(input_format, np.signedinteger):
        write_bit_depth = np.dtype(input_format).itemsize * 8

    # Handle integer audio types by scaling the floating-point data to the full integer range:
    if np.issubdtype(input_format, np.signedinteger):
        _max = np.iinfo(input_format).max
        audio = (original_audio * _max).astype(input_format)
    else:
        _max = 1.0
        audio = original_audio.astype(input_format)

    # Before writing, assert that the data we're about to write is what we expect:
    tolerance = get_tolerance_for_format_and_bit_depth(".wav", input_format, "int16")
    np.testing.assert_allclose(original_audio, audio.astype(np.float32) / _max, atol=tolerance)

    num_samples = audio.shape[-1]

    with pedalboard.io.WriteableAudioFile(
        filename,
        samplerate=samplerate,
        num_channels=num_channels,
        bit_depth=write_bit_depth,
    ) as af:
        if transposed:
            af.write(audio.T)
        else:
            af.write(audio)

    assert os.path.exists(filename)
    assert os.path.getsize(filename) > 0
    with pedalboard.io.ReadableAudioFile(filename) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == num_channels
        assert af.frames >= num_samples
        tolerance = get_tolerance_for_format_and_bit_depth(extension, input_format, af.file_dtype)
        as_written = af.read(num_samples)
        np.testing.assert_allclose(original_audio, np.squeeze(as_written), atol=tolerance)


def test_write_exact_int32_to_16_bit_wav(tmp_path: pathlib.Path):
    filename = str(tmp_path / "test.wav")
    original = np.array([1, 2, 3, -1, -2, -3]).astype(np.int32)
    signal = (original << 16).astype(np.int32)

    with pedalboard.io.WriteableAudioFile(filename, samplerate=1) as af:
        af.write(signal)

    assert os.path.exists(filename)

    # Read the exact wave values out with the `wave` package:
    with wave.open(filename) as f:
        assert f.getsampwidth() == 2
        encoded = np.frombuffer(f.readframes(len(signal)), dtype=np.int16)
        np.testing.assert_allclose(encoded, original)


def test_read_16_bit_wav_matches_stdlib(tmp_path: pathlib.Path):
    filename = str(tmp_path / "test-16bit.wav")
    original = np.array([1, 2, 3, -1, -2, -3]).astype(np.int16)
    signal = original.astype(np.int32) << 16

    with pedalboard.io.WriteableAudioFile(filename, samplerate=1, bit_depth=16) as af:
        af.write(signal)

    # Read the exact wave values out with the `wave` package:
    with wave.open(filename) as f:
        assert f.getsampwidth() == 2
        stdlib_result = np.frombuffer(f.readframes(len(signal)), dtype=np.int16)
        np.testing.assert_allclose(stdlib_result, original)

    float_signal = original / np.iinfo(np.int16).max
    with pedalboard.io.AudioFile(filename) as af:
        np.testing.assert_allclose(float_signal, af.read(af.frames)[0])


def test_basic_write_int32_to_16_bit_wav(tmp_path: pathlib.Path):
    samplerate = 44100
    num_channels = 1
    filename = str(tmp_path / "test.wav")
    original = np.linspace(0, 1, 11)

    # As per AES17: the integer value -(2^31) should never show up in the stream.
    signal = (original * (2**31 - 1)).astype(np.int32)

    with pedalboard.io.WriteableAudioFile(
        filename,
        samplerate=samplerate,
        num_channels=num_channels,
        bit_depth=16,
    ) as af:
        af.write(signal)

    # Read the exact wave values out with the `wave` package:
    with wave.open(filename) as f:
        assert f.getsampwidth() == 2
        stdlib_result = np.frombuffer(f.readframes(len(signal)), dtype=np.int16)
        assert np.all(np.equal(stdlib_result, signal >> 16))

    with pedalboard.io.ReadableAudioFile(filename) as af:
        as_written = af.read(len(signal))[0]
        np.testing.assert_allclose(original, as_written, atol=2 / (2**15))


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize(
    "samplerate", [8000, 11025, 12000, 16000, 22050, 32000, 44100, 48000, 88200, 96000]
)
@pytest.mark.parametrize("num_channels", [1, 2, 3])
@pytest.mark.parametrize("transposed", [False, True])
@pytest.mark.parametrize("input_format", [np.float32, np.float64, np.int8, np.int16, np.int32])
def test_write_to_seekable_stream(
    extension: str, samplerate: float, num_channels: int, transposed: bool, input_format
):
    if extension == ".mp3":
        if samplerate not in {32000, 44100, 48000}:
            return
        if num_channels > 2:
            return
    original_audio = generate_sine_at(samplerate, num_channels=num_channels)

    write_bit_depth = 16

    # Not all formats support full 32-bit depth:
    if extension in {".wav"} and np.issubdtype(input_format, np.signedinteger):
        write_bit_depth = np.dtype(input_format).itemsize * 8

    # Handle integer audio types by scaling the floating-point data to the full integer range:
    if np.issubdtype(input_format, np.signedinteger):
        _max = np.iinfo(input_format).max
        audio = (original_audio * _max).astype(input_format)
    else:
        _max = 1.0
        audio = original_audio.astype(input_format)

    # Before writing, assert that the data we're about to write is what we expect:
    tolerance = get_tolerance_for_format_and_bit_depth(".wav", input_format, "int16")
    np.testing.assert_allclose(original_audio, audio.astype(np.float32) / _max, atol=tolerance)

    num_samples = audio.shape[-1]

    stream = io.BytesIO()
    stream.name = f"my_file{extension}"

    with pedalboard.io.WriteableAudioFile(
        stream,
        samplerate=samplerate,
        num_channels=num_channels,
        bit_depth=write_bit_depth,
    ) as af:
        if transposed:
            af.write(audio.T)
        else:
            af.write(audio)

    assert stream.tell() > 0
    stream.seek(0)

    with pedalboard.io.AudioFile(stream) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == num_channels
        tolerance = get_tolerance_for_format_and_bit_depth(extension, input_format, af.file_dtype)
        as_written = af.read(num_samples)
        np.testing.assert_allclose(original_audio, np.squeeze(as_written), atol=tolerance)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize("samplerate", [32000, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_write_twice_overwrites(
    tmp_path: pathlib.Path, extension: str, samplerate: float, num_channels: int
):
    filename = str(tmp_path / f"test{extension}")
    original_audio = np.zeros((num_channels, samplerate))

    with pedalboard.io.AudioFile(
        filename, "w", samplerate=samplerate, num_channels=num_channels
    ) as af:
        af.write(original_audio)

    assert os.path.exists(filename)
    assert os.path.getsize(filename) > 0

    with pedalboard.io.AudioFile(filename) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == num_channels
        assert af.frames > 0
        first_read_result = af.read(af.frames)

    # Write again:
    with pedalboard.io.AudioFile(
        filename, "w", samplerate=samplerate, num_channels=num_channels
    ) as af:
        af.write(original_audio)

    with pedalboard.io.AudioFile(filename) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == num_channels
        assert af.frames > 0
        second_read_result = af.read(af.frames)

    np.testing.assert_allclose(second_read_result, first_read_result)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize("samplerate", [1234.5, 23.0000000001])
def test_fractional_sample_rates(tmp_path: pathlib.Path, extension: str, samplerate):
    filename = str(tmp_path / f"test{extension}")
    with pytest.raises(ValueError):
        pedalboard.io.WriteableAudioFile(filename, samplerate=samplerate, num_channels=1)


@pytest.mark.parametrize("extension", set(pedalboard.io.get_supported_write_formats()) - {".mp3"})
@pytest.mark.parametrize("samplerate", [123, 999, 48001])
def test_uncommon_sample_rates(tmp_path: pathlib.Path, extension: str, samplerate):
    filename = str(tmp_path / f"test{extension}")
    with pedalboard.io.WriteableAudioFile(filename, samplerate=samplerate, num_channels=1):
        pass
    with pedalboard.io.ReadableAudioFile(filename) as af:
        assert af.samplerate == samplerate


@pytest.mark.parametrize("extension", [".flac"])
@pytest.mark.parametrize("samplerate", [123456, 234567])
def test_unusable_sample_rates(tmp_path: pathlib.Path, extension: str, samplerate):
    filename = str(tmp_path / f"test{extension}")
    with pytest.raises(ValueError) as e:
        pedalboard.io.WriteableAudioFile(filename, samplerate=samplerate, num_channels=1)
    assert "44100" in str(e), "Expected exception to include details about supported sample rates."


@pytest.mark.parametrize("dtype", [np.uint8, np.uint16, np.uint32, np.uint64])
def test_fail_to_write_unsigned(tmp_path: pathlib.Path, dtype):
    filename = str(tmp_path / "test.wav")
    with pytest.raises(TypeError):
        with pedalboard.io.WriteableAudioFile(filename, samplerate=44100) as af:
            af.write(np.array([1, 2, 3, 4], dtype=dtype))


@pytest.mark.parametrize("samplerate", [32000, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize(
    "qualities",
    [
        ["V9", "V8", "V7", "V6", "V5", "V4", "V3", "V2", "V1", "V0"],
        [32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
    ],
)
def test_mp3_write(samplerate: float, num_channels: int, qualities):
    audio = generate_sine_at(samplerate, num_channels=num_channels)

    file_sizes = []
    for quality in qualities:
        buffer = io.BytesIO()
        with pedalboard.io.WriteableAudioFile(
            buffer,
            samplerate,
            num_channels,
            format="mp3",
            quality=quality,
        ) as af:
            af.write(audio)
        file_sizes.append(len(buffer.getvalue()))

    # Assert that file sizes change as bitrate changes:
    assert file_sizes == sorted(file_sizes)
    # ...and that they don't all share the same output size:
    # (note that this won't be true for VBR, because it's adaptive)
    assert len(set(file_sizes)) >= len(file_sizes) // 2
    assert len(set(file_sizes)) <= len(file_sizes)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize("samplerate", [32000, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_write_empty_file(extension: str, samplerate: float, num_channels: int):
    stream = io.BytesIO()

    # Set the stream's name so that ReadableAudioFile knows that it's an MP3 on Linux/Windows
    stream.name = f"test{extension}"

    with pedalboard.io.WriteableAudioFile(
        stream,
        samplerate=samplerate,
        num_channels=num_channels,
        format=extension,
    ) as af:
        pass

    assert len(stream.getvalue()) > 0
    stream.seek(0)

    with pedalboard.io.AudioFile(stream) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == num_channels
        # The built-in JUCE MP3 reader (only used on Linux and Windows)
        # reads zero-length MP3 files as having exactly one frame.
        if "mp3" in extension and platform.system() != "Darwin":
            assert af.frames <= 1152
            contents = af.read(af.frames)
            np.testing.assert_allclose(np.zeros_like(contents), contents)
        else:
            assert af.frames == 0


@pytest.mark.parametrize(
    "extension,quality,expected",
    [
        ("ogg", "64 kbps", "64 kbps"),
        ("ogg", "80 kbps", "80 kbps"),
        ("ogg", "96 kbps", "96 kbps"),
        ("ogg", "112 kbps", "112 kbps"),
        ("ogg", "128 kbps", "128 kbps"),
        ("ogg", "160 kbps", "160 kbps"),
        ("ogg", "192 kbps", "192 kbps"),
        ("ogg", "224 kbps", "224 kbps"),
        ("ogg", "256 kbps", "256 kbps"),
        ("ogg", "320 kbps", "320 kbps"),
        ("ogg", "500 kbps", "500 kbps"),
        ("ogg", 64, "64 kbps"),
        ("ogg", 80, "80 kbps"),
        ("ogg", 96, "96 kbps"),
        ("ogg", 112, "112 kbps"),
        ("ogg", 128, "128 kbps"),
        ("ogg", 160, "160 kbps"),
        ("ogg", 192, "192 kbps"),
        ("ogg", 224, "224 kbps"),
        ("ogg", 256, "256 kbps"),
        ("ogg", 320, "320 kbps"),
        ("ogg", 500, "500 kbps"),
        ("ogg", 64.0, "64 kbps"),
        ("ogg", 80.0, "80 kbps"),
        ("ogg", 96.0, "96 kbps"),
        ("ogg", 112.0, "112 kbps"),
        ("ogg", 128.0, "128 kbps"),
        ("ogg", 160.0, "160 kbps"),
        ("ogg", 192.0, "192 kbps"),
        ("ogg", 224.0, "224 kbps"),
        ("ogg", 256.0, "256 kbps"),
        ("ogg", 320.0, "320 kbps"),
        ("ogg", 500.0, "500 kbps"),
        ("ogg", "64", "64 kbps"),
        ("ogg", "80", "80 kbps"),
        ("ogg", "96", "96 kbps"),
        ("ogg", "112", "112 kbps"),
        ("ogg", "128", "128 kbps"),
        ("ogg", "160", "160 kbps"),
        ("ogg", "192", "192 kbps"),
        ("ogg", "224", "224 kbps"),
        ("ogg", "256", "256 kbps"),
        ("ogg", "320", "320 kbps"),
        ("ogg", "500", "500 kbps"),
        ("ogg", "   500   ", "500 kbps"),
        ("ogg", "", "500 kbps"),
        ("ogg", "  ", "500 kbps"),
        ("ogg", None, "500 kbps"),
        ("flac", "0 (Fastest)", "0 (Fastest)"),
        ("flac", "0", "0 (Fastest)"),
        ("flac", "fastest", "0 (Fastest)"),
        ("flac", "1", "1"),
        ("flac", "2", "2"),
        ("flac", "3", "3"),
        ("flac", "4", "4"),
        ("flac", "default", "5 (Default)"),
        ("flac", "5 (Default)", "5 (Default)"),
        ("flac", "5", "5 (Default)"),
        ("flac", "6", "6"),
        ("flac", "7", "7"),
        ("flac", "8 (Highest quality)", "8 (Highest quality)"),
        ("flac", "8", "8 (Highest quality)"),
        ("flac", "", "8 (Highest quality)"),
        ("flac", "  ", "8 (Highest quality)"),
        ("flac", None, "8 (Highest quality)"),
        ("flac", "high", "8 (Highest quality)"),
        ("flac", 0, "0 (Fastest)"),
        ("flac", 1, "1"),
        ("flac", 2, "2"),
        ("flac", 3, "3"),
        ("flac", 4, "4"),
        ("flac", 5, "5 (Default)"),
        ("flac", 6, "6"),
        ("flac", 7, "7"),
        ("flac", 8, "8 (Highest quality)"),
        ("mp3", "V0", "V0 (best)"),
        ("mp3", "V0 (best)", "V0 (best)"),
        ("mp3", "best", "V0 (best)"),
        ("mp3", "V1", "V1"),
        ("mp3", "V2", "V2"),
        ("mp3", "V3", "V3"),
        ("mp3", "V4", "V4 (normal)"),
        ("mp3", "V4 (normal)", "V4 (normal)"),
        ("mp3", "normal", "V4 (normal)"),
        ("mp3", "V5", "V5"),
        ("mp3", "V6", "V6"),
        ("mp3", "V7", "V7"),
        ("mp3", "V8", "V8"),
        ("mp3", "V9", "V9 (smallest)"),
        ("mp3", "V9 (smallest)", "V9 (smallest)"),
        ("mp3", "smallest", "V9 (smallest)"),
        ("mp3", "", "320 kbps"),
        ("mp3", "  ", "320 kbps"),
        ("mp3", None, "320 kbps"),
        ("mp3", 32, "32 kbps"),
        ("mp3", 40, "40 kbps"),
        ("mp3", 48, "48 kbps"),
        ("mp3", 56, "56 kbps"),
        ("mp3", 64, "64 kbps"),
        ("mp3", 80, "80 kbps"),
        ("mp3", 96, "96 kbps"),
        ("mp3", 112, "112 kbps"),
        ("mp3", 128, "128 kbps"),
        ("mp3", 160, "160 kbps"),
        ("mp3", 192, "192 kbps"),
        ("mp3", 224, "224 kbps"),
        ("mp3", 256, "256 kbps"),
        ("mp3", 320, "320 kbps"),
        ("mp3", "32 kbps", "32 kbps"),
        ("mp3", "40 kbps", "40 kbps"),
        ("mp3", "48 kbps", "48 kbps"),
        ("mp3", "56 kbps", "56 kbps"),
        ("mp3", "64 kbps", "64 kbps"),
        ("mp3", "80 kbps", "80 kbps"),
        ("mp3", "96 kbps", "96 kbps"),
        ("mp3", "112 kbps", "112 kbps"),
        ("mp3", "128 kbps", "128 kbps"),
        ("mp3", "160 kbps", "160 kbps"),
        ("mp3", "192 kbps", "192 kbps"),
        ("mp3", "224 kbps", "224 kbps"),
        ("mp3", "256 kbps", "256 kbps"),
        ("mp3", "320 kbps", "320 kbps"),
        ("wav", None, None),
        ("wav", "", None),
        ("aiff", None, None),
        ("aiff", "", None),
        ("ogg", "fastest", "64 kbps"),
        ("ogg", "worst", "64 kbps"),
        ("ogg", "slowest", "500 kbps"),
        ("ogg", "best", "500 kbps"),
        ("flac", "fastest", "0 (Fastest)"),
        ("flac", "worst", "0 (Fastest)"),
        ("flac", "slowest", "8 (Highest quality)"),
        ("flac", "best", "8 (Highest quality)"),
        ("mp3", "fastest", "V9 (smallest)"),
        ("mp3", "worst", "V9 (smallest)"),
        ("mp3", "slowest", "V0 (best)"),
        ("mp3", "best", "V0 (best)"),
        ("wav", "fastest", None),
        ("wav", "slowest", None),
        ("aiff", "fastest", None),
        ("aiff", "slowest", None),
    ],
)
def test_write_quality(tmp_path: pathlib.Path, extension: str, quality, expected: Optional[str]):
    filename = str(tmp_path / f"test.{extension}")
    with pedalboard.io.WriteableAudioFile(filename, samplerate=44100, quality=quality) as af:
        assert af.quality == expected


@pytest.mark.parametrize(
    "extension,quality",
    [
        ("ogg", "63 kbps"),
        ("ogg", 63),
        ("ogg", 63.5),
        ("ogg", -500),
        ("flac", 11),
        ("flac", -1),
        ("wav", "128"),
        ("wav", 128),
        ("aiff", "128"),
        ("aiff", 128),
    ],
)
def test_bad_write_quality(tmp_path: pathlib.Path, extension: str, quality):
    filename = str(tmp_path / f"test.{extension}")
    with pytest.raises(ValueError):
        pedalboard.io.WriteableAudioFile(filename, samplerate=44100, quality=quality)


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="Windows file handling behaves differently, for some reason",
)
def test_file_not_created_if_constructor_error_thrown(tmp_path: pathlib.Path):
    filename = str(tmp_path / "test.wav")
    assert not os.path.exists(filename)
    with pytest.raises(ValueError):
        pedalboard.io.WriteableAudioFile(filename, samplerate=44100, quality="break")
    assert not os.path.exists(filename)
