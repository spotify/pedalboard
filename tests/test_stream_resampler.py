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
import numpy as np
from pedalboard import Resample
from pedalboard.io import StreamResampler
from .utils import generate_sine_at

TOLERANCE_PER_QUALITY = {
    Resample.Quality.ZeroOrderHold: 0.65,
    Resample.Quality.Linear: 0.35,
    Resample.Quality.CatmullRom: 0.16,
    Resample.Quality.Lagrange: 0.16,
    Resample.Quality.WindowedSinc: 0.151,
}


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [256, 8192, 1_000_000])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_stream_resample(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    buffer_size: int,
    num_channels: int,
    quality: Resample.Quality,
):
    sine_wave = generate_sine_at(
        sample_rate,
        fundamental_hz,
        num_channels=num_channels,
        num_seconds=1,
    ).astype(np.float32)
    expected_sine_wave = generate_sine_at(
        target_sample_rate,
        fundamental_hz,
        num_channels=num_channels,
        num_seconds=1,
    ).astype(np.float32)
    if num_channels == 1:
        sine_wave = np.expand_dims(sine_wave, 0)
        expected_sine_wave = np.expand_dims(expected_sine_wave, 0)

    # Downsample:
    resampler = StreamResampler(sample_rate, target_sample_rate, num_channels, quality)
    outputs = [
        resampler.process(sine_wave[:, i : i + buffer_size])
        for i in range(0, sine_wave.shape[1], buffer_size)
    ]
    outputs.append(resampler.process(None))
    output = np.concatenate(outputs, axis=1)

    num_samples = min(output.shape[1], expected_sine_wave.shape[1])

    np.testing.assert_allclose(
        expected_sine_wave[:, :num_samples],
        output[:, :num_samples],
        atol=TOLERANCE_PER_QUALITY[quality],
    )


@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [8000, 11025, 22050])
@pytest.mark.parametrize("target_sample_rate", [8000, 11025, 12345.67])
@pytest.mark.parametrize("buffer_size", [256, 8192, 1_000_000])
@pytest.mark.parametrize("num_channels", [1, 2])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_reset(
    fundamental_hz: float,
    sample_rate: float,
    target_sample_rate: float,
    buffer_size: int,
    num_channels: int,
    quality: Resample.Quality,
):
    sine_wave = generate_sine_at(
        sample_rate,
        fundamental_hz,
        num_channels=num_channels,
        num_seconds=1,
    ).astype(np.float32)
    expected_sine_wave = generate_sine_at(
        target_sample_rate,
        fundamental_hz,
        num_channels=num_channels,
        num_seconds=1,
    ).astype(np.float32)
    if num_channels == 1:
        sine_wave = np.expand_dims(sine_wave, 0)
        expected_sine_wave = np.expand_dims(expected_sine_wave, 0)

    resampler = StreamResampler(sample_rate, target_sample_rate, num_channels, quality)
    original_output = np.concatenate(
        [
            resampler.process(sine_wave[:, i : i + buffer_size])
            for i in range(0, sine_wave.shape[1], buffer_size)
        ]
        + [resampler.process(None)],
        axis=1,
    )

    resampler.reset()
    output_with_reset = np.concatenate(
        [
            resampler.process(sine_wave[:, i : i + buffer_size])
            for i in range(0, sine_wave.shape[1], buffer_size)
        ]
        + [resampler.process(None)],
        axis=1,
    )

    num_samples = min(output_with_reset.shape[1], original_output.shape[1])

    np.testing.assert_allclose(original_output[:, :num_samples], output_with_reset[:, :num_samples])


@pytest.mark.parametrize("sample_rate", [123.45, 8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [123.45, 8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_input_latency(sample_rate: float, target_sample_rate: float, quality: Resample.Quality):
    resampler = StreamResampler(sample_rate, target_sample_rate, 1, quality)
    assert (
        resampler.process(np.random.rand(int(resampler.input_latency)).astype(np.float32)).shape[1]
        == 0
    )
    np.testing.assert_allclose(
        resampler.process().shape[1],
        resampler.input_latency * target_sample_rate / sample_rate,
        atol=1.5,
    )
