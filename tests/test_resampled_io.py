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
    print(sine_wave.shape, expected_sine_wave.shape)

    read_buffer = BytesIO()
    read_buffer.name = "test.wav"
    with AudioFile(read_buffer, "w", sample_rate, num_channels, bit_depth=32) as f:
        f.write(sine_wave)

    with AudioFile(BytesIO(read_buffer.getvalue())) as f:
        assert f.frames == sine_wave.shape[-1]

    with ResampledReadableAudioFile(
        AudioFile(BytesIO(read_buffer.getvalue())),
        target_sample_rate,
        quality,
    ) as f:
        assert f.frames == expected_sine_wave.shape[-1]
        actual = f.read(f.frames)
        assert actual.shape[1] == f.frames
        np.testing.assert_allclose(expected_sine_wave, actual)
