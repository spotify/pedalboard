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


import pytest
import numpy as np
from pedalboard import HighpassFilter, LowpassFilter, HighShelfFilter, LowShelfFilter, PeakFilter
from .utils import generate_sine_at


def rms(x: np.ndarray) -> float:
    return np.sqrt(np.mean(x**2))


def normalized(x: np.ndarray) -> np.ndarray:
    return x / np.amax(np.abs(x))


def db_to_gain(db: float) -> float:
    return 10.0 ** (db / 20.0)


def gain_to_db(gain: float) -> float:
    return 20 * np.log10(gain)


def octaves_between(a_hz: float, b_hz: float) -> float:
    return np.log2(a_hz / b_hz)


@pytest.mark.parametrize("filter_type", [HighpassFilter, LowpassFilter])
@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_filter_attenutation(filter_type, fundamental_hz, sample_rate):
    num_seconds = 10.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    filtered = filter_type(cutoff_frequency_hz=fundamental_hz)(sine_wave, sample_rate)

    # The volume at the cutoff frequency should be -3dB of the input volume
    assert np.allclose(rms(filtered) / rms(sine_wave), db_to_gain(-3), rtol=0.01, atol=0.01)


@pytest.mark.parametrize("cutoff_frequency_hz", [440, 880])
@pytest.mark.parametrize("fundamental_hz", [880, 1760])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_lowpass_slope(cutoff_frequency_hz, fundamental_hz, sample_rate):
    num_seconds = 1.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)

    filtered = LowpassFilter(cutoff_frequency_hz=cutoff_frequency_hz)(sine_wave, sample_rate)

    num_octaves = octaves_between(fundamental_hz, cutoff_frequency_hz)
    # The volume of the fundamental frequency should
    # be (-3dB * number of octaves) of the input volume
    assert np.allclose(
        rms(filtered) / rms(sine_wave), db_to_gain((num_octaves + 1) * -3), rtol=0.1, atol=0.1
    )


@pytest.mark.parametrize("filter_type", [HighShelfFilter, LowShelfFilter])
@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("gain_db", [-12, -6, 0, 6, 12])
def test_shelf_filters(filter_type, fundamental_hz, sample_rate, gain_db):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_seconds=2)
    filtered = filter_type(cutoff_frequency_hz=fundamental_hz, gain_db=gain_db)(
        sine_wave, sample_rate
    )
    np.testing.assert_allclose(
        rms(filtered) / rms(sine_wave), db_to_gain(gain_db / 2), rtol=0.01, atol=0.01
    )


@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("gain_db", [-12, -6, 0, 6, 12])
def test_peak_filter(fundamental_hz, sample_rate, gain_db):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_seconds=2)
    filtered = PeakFilter(cutoff_frequency_hz=fundamental_hz, gain_db=gain_db)(
        sine_wave, sample_rate
    )
    np.testing.assert_allclose(
        rms(filtered) / rms(sine_wave), db_to_gain(gain_db), rtol=0.01, atol=0.01
    )


@pytest.mark.parametrize("filter_type", [HighShelfFilter, LowShelfFilter])
@pytest.mark.parametrize("fundamental_hz", [440, 880])
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("gain_db", [-12, -6, 0, 6, 12])
@pytest.mark.parametrize("q", [1 / np.sqrt(2), 1, 100])
def test_q_factor(filter_type, fundamental_hz, sample_rate, gain_db, q):
    sine_wave = generate_sine_at(sample_rate, fundamental_hz, num_seconds=2)

    cutoff_frequency_hz = (
        (fundamental_hz / 4) if filter_type == HighShelfFilter else (fundamental_hz * 4)
    )

    filtered = filter_type(cutoff_frequency_hz=cutoff_frequency_hz, gain_db=gain_db, q=q)(
        sine_wave, sample_rate
    )
    np.testing.assert_allclose(
        rms(filtered) / rms(sine_wave), db_to_gain(gain_db), rtol=0.1, atol=0.1
    )
