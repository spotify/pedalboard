from functools import lru_cache

import numpy as np

TEST_SINE_WAVE_CACHE = {}


@lru_cache(maxsize=None)
def generate_sine_at(
    sample_rate: float,
    fundamental_hz: float = 440.0,
    num_seconds: float = 1.0,
    num_channels: int = 1,
) -> np.ndarray:
    num_samples = int(num_seconds * sample_rate)
    samples = np.arange(num_samples)
    sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
    # Fade the sine wave in at the start and out at the end to remove any transients:
    fade_duration = min(num_samples // 2, int(sample_rate * 0.1))
    sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
    sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
    if num_channels != 1:
        return np.stack([sine_wave] * num_channels)
    else:
        return sine_wave


def db_to_gain(db: float) -> float:
    return 10.0 ** (db / 20.0)


def gain_to_db(gain: float) -> float:
    return 20 * np.log10(gain)
