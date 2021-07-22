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


import os
import pytest
import numpy as np
from pedalboard import process, Distortion, Gain, Compressor, Convolution


IMPULSE_RESPONSE_PATH = os.path.join(os.path.dirname(__file__), "impulse_response.wav")


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_no_transforms(shape, sr=44100):
    _input = np.random.rand(*shape).astype(np.float32)

    output = process(_input, sr, [])

    assert _input.shape == output.shape
    assert np.allclose(_input, output, rtol=0.0001)


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_noise_gain(shape, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)

    # Use the Gain transform to scale down the noise by 6dB (0.5x)
    half_noise = process(full_scale_noise, sr, [Gain(-6)])

    assert full_scale_noise.shape == half_noise.shape
    assert np.allclose(full_scale_noise / 2.0, half_noise, rtol=0.01)


def test_throw_on_invalid_compressor_ratio(sr=44100):
    full_scale_noise = np.random.rand(sr, 1).astype(np.float32)

    # Should work:
    process(full_scale_noise, sr, [Compressor(ratio=1.1)])

    # Should fail:
    with pytest.raises(ValueError):
        Compressor(ratio=0.1)


def test_convolution_file_exists():
    """
    A meta-test - if this fails, we can't find the file, so the following two tests will fail!
    """
    assert os.path.isfile(IMPULSE_RESPONSE_PATH)


def test_convolution_works(sr=44100, duration=10):
    full_scale_noise = np.random.rand(sr * duration).astype(np.float32)

    result = process(full_scale_noise, sr, [Convolution(IMPULSE_RESPONSE_PATH, 0.5)])
    assert not np.allclose(full_scale_noise, result, rtol=0.1)


def test_throw_on_inaccessible_convolution_file():
    # Should work:
    Convolution(IMPULSE_RESPONSE_PATH)

    # Should fail:
    with pytest.raises(RuntimeError):
        Convolution("missing_impulse_response.wav")


@pytest.mark.parametrize("gain_db", [-12, -6, 0, 6, 12, 24, 48, 96])
@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_distortion(gain_db, shape, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)

    # Use the Distortion transform with ±0dB, which should change nothing:
    result = process(full_scale_noise, sr, [Distortion(gain_db)])

    np.testing.assert_equal(result.shape, full_scale_noise.shape)
    gain_scale = 10.0 ** (gain_db / 20.0)
    assert np.allclose(np.tanh(full_scale_noise * gain_scale), result)
