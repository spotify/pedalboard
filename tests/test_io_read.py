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
import pytest
import pedalboard

TEST_AUDIO_FILES = {
    44100: glob.glob(os.path.join(os.path.dirname(__file__), "audio", "correct", "*44100*")),
    48000: glob.glob(os.path.join(os.path.dirname(__file__), "audio", "correct", "*48000*")),
}

FILENAMES_AND_SAMPLERATES = [
    (filename, samplerate)
    for samplerate, filenames in TEST_AUDIO_FILES.items()
    for filename in filenames
    # On some platforms, not all extensions will be available.
    if any(filename.endswith(extension) for extension in pedalboard.io.AudioFile.supported_formats)
]

UNSUPPORTED_FILENAMES = [
    filename
    for filename in sum(TEST_AUDIO_FILES.values(), [])
    if not any(
        filename.endswith(extension) for extension in pedalboard.io.AudioFile.supported_formats
    )
]


@pytest.mark.parametrize("extension", [".mp3", ".wav", ".ogg", ".flac"])
def test_basic_formats_available_on_all_platforms(extension: str):
    assert extension in pedalboard.io.AudioFile.supported_formats


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_basic_load(audio_filename: str, samplerate: float):
    af = pedalboard.io.AudioFile(audio_filename)
    assert af.samplerate == samplerate
    assert af.channels == 1
    assert af.frames == int(samplerate * 5)

    samples = af.read(af.frames)
    assert samples.shape == (1, int(samplerate * 5))

    # File should no longer be useful:
    af.read(1).nbytes == 0


@pytest.mark.parametrize("audio_filename,samplerate", FILENAMES_AND_SAMPLERATES)
def test_use_as_context_manager(audio_filename: str, samplerate: float):
    with pedalboard.io.AudioFile(audio_filename) as af:
        assert af.samplerate == samplerate
        assert af.channels == 1
        assert af.frames == int(samplerate * 5)

        samples = af.read(af.frames)
        assert samples.shape == (1, int(samplerate * 5))

        # File should no longer be useful:
        af.read(1).nbytes == 0

    with pytest.raises(RuntimeError):
        af.channels

    with pytest.raises(RuntimeError):
        af.read(1)


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
