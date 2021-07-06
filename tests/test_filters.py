import pytest
import numpy as np
from pedalboard import HighpassFilter, LowpassFilter


def rms(x: np.ndarray) -> float:
    return np.sqrt(np.mean(x ** 2))


def normalized(x: np.ndarray) -> np.ndarray:
    return x / np.amax(np.abs(x))


def db_to_gain(db: float) -> float:
    return 10.0 ** (db / 20.0)


def gain_to_db(gain: float) -> float:
    return 20 * np.log10(gain)


def octaves_between(a_hz: float, b_hz: float) -> float:
    return np.log2(a_hz / b_hz)


@pytest.mark.parametrize('filter_type', [HighpassFilter, LowpassFilter])
@pytest.mark.parametrize('fundamental_hz', [440, 880])
@pytest.mark.parametrize('sample_rate', [22050, 44100, 48000])
def test_filter_attenutation(filter_type, fundamental_hz, sample_rate):
    num_seconds = 10.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    filtered = filter_type(cutoff_frequency_hz=fundamental_hz)(sine_wave, sample_rate)

    # The volume at the cutoff frequency should be -3dB of the input volume
    assert np.allclose(rms(filtered) / rms(sine_wave), db_to_gain(-3), rtol=0.01, atol=0.01)


@pytest.mark.parametrize('cutoff_frequency_hz', [440, 880])
@pytest.mark.parametrize('fundamental_hz', [880, 1760])
@pytest.mark.parametrize('sample_rate', [22050, 44100, 48000])
def test_lowpass_slope(cutoff_frequency_hz, fundamental_hz, sample_rate):
    num_seconds = 1.0
    samples = np.arange(num_seconds * sample_rate)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)

    filtered = LowpassFilter(cutoff_frequency_hz=cutoff_frequency_hz)(sine_wave, sample_rate)

    num_octaves = octaves_between(fundamental_hz, cutoff_frequency_hz)
    # The volume of the fundamental frequency should be (-3dB * number of octaves) of the input volume
    assert np.allclose(
        rms(filtered) / rms(sine_wave), db_to_gain((num_octaves + 1) * -3), rtol=0.1, atol=0.1
    )
