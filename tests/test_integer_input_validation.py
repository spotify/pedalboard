#! /usr/bin/env python
#
# Copyright 2025 Spotify AB
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


import numpy as np
import pytest

import pedalboard
from pedalboard import Gain, Pedalboard

from .utils import generate_sine_at

SAMPLE_RATE = 44100


def _int16_scaled_as_float(dtype) -> np.ndarray:
    """A sine wave that was scaled to the int16 range but left as floating point.

    This mimics the most common version of this mistake: calling something like
    ``(audio * 32767).astype(np.int16).astype(np.float32)`` and passing the
    result straight into Pedalboard without rescaling back to [-1.0, 1.0].
    """
    sine_wave = generate_sine_at(SAMPLE_RATE, num_seconds=0.1)
    return (sine_wave * 32767).astype(np.int16).astype(dtype)


@pytest.mark.parametrize("dtype", [np.float32, np.float64])
def test_unscaled_integer_input_raises_descriptive_error(dtype):
    unscaled = _int16_scaled_as_float(dtype)
    with pytest.raises(ValueError, match="integer-valued"):
        Pedalboard([Gain()])(unscaled, SAMPLE_RATE)


@pytest.mark.parametrize("dtype", [np.float32, np.float64])
def test_unscaled_integer_input_raises_on_single_plugin(dtype):
    unscaled = _int16_scaled_as_float(dtype)
    with pytest.raises(ValueError, match=r"\[-1.0, 1.0\]"):
        Gain().process(unscaled, SAMPLE_RATE)


@pytest.mark.parametrize("dtype", [np.float32, np.float64])
def test_unscaled_integer_input_raises_on_process_function(dtype):
    unscaled = _int16_scaled_as_float(dtype)
    with pytest.raises(ValueError, match="rescaled"):
        pedalboard.process(unscaled, SAMPLE_RATE, [Gain()])


@pytest.mark.parametrize("num_channels", [1, 2])
def test_correctly_scaled_integer_input_is_accepted(num_channels):
    sine_wave = generate_sine_at(SAMPLE_RATE, num_seconds=0.1, num_channels=num_channels)
    # The correct way to use int16 data: rescale back into [-1.0, 1.0].
    rescaled = (sine_wave * 32767).astype(np.int16).astype(np.float32) / 32768.0
    output = Pedalboard([Gain()])(rescaled, SAMPLE_RATE)
    assert np.all(np.isfinite(output))


@pytest.mark.parametrize(
    "signal",
    [
        pytest.param(generate_sine_at(SAMPLE_RATE, num_seconds=0.1).astype(np.float32), id="sine"),
        pytest.param(np.zeros(1024, dtype=np.float32), id="silence"),
        pytest.param(np.ones(1024, dtype=np.float32), id="full_scale_dc"),
        pytest.param(
            np.sign(generate_sine_at(SAMPLE_RATE, num_seconds=0.1)).astype(np.float32),
            id="full_scale_square_wave",
        ),
        pytest.param(
            (generate_sine_at(SAMPLE_RATE, num_seconds=0.1) * 3.5 + 0.1).astype(np.float32),
            id="hot_non_integer_signal",
        ),
        pytest.param(np.zeros(0, dtype=np.float32), id="empty"),
    ],
)
def test_legitimate_float_audio_is_not_flagged(signal):
    output = Pedalboard([Gain()])(signal, SAMPLE_RATE)
    assert np.all(np.isfinite(output))
