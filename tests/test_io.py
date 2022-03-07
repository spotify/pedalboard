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


import os
import glob
import pathlib
import shutil
import pytest
import pedalboard

import numpy as np

from .utils import generate_sine_at

EXPECTED_DURATION_SECONDS = 5
EXPECT_LENGTH_TO_BE_EXACT = {"mp3", "wav", "aiff", "aifc", "caf", "ogg", "m4a", "mp4"}

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


def test_read_constructor_dispatch():
    filename, _samplerate = FILENAMES_AND_SAMPLERATES[0]

    # Support reading a file with just its filename:
    assert isinstance(pedalboard.io.AudioFile(filename), pedalboard.io.ReadableAudioFile)

    # Support reading a file with just its filename and an explicit "r" (read) flag:
    assert isinstance(pedalboard.io.AudioFile(filename, "r"), pedalboard.io.ReadableAudioFile)

    # Support reading a file by using the appropriate subclass constructor just its filename:
    assert isinstance(pedalboard.io.ReadableAudioFile(filename), pedalboard.io.ReadableAudioFile)

    # Don't support reading a file by passing a mode to the subclass constructor (which would be redundant):
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

    # Don't support writing to a file by passing a mode to the subclass constructor (which would be redundant):
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
    assert af.channels == 1
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
    assert f"channels={af.channels}" in repr(af)
    assert f"frames={af.frames}" in repr(af)

    af.close()

    with pytest.raises(RuntimeError):
        af.channels

    with pytest.raises(RuntimeError):
        af.read(1)


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_use_reader_as_context_manager(audio_filename: str, samplerate: float):
    with pedalboard.io.AudioFile(audio_filename) as af:
        assert af.samplerate == samplerate
        assert af.channels == 1
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
        assert f"channels={af.channels}" in repr(af)
        assert f"frames={af.frames}" in repr(af)

    with pytest.raises(RuntimeError):
        af.channels

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
        assert af.channels == 1


def test_write_fails_without_extension(tmp_path: pathlib.Path):
    dest_path = str(tmp_path / "no_extension")
    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(dest_path, "w", 44100, 1)


def test_fails_gracefully():
    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(__file__)

    with pytest.raises(ValueError):
        with pedalboard.io.AudioFile(__file__) as f:
            pass


@pytest.mark.parametrize("audio_filename", UNSUPPORTED_FILENAMES)
def test_fails_on_unsupported_format(audio_filename: str):
    with pytest.raises(ValueError):
        pedalboard.io.AudioFile(audio_filename)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize(
    "samplerate",
    [8000, 11025, 12000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000],
)
@pytest.mark.parametrize("num_channels", [1, 2, 8])
@pytest.mark.parametrize("transposed", [False, True])
def test_basic_write(
    tmp_path: pathlib.Path,
    extension: str,
    samplerate: float,
    num_channels: int,
    transposed: bool,
):
    filename = str(tmp_path / f"test{extension}")
    audio = generate_sine_at(samplerate, num_channels=num_channels).astype(np.float32)
    num_samples = audio.shape[-1]
    if transposed:
        audio = audio.T
    with pedalboard.io.WriteableAudioFile(
        filename, samplerate=samplerate, num_channels=num_channels
    ) as af:
        af.write(audio)
    assert os.path.exists(filename)
    assert os.path.getsize(filename) > 0
    with pedalboard.io.ReadableAudioFile(filename) as af:
        assert af.samplerate == samplerate
        assert af.channels == num_channels
        tolerance = 0.11 if extension == ".ogg" else 2 / (2**16)
        as_written = af.read(num_samples)
        if transposed:
            as_written = as_written.T
        np.testing.assert_allclose(audio, np.squeeze(as_written), atol=tolerance)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
@pytest.mark.parametrize("samplerate", [1234.5, 23.0000000001])
def test_fractional_sample_rates(tmp_path: pathlib.Path, extension: str, samplerate):
    filename = str(tmp_path / f"test{extension}")
    with pytest.raises(ValueError):
        pedalboard.io.WriteableAudioFile(filename, samplerate=samplerate, num_channels=1)


@pytest.mark.parametrize("extension", pedalboard.io.get_supported_write_formats())
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
