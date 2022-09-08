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

import pytest
from pedalboard import Resample
from pedalboard.io import AudioFile, StreamResampler, ResampledReadableAudioFile
from io import BytesIO

import numpy as np


from .utils import generate_sine_at


def expected_output(
    input_signal, sample_rate: float, target_sample_rate: float, num_channels: int, quality
) -> np.ndarray:
    resampler = StreamResampler(sample_rate, target_sample_rate, num_channels, quality)
    output = np.concatenate([resampler.process(input_signal), resampler.process(None)], axis=1)
    return output


QUALITIES = [v[0] for v in Resample.Quality.__entries.values()]


def test_read_resampled_constructor():
    sine_wave = generate_sine_at(44100, 440, num_seconds=1, num_channels=1).astype(np.float32)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", 44100, 1, bit_depth=32) as f:
        f.write(sine_wave)

    with AudioFile(BytesIO(read_buffer.getvalue())) as f:
        with f.resampled_to(22050) as r:
            assert isinstance(r, ResampledReadableAudioFile)
        assert r.closed
        assert not f.closed
    assert f.closed


def test_read_zero():
    sine_wave = generate_sine_at(44100, 440, num_seconds=1, num_channels=1).astype(np.float32)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", 44100, 1, bit_depth=32) as f:
        f.write(sine_wave)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(22050) as f:
        with pytest.raises(ValueError):
            f.read()


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [123.45, 8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", QUALITIES)
def test_read_resampled(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    num_channels: int,
    quality,
):
    sine_wave = generate_sine_at(
        sample_rate, fundamental_hz, num_seconds=1, num_channels=num_channels
    ).astype(np.float32)
    expected_sine_wave = expected_output(
        sine_wave, sample_rate, target_sample_rate, num_channels, quality
    )

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, num_channels, bit_depth=32) as f:
        f.write(sine_wave)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(target_sample_rate, quality) as f:
        actual = f.read(f.frames)
        np.testing.assert_allclose(expected_sine_wave, actual)


@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("chunk_size", [10, 100])
@pytest.mark.parametrize("quality", QUALITIES)
def test_tell_resampled(sample_rate: float, target_sample_rate: float, chunk_size: int, quality):
    signal = np.linspace(1, sample_rate, sample_rate).astype(np.float32)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, 1, bit_depth=32) as f:
        f.write(signal)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(target_sample_rate, quality) as f:
        for i in range(0, f.frames, chunk_size):
            assert f.tell() == i
            if f.read(chunk_size).shape[-1] < chunk_size:
                break


@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("quality", QUALITIES)
def test_seek_resampled(sample_rate: float, target_sample_rate: float, quality):
    signal = np.linspace(1, sample_rate, sample_rate).astype(np.float32)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, 1, bit_depth=32) as f:
        f.write(signal)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(target_sample_rate, quality) as f:
        f.read(10)
        expected = f.read(10)
        f.seek(10)
        actual = f.read(10)
        np.testing.assert_allclose(expected, actual)


@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("chunk_size", [1, 4, 5, 10, 100])
@pytest.mark.parametrize("quality", QUALITIES)
def test_read_resampled_in_chunks(
    sample_rate: float, target_sample_rate: float, chunk_size: int, quality
):
    signal = np.linspace(1, sample_rate, sample_rate).astype(np.float32)
    expected_signal = expected_output(signal, sample_rate, target_sample_rate, 1, quality)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, 1, bit_depth=32) as f:
        f.write(signal)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(target_sample_rate, quality) as f:
        samples_received = 0
        while f.tell() < expected_signal.shape[-1]:
            expected_num_frames = min(chunk_size, expected_signal.shape[-1] - f.tell())
            pos = f.tell()
            output = f.read(chunk_size)
            output_size = output.shape[-1]
            assert output_size == expected_num_frames

            np.testing.assert_allclose(
                expected_signal[:, samples_received : samples_received + output_size],
                output,
                err_msg=f"Output mismatch from {pos:,} to {f.tell():,} of {f.frames:,} samples.",
            )

            samples_received += output_size
        assert samples_received == f.tell()


@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("quality", QUALITIES)
def test_frame_count(sample_rate: float, target_sample_rate: float, quality):
    signal = np.linspace(1, sample_rate, sample_rate).astype(np.float32)
    expected_signal = expected_output(signal, sample_rate, target_sample_rate, 1, quality)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, 1, bit_depth=32) as f:
        f.write(signal)

    with AudioFile(BytesIO(read_buffer.getvalue())).resampled_to(target_sample_rate, quality) as f:
        assert f.frames == expected_signal.shape[-1]
