import numpy as np


TEST_SINE_WAVE_CACHE = {}


def generate_sine_at(
    sample_rate: float,
    fundamental_hz: float = 440.0,
    num_seconds: float = 3.0,
    num_channels: int = 1,
) -> np.ndarray:
    cache_key = "-".join([str(x) for x in [sample_rate, fundamental_hz, num_seconds, num_channels]])
    if cache_key not in TEST_SINE_WAVE_CACHE:
        samples = np.arange(num_seconds * sample_rate)
        sine_wave = np.sin(2 * np.pi * fundamental_hz * samples / sample_rate)
        # Fade the sine wave in at the start and out at the end to remove any transients:
        fade_duration = int(sample_rate * 0.1)
        sine_wave[:fade_duration] *= np.linspace(0, 1, fade_duration)
        sine_wave[-fade_duration:] *= np.linspace(1, 0, fade_duration)
        if num_channels == 2:
            TEST_SINE_WAVE_CACHE[cache_key] = np.stack([sine_wave, sine_wave])
        else:
            TEST_SINE_WAVE_CACHE[cache_key] = sine_wave
    return TEST_SINE_WAVE_CACHE[cache_key]
