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
@pytest.mark.parametrize("buffer_size", [4, 256, 8192, 1_000_000])
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

    np.testing.assert_allclose(original_output, output_with_reset)


@pytest.mark.parametrize("sample_rate", [123.45, 8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [123.45, 8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_input_latency(sample_rate: float, target_sample_rate: float, quality: Resample.Quality):
    resampler = StreamResampler(sample_rate, target_sample_rate, 1, quality)
    _input = np.random.rand(int(resampler.input_latency)).astype(np.float32)
    outputs = [resampler.process(_input), resampler.process(), resampler.process()]
    assert outputs[0].shape[1] == 0
    assert outputs[1].shape[1] <= np.ceil(
        (resampler.input_latency / sample_rate) * target_sample_rate
    )


@pytest.mark.parametrize("sample_rate", [123.45, 8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [123.45, 8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_flush(sample_rate: float, target_sample_rate: float, quality: Resample.Quality):
    resampler = StreamResampler(sample_rate, target_sample_rate, 1, quality)
    _input = np.random.rand(int(sample_rate)).astype(np.float32)
    # Accept input...
    first_output = resampler.process(_input)
    # ...then flush the resampler:
    resampler.process()
    # ...then make sure that nothing else comes out after a second flush:
    assert resampler.process().shape[-1] == 0
    # Then allow more input to be processed:
    second_output = resampler.process(_input)

    np.testing.assert_allclose(first_output, second_output)


@pytest.mark.parametrize("sample_rate", [123.45, 8000, 11025, 22050, 44100, 48000])
@pytest.mark.parametrize("target_sample_rate", [123.45, 8000, 11025, 12345.67, 22050, 44100, 48000])
@pytest.mark.parametrize("chunk_size", [1, 4, 256, 8192, 1_000_000])
@pytest.mark.parametrize("quality", TOLERANCE_PER_QUALITY.keys())
def test_returned_sample_count(
    sample_rate: float, target_sample_rate: float, chunk_size: int, quality
) -> np.ndarray:
    input_signal = np.linspace(0, sample_rate, 3, dtype=np.float32)
    resampler = StreamResampler(sample_rate, target_sample_rate, 1, quality)

    print("input", input_signal)
    expected_output = np.concatenate(
        [resampler.process(input_signal), resampler.process(None)], axis=-1
    )

    resampler.reset()
    outputs = []
    for i in range(0, input_signal.shape[-1], chunk_size):
        outputs.append(resampler.process(input_signal[..., i : i + chunk_size]))
    outputs.append(resampler.process(None))

    output = np.concatenate(outputs, axis=1)

    for i, (e, a) in enumerate(zip(expected_output[0], output[0])):
        assert e == a, (
            f"First mismatch at index {i}:\nExpected: [..., {expected_output[0][i - 2: i + 2]},"
            f" ...]\nActual:   [..., {output[0][i - 2: i + 2]}, ...]"
        )

    assert output.shape[1] == expected_output.shape[1], (
        f"{output.shape[1]:,} samples were output by resampler (in chunks:"
        f" {[o.shape[1] for o in outputs]}) when {expected_output.shape[1]:,} were expected."
    )
