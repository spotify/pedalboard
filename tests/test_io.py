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
import mutagen
from typing import Optional

import numpy as np

from .utils import generate_sine_at

EXPECTED_DURATION_SECONDS = 5
EXPECT_LENGTH_TO_BE_EXACT = {"wav", "aiff", "caf", "ogg", "m4a", "mp4"}
MP3_FRAME_LENGTH_SAMPLES = 1152

TEST_AUDIO_FILES = {
    22050: glob.glob(os.path.join(os.path.dirname(__file__), "audio", "correct", "*22050*")),
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
    if not audio_filename.endswith(".mp3"):
        assert af.exact_duration_known
    else:
        assert af.exact_duration_known in (True, False)
    if any(ext in audio_filename for ext in EXPECT_LENGTH_TO_BE_EXACT):
        assert af.frames == int(samplerate * EXPECTED_DURATION_SECONDS)
    else:
        assert af.frames >= int(samplerate * EXPECTED_DURATION_SECONDS)

    samples = af.read(samplerate * EXPECTED_DURATION_SECONDS)
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
    assert af.exact_duration_known in (True, False)
    actual = af.read(af.frames)
    assert af.exact_duration_known
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
    with pedalboard.io.AudioFile(dest_path) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == 1


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_read_from_seekable_stream(audio_filename: str, samplerate: float):
    with open(audio_filename, "rb") as f:
        stream = io.BytesIO(f.read())

    af = pedalboard.io.AudioFile(stream)

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
def test_read_mp3_from_unnamed_stream(mp3_filename: str):
    with open(mp3_filename, "rb") as f:
        file_like = io.BytesIO(f.read())
    with pedalboard.io.AudioFile(file_like) as af:
        assert af is not None


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
def test_read_from_end_of_stream_produces_helpful_error_message(extension: str):
    buf = io.BytesIO()
    buf.name = f"something.{extension}"
    with pedalboard.io.AudioFile(buf, "w", 44100, 1) as af:
        af.write(np.random.rand(44100))
    buf.seek(len(buf.getvalue()))

    try:
        with pedalboard.io.AudioFile(buf) as af:
            assert af.frames >= 44100
    except Exception as e:
        assert "end of the stream" in str(e)
        assert "Try seeking" in str(e)


def test_read_from_empty_stream_produces_helpful_error_message():
    with pytest.raises(ValueError) as exc_info:
        with pedalboard.io.AudioFile(io.BytesIO()):
            pass
    assert "is empty" in str(exc_info.value)


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


def test_file_like_must_be_seekable_for_write():
    stream = io.BytesIO()
    stream.seek = lambda x: (_ for _ in ()).throw(
        ValueError(f"Failed to seek from {stream.tell():,} to {x:,} because I don't wanna")
    )

    with pytest.raises(ValueError) as e:
        with pedalboard.io.AudioFile(stream, "w", 44100, 2, format="flac"):
            pass

    assert "I don't wanna" in str(e)


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
    with pytest.raises(TypeError):
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


@pytest.mark.parametrize("samplerate", [1234, 44100, 48000])
def test_sample_rate_is_int_by_default(samplerate: int):
    buf = io.BytesIO()
    buf.name = "foo.wav"
    with pedalboard.io.AudioFile(buf, "w", samplerate=samplerate, num_channels=1) as f:
        f.write(np.random.rand(100))

    buf.seek(0)
    with pedalboard.io.AudioFile(buf) as f:
        assert isinstance(f.samplerate, int)
        assert f.samplerate == samplerate


@pytest.mark.parametrize("extension", [".flac"])
@pytest.mark.parametrize("samplerate", [22050, 44100, 48000])
def test_swapped_parameter_exception(tmp_path: pathlib.Path, extension: str, samplerate):
    filename = str(tmp_path / f"test{extension}")
    with pytest.raises(ValueError) as e:
        pedalboard.io.WriteableAudioFile(filename, samplerate=1, num_channels=samplerate)
    assert "reversing" in str(
        e
    ), "Expected exception to include details about reversing parameters."


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
        # The built-in JUCE MP3 reader reads zero-length MP3 files as having exactly one frame:
        assert af.frames <= MP3_FRAME_LENGTH_SAMPLES
        if af.frames > 0:
            contents = af.read(af.frames)
            np.testing.assert_allclose(np.zeros_like(contents), contents)


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


def test_real_mp3_parsing_with_lyrics3():
    """
    Lyrics3 is a non-standard extension to the MP3 format that appends lyric data after the last
    MP3 frame. This data is not read by Pedalboard - but the MP3 parser used on Linux and Windows
    chokes if it discovers "garbage" data past the end of the valid MP3 frames. As of v0.6.4, we
    use a patched version of JUCE's MP3 parser to allow reading MP3 files that contain Lyrics3
    data.
    """
    filename = os.path.join(
        os.path.dirname(__file__), "audio", "correct", "bad_audio_file_lyrics3.mp3"
    )
    with pedalboard.io.AudioFile(filename) as f:
        assert f.frames >= 0
        assert f.read(f.frames).shape[1] == f.frames


def test_real_mp3_parsing_with_no_header():
    filename = os.path.join(os.path.dirname(__file__), "audio", "correct", "no_header.mp3")
    with pedalboard.io.AudioFile(filename) as f:
        assert f.frames >= 40000 and f.frames <= 45000
        assert f.read(f.frames).shape[1] == f.frames


@pytest.mark.parametrize("samplerate", [44100, 32000])
@pytest.mark.parametrize("chunk_size", [1, 2, 16])
@pytest.mark.parametrize("target_samplerate", [44100, 32000, 22050, 1234.56])
def test_mp3_duration_estimate(samplerate: float, target_samplerate: float, chunk_size: int):
    buf = io.BytesIO()
    buf.name = "output.mp3"

    num_frames = int(samplerate * 2)
    with pedalboard.io.AudioFile(buf, "w", samplerate, 2, quality=320) as f:
        f.write(np.random.rand(2, num_frames))

    # Write some random crap past the end of the file to break length estimation:
    buf.write(np.random.rand(12345).tobytes())
    # Skip the VBR header to force Pedalboard to estimate length:
    buf.seek(1044)

    with pedalboard.io.AudioFile(buf) as f:
        assert not f.exact_duration_known
        assert f.samplerate == samplerate
        original_frame_estimate = f.frames
        audio = f.read(f.frames)
        assert f.exact_duration_known
        assert f.frames >= num_frames
        assert f.frames <= original_frame_estimate
        assert audio.shape[0] == f.num_channels
        assert audio.shape[1] == f.frames
        assert audio.shape[1] >= num_frames
        assert audio.shape[1] <= original_frame_estimate

        # Seek back to the start
        f.seek(0)
        # Now that we've read the whole file and
        # seeked back, our duration should be accurate:
        assert f.exact_duration_known
        # Number of frames read should match reported value:
        assert f.frames == audio.shape[1]

    # Try reading again, but in chunks this time:
    buf.seek(1044)
    with pedalboard.io.AudioFile(buf) as f:
        # Ensure that if we just keep reading, we get exactly `f.frames` frames:
        total_frames_read = 0
        while True:
            frames_read = f.read(chunk_size).shape[1]
            total_frames_read += frames_read
            if not frames_read:
                break
        assert total_frames_read == audio.shape[1]
        assert total_frames_read == f.frames

    # Try reading again, but checking against f.frames this time:
    buf.seek(1044)
    with pedalboard.io.AudioFile(buf) as f:
        total_frames_read = 0
        while f.tell() < f.frames:
            frames_read = f.read(chunk_size).shape[1]
            total_frames_read += frames_read
            if not frames_read:
                break
        assert total_frames_read == audio.shape[1]
        assert total_frames_read == f.frames

        assert f.frames <= original_frame_estimate

    # Ensure that the ResampledReadableAudioFile interface works as expected too:
    buf.seek(1044)
    with pedalboard.io.AudioFile(buf).resampled_to(
        target_samplerate, quality=pedalboard.Resample.Quality.Linear
    ) as f:
        original_frame_estimate = f.frames
        total_frames_read = 0
        # When resampling, we may need to call read() one more time after getting f.frames
        # out of the file to ensure that we've pulled all of the data from the original file
        # _and_ exhausted any resampler buffers.
        while f.tell() < f.frames or not f.exact_duration_known:
            frames_read = f.read(chunk_size).shape[1]
            total_frames_read += frames_read
        assert f.exact_duration_known
        assert abs(total_frames_read - int(audio.shape[1] * (target_samplerate / samplerate))) <= 1
        assert total_frames_read == f.frames
        assert f.frames <= original_frame_estimate


@pytest.mark.parametrize("chunk_duration", [16, 1024, 2048, 1024 * 1024])
@pytest.mark.parametrize(
    "granularity,max_num_frames",
    [(1, 2305), (16, 2048), (17, 2048), (1024, 44100), (1025, 44100), (44100, 44100)],
)
@pytest.mark.parametrize(
    "extension,quality", [(".wav", None), (".aiff", None)] + [(".flac", q) for q in [0, 5, 8]]
)
def test_seek_accuracy(
    quality: Optional[int],
    chunk_duration: int,
    granularity: int,
    max_num_frames: int,
    extension: str,
):
    stream = io.BytesIO()
    stream.name = "foo" + extension
    sr = 44100
    input_audio = np.random.rand(sr).astype(np.float32)
    with pedalboard.io.AudioFile(stream, "w", sr, quality=quality) as f:
        f.write(input_audio)
    stream.seek(0)

    with pedalboard.io.ReadableAudioFile(stream) as f:
        num_frames = f.frames
        np.testing.assert_allclose(f.read(f.frames)[0, : len(input_audio)], input_audio, atol=0.1)

    for offset in range(0, min(max_num_frames, num_frames), granularity):
        stream.seek(0)
        with pedalboard.io.ReadableAudioFile(stream) as f:
            for _ in range(2):
                f.seek(offset)
                np.testing.assert_allclose(
                    f.read(chunk_duration)[0, : len(input_audio) - offset],
                    input_audio[offset : offset + chunk_duration],
                    atol=0.1,
                    err_msg=(
                        f"{extension} file contents no longer matched after seeking to offset:"
                        f" {offset:,}"
                    ),
                )


@pytest.mark.parametrize("seek_seconds", [0, 0.1, 0.5, 1, 10])
@pytest.mark.parametrize("read_seconds", [1, 10])
@pytest.mark.parametrize(
    "filename", [os.path.join(os.path.dirname(__file__), "audio", "correct", "seek_pinknoise.flac")]
)
def test_real_world_flac_seek_accuracy(filename: str, seek_seconds: float, read_seconds: float):
    with pedalboard.io.AudioFile(filename) as f:
        f.seek(int(seek_seconds * f.samplerate))
        chunk = f.read(int(f.samplerate * read_seconds))
        assert np.amax(np.abs(chunk)) > 0

        # Seek backwards, but not to the start of the file:
        f.seek(int(seek_seconds * f.samplerate))
        chunk_again = f.read(int(f.samplerate * read_seconds))
        assert np.amax(np.abs(chunk)) > 0
        np.testing.assert_allclose(chunk, chunk_again)


def test_flac_files_have_seektable():
    buf = io.BytesIO()
    buf.name = "test.flac"
    with pedalboard.io.AudioFile(buf, "w", 44100) as o:
        o.write(np.random.rand(44100))
    assert mutagen.File(buf).seektable, "Expected to write a FLAC seek table"


@pytest.mark.parametrize(
    "audio_filename,samplerate",
    [(a, s) for a, s in FILENAMES_AND_SAMPLERATES if s == 22050 and ".mp3" in a],
)
def test_22050Hz_mono_mp3(audio_filename: str, samplerate: float):
    """
    File size estimation was broken for 22kHz mono MP3 files.
    This test should catch that kind of problem.
    """
    af = pedalboard.io.ReadableAudioFile(audio_filename)
    assert af.duration < 30.5
    assert af.samplerate == samplerate
    assert af.exact_duration_known in (True, False)
    data_read_all_at_once = af.read(af.frames)
    assert af.exact_duration_known

    chunk_size = MP3_FRAME_LENGTH_SAMPLES
    chunks = []
    af.seek(0)
    while af.tell() < af.frames:
        chunks.append(af.read(chunk_size))
    data_read_in_chunks = np.concatenate(chunks, axis=1)
    np.testing.assert_allclose(data_read_all_at_once, data_read_in_chunks)


@pytest.mark.parametrize("quality", [f"V{x}" for x in range(0, 10)] + [320, 64])
@pytest.mark.parametrize(
    "samplerate", [8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000]
)
@pytest.mark.parametrize("num_channels", [1, 2])
def test_mp3_at_all_samplerates(quality: str, samplerate: float, num_channels: int):
    secs = 2
    # Make an audio signal that is equal parts noise and silence to make sure
    # we end up with a mixture of bitrates in the file:
    signal = np.concatenate(
        [np.random.rand(samplerate * secs) - 0.5, np.zeros(samplerate * secs)]
    ).astype(np.float32)
    if num_channels == 2:
        signal = np.stack([signal] * num_channels)
    else:
        signal = np.expand_dims(signal, 0)

    buf = io.BytesIO()
    buf.name = "test.mp3"
    with pedalboard.io.AudioFile(
        buf, "w", samplerate, num_channels=num_channels, quality=quality
    ) as f:
        f.write(signal)

    read_buf = io.BytesIO(buf.getvalue())

    with pedalboard.io.ReadableAudioFile(read_buf) as af:
        # Allow for up to two MP3 frames of padding:
        assert af.frames <= (signal.shape[-1] + MP3_FRAME_LENGTH_SAMPLES * 2)
        assert af.frames >= signal.shape[-1]
        assert af.exact_duration_known
        # MP3 is lossy, so we can't expect the waveforms to be comparable;
        # but at least make sure that the first half of the signal is loud
        # and the second half is silent:
        assert np.amax(np.mean(af.read(samplerate * secs), axis=0)) >= np.amax(
            signal[:, : samplerate * secs]
        )
        # skip a couple MP3 frames:
        af.read(MP3_FRAME_LENGTH_SAMPLES * 2)
        assert np.amax(np.mean(af.read(samplerate * secs), axis=0)) < 0.01


def test_useful_exception_when_writing_to_unseekable_file_like():
    """
    Sigh.

    Writing to a tensorflow.io.gfile.GFile object fails, due to a known TensorFlow
    bug (as of March 9th, 2023): https://github.com/tensorflow/tensorflow/issues/32122

    While Pedalboard can't (or won't) easily work around this bug,
    this test ensures that if we encounter a misbehaving file-like object,
    we throw a useful error message.
    """

    class ILieAboutSeekability(object):
        def __init__(self):
            self.bytes_written = 0

        @property
        def name(self) -> str:
            return "something.wav"

        def seekable(self) -> bool:
            return True

        def write(self, data: bytes) -> None:
            self.bytes_written += len(data)

        def seek(self, new_position: int) -> None:
            raise NotImplementedError("What's a seek?")

        def tell(self) -> int:
            return self.bytes_written

    with pytest.raises(NotImplementedError) as e:
        with pedalboard.io.AudioFile(ILieAboutSeekability(), "w", 44100, 2) as f:
            f.write(np.random.rand(2, 44100))
    assert "What's a seek?" in str(e)
