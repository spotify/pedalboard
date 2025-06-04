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


import numpy as np
from pedalboard import Bitcrush
import pytest


def generate_sine_wave(frequency=440, duration=1.0, sample_rate=44100):
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    return 0.5 * np.sin(2 * np.pi * frequency * t).astype(np.float32)


def test_bitcrush_basic_application():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=8)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape
    assert not np.array_equal(audio, processed_audio), "Audio should be altered by bitcrushing"


def test_bitcrush_extreme_low_bit_depth():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=1)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape
    assert not np.array_equal(audio, processed_audio), "Bitcrush with bit depth 1 should drastically alter the signal"


def test_bitcrush_max_bit_depth():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=24)

    processed_audio = effect(audio, 44100)

    # With bit depth this high, the result should be close to original
    assert np.allclose(audio, processed_audio, atol=1e-2), "Bit depth of 24 should yield near-original audio"


def test_invalid_bit_depth_raises_exception():
    with pytest.raises(ValueError):
        Bitcrush(bit_depth=-5)
    with pytest.raises(ValueError):
        Bitcrush(bit_depth=100)

    with pytest.raises(ValueError):
        Bitcrush(bit_depth=None)
def test_bitcrush_with_different_sample_rates():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=8)

    # Test with different sample rates
    for sample_rate in [22050, 44100, 48000]:
        processed_audio = effect(audio, sample_rate)
        assert processed_audio.shape == audio.shape, f"Processed audio shape mismatch for sample rate {sample_rate}"
        assert not np.array_equal(audio, processed_audio), "Audio should be altered by bitcrushing at different sample rates"
def test_bitcrush_with_zero_length_audio():
    audio = np.array([], dtype=np.float32)
    effect = Bitcrush(bit_depth=8)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape, "Processed audio should have the same shape as input for zero-length audio"
    assert len(processed_audio) == 0, "Processed audio should be empty for zero-length input"
def test_bitcrush_with_non_audio_data():
    # Test with random non-audio data
    audio = np.random.rand(1000).astype(np.float32)  # Random data
    effect = Bitcrush(bit_depth=8)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape, "Processed audio shape should match input for non-audio data"
    assert not np.array_equal(audio, processed_audio), "Non-audio data should still be altered by bitcrushing"
def test_bitcrush_with_different_bit_depths():
    audio = generate_sine_wave()
    bit_depths = [1, 4, 8, 16, 24]

    for bit_depth in bit_depths:
        effect = Bitcrush(bit_depth=bit_depth)
        processed_audio = effect(audio, 44100)

        assert processed_audio.shape == audio.shape, f"Processed audio shape mismatch for bit depth {bit_depth}"
        assert not np.array_equal(audio, processed_audio), f"Audio should be altered by bitcrushing at bit depth {bit_depth}"
def test_bitcrush_with_high_sample_rate():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=8)

    # Test with a high sample rate
    sample_rate = 96000
    processed_audio = effect(audio, sample_rate)

    assert processed_audio.shape == audio.shape, "Processed audio shape should match input for high sample rate"
    assert not np.array_equal(audio, processed_audio), "Audio should be altered by bitcrushing at high sample rate"
def test_bitcrush_with_low_sample_rate():
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=8)

    # Test with a low sample rate
    sample_rate = 16000
    processed_audio = effect(audio, sample_rate)

    assert processed_audio.shape == audio.shape, "Processed audio shape should match input for low sample rate"
    assert not np.array_equal(audio, processed_audio), "Audio should be altered by bitcrushing at low sample rate"
def test_bitcrush_with_stereo_audio():
    # Generate stereo audio (two channels)
    audio = np.vstack((generate_sine_wave(), generate_sine_wave(frequency=880))).T
    effect = Bitcrush(bit_depth=8)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape, "Processed stereo audio shape should match input"
    assert not np.array_equal(audio, processed_audio), "Stereo audio should be altered by bitcrushing"
def test_bitcrush_with_mono_audio():
    # Generate mono audio (single channel)
    audio = generate_sine_wave()
    effect = Bitcrush(bit_depth=8)

    processed_audio = effect(audio, 44100)

    assert processed_audio.shape == audio.shape, "Processed mono audio shape should match input"
    assert not np.array_equal(audio, processed_audio), "Mono audio should be altered by bitcrushing"