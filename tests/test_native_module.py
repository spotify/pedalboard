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

import numpy as np
import pytest

from pedalboard import (
    Bitcrush,
    Chorus,
    Clipping,
    Compressor,
    Convolution,
    Delay,
    Distortion,
    Gain,
    GSMFullRateCompressor,
    HighpassFilter,
    HighShelfFilter,
    Invert,
    LowpassFilter,
    LowShelfFilter,
    MP3Compressor,
    NoiseGate,
    PeakFilter,
    Phaser,
    PitchShift,
    Resample,
    Reverb,
    process,
)
from pedalboard.io import AudioFile

IMPULSE_RESPONSE_PATH = os.path.join(os.path.dirname(__file__), "impulse_response.wav")


ALL_BUILTIN_PLUGINS = [
    Compressor,
    Delay,
    Distortion,
    Gain,
    Invert,
    Reverb,
    Bitcrush,
    Chorus,
    Clipping,
    # Convolution, # Not instantiable with zero arguments
    HighpassFilter,
    LowpassFilter,
    HighShelfFilter,
    LowShelfFilter,
    PeakFilter,
    NoiseGate,
    Phaser,
    PitchShift,
    MP3Compressor,
    GSMFullRateCompressor,
    Resample,
]


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 44100), (2, 44100)])
def test_no_transforms(shape, sr=44100):
    _input = np.random.rand(*shape).astype(np.float32)

    output = process(_input, sr, [])

    assert _input.shape == output.shape
    assert np.allclose(_input, output, rtol=0.0001)


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 44100), (2, 44100)])
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


def test_convolution_works_with_buffer(sr=44100, duration=10):
    full_scale_noise = np.random.rand(sr * duration).astype(np.float32)

    expected = Convolution(IMPULSE_RESPONSE_PATH, 0.5)(full_scale_noise, sr)
    with AudioFile(IMPULSE_RESPONSE_PATH) as f:
        actual = Convolution(f.read(f.frames), 0.5, sample_rate=sr)(full_scale_noise, sr)
    np.testing.assert_allclose(expected, actual, atol=0.0001)


def test_throw_on_inaccessible_convolution_file():
    # Should work:
    Convolution(IMPULSE_RESPONSE_PATH)

    # Should fail:
    with pytest.raises(RuntimeError):
        Convolution("missing_impulse_response.wav")


def test_convolution_repr():
    assert IMPULSE_RESPONSE_PATH in repr(Convolution(IMPULSE_RESPONSE_PATH))
    assert "12345 samples of 2-channel audio at 44100 Hz" in repr(
        Convolution(np.random.rand(12345, 2).astype(np.float32), 0.5, 44100)
    )


def test_convolution_filename():
    conv = Convolution(IMPULSE_RESPONSE_PATH)
    assert conv.impulse_response_filename == IMPULSE_RESPONSE_PATH
    assert conv.impulse_response is None


def test_convolution_impulse_response_storage():
    ir = np.random.rand(2, 12345).astype(np.float32)
    conv = Convolution(ir, 0.5, 44100)
    assert conv.impulse_response_filename is None
    np.testing.assert_allclose(conv.impulse_response, ir)


@pytest.mark.parametrize("gain_db", [-12, -6, 0, 1.1, 6, 12, 24, 48, 96])
@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 44100), (2, 44100)])
def test_distortion(gain_db, shape, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)

    # Use the Distortion transform with ±0dB, which should change nothing:
    result = process(full_scale_noise, sr, [Distortion(gain_db)])

    np.testing.assert_equal(result.shape, full_scale_noise.shape)
    gain_scale = np.power(10.0, 0.05 * gain_db)
    np.testing.assert_allclose(np.tanh(full_scale_noise * gain_scale), result, rtol=4e-7, atol=2e-7)


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 44100), (2, 44100)])
def test_invert(shape, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)
    result = Invert()(full_scale_noise, sr)
    np.testing.assert_allclose(full_scale_noise * -1, result, rtol=4e-7, atol=2e-7)


def test_delay():
    delay_seconds = 2.5
    feedback = 0.0
    mix = 0.5
    duration = 10.0
    sr = 44100

    full_scale_noise = np.random.rand(int(sr * duration)).astype(np.float32)
    result = Delay(delay_seconds, feedback, mix)(full_scale_noise, sr)

    # Manually do what a delay plugin would do:
    dry_volume = 1.0 - mix
    wet_volume = mix

    delayed_line = np.concatenate([np.zeros(int(delay_seconds * sr)), full_scale_noise])[
        : len(result)
    ]
    expected = (dry_volume * full_scale_noise) + (wet_volume * delayed_line)

    np.testing.assert_equal(result.shape, expected.shape)
    np.testing.assert_allclose(expected, result, rtol=4e-7, atol=2e-7)


@pytest.mark.parametrize("reset", (True, False))
def test_plugin_state_not_cleared_between_invocations(reset: bool):
    """
    Ensure that if `reset` is True, we do reset the plugin state
    (i.e.: we cut off reverb tails). If `reset` is False, plugin
    state should be maintained between calls to `render`
    (preserving tails).
    """
    reverb = Reverb()
    sr = 44100
    noise = np.random.rand(sr)
    silence = np.zeros_like(noise)

    # Assert that reverb adds nothing if no signal was present already:
    assert np.amax(np.abs(reverb(silence, sr, reset=reset))) == 0.0

    # Pass in noise followed by silence:
    reverb(noise, sr, reset=reset)
    effected_silence = reverb(silence, sr, reset=reset)
    effected_silence_noise_floor = np.amax(np.abs(effected_silence))

    if reset:
        assert effected_silence_noise_floor == 0.0
    else:
        assert effected_silence_noise_floor > 0.25


def test_plugin_state_not_cleared_if_passed_smaller_buffer():
    """
    Ensure that if `reset` is False, a smaller buffer size can be
    passed without clearing the plugin's internal state:
    """
    reverb = Reverb()
    sr = 44100
    noise = np.random.rand(sr)
    silence = np.zeros_like(noise)

    # Assert that reverb adds nothing if no signal was present already:
    assert np.amax(np.abs(reverb(silence, sr, reset=False))) == 0.0

    # Pass in noise followed by silence, but less silence:
    reverb(noise, sr, reset=False)
    effected_silence = reverb(silence[: int(len(silence) / 2)], sr, reset=False)
    effected_silence_noise_floor = np.amax(np.abs(effected_silence))

    assert effected_silence_noise_floor > 0.25


@pytest.mark.parametrize("plugin_class", ALL_BUILTIN_PLUGINS)
def test_process_differently_shaped_empty_buffers(plugin_class):
    plugin = plugin_class()
    sr = 44100
    assert plugin(np.zeros((0, 1), dtype=np.float32), sr).shape == (0, 1)
    assert plugin(np.zeros((1, 0), dtype=np.float32), sr).shape == (1, 0)
    assert plugin(np.zeros((0,), dtype=np.float32), sr).shape == (0,)


@pytest.mark.parametrize("plugin_class", ALL_BUILTIN_PLUGINS)
def test_process_one_by_one_buffer(plugin_class):
    plugin = plugin_class()
    sr = 44100
    # Processing a single sample at a time should work:
    assert plugin(np.zeros((1, 1), dtype=np.float32), sr).shape == (1, 1)
    # Processing that same sample as a flat 1D array should work too:
    assert plugin(np.zeros((1,), dtype=np.float32), sr).shape == (1,)


@pytest.mark.parametrize("plugin_class", ALL_BUILTIN_PLUGINS)
def test_process_two_by_two_buffer(plugin_class):
    plugin = plugin_class()
    sr = 44100

    # Writing a 2x2 buffer should not work right off the bat, as we
    # can't tell which dimension is channels and which dimension is
    # samples:
    with pytest.raises(RuntimeError) as e:
        plugin(np.zeros((2, 2), dtype=np.float32), sr).shape
    assert "Provide a non-square array first" in str(e)

    # ...but if we write a non-square buffer, it should work:
    output = plugin(np.zeros((2, 1024), dtype=np.float32), sr)
    assert output.shape[0] == 2
    # ...and now square buffers are interpreted as having the same channel layout:
    assert plugin(np.zeros((2, 2), dtype=np.float32), sr).shape[0] == 2


@pytest.mark.parametrize("channel_dimension", [0, 1])
@pytest.mark.parametrize("plugin_class", ALL_BUILTIN_PLUGINS)
def test_process_two_by_two_buffer_with_hint(plugin_class, channel_dimension: int):
    plugin = plugin_class()
    sr = 44100

    empty_shape = (2, 0) if channel_dimension == 0 else (0, 2)

    # ...if we pass an empty array of the right shape, that shape hint should be saved:
    assert plugin(np.zeros(empty_shape, dtype=np.float32), sr).shape == empty_shape
    # ...and now square buffers are interpreted as having the same channel layout:
    output = plugin(np.zeros((2, 2), dtype=np.float32), sr)
    assert output.shape[channel_dimension] == 2

    # Some plugins buffer their output, so make sure we eventually do get something out in the right shape:
    while output.shape[1 if channel_dimension == 0 else 0] == 0:
        output = plugin(np.zeros((2, 2), dtype=np.float32), sr, reset=False)
        assert output.shape[channel_dimension] == 2

    # ...but not if we call reset():
    plugin.reset()
    with pytest.raises(RuntimeError) as e:
        plugin(np.zeros((2, 2), dtype=np.float32), sr).shape
    assert "Provide a non-square array first" in str(e)
