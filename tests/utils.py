import importlib
import os
from functools import lru_cache

import numpy as np
import pytest

# CI's ML framework job sets this so that a missing framework fails instead of skipping.
REQUIRE_ML_FRAMEWORKS = os.environ.get("PEDALBOARD_REQUIRE_ML_FRAMEWORKS") == "1"

ML_FRAMEWORKS = ["torch", "jax", "tensorflow", "numpy"]


def import_framework(name):
    if REQUIRE_ML_FRAMEWORKS:
        return importlib.import_module(name)
    return pytest.importorskip(name)


def make_audio(channels=2, frames=4410, seed=0):
    rng = np.random.default_rng(seed)
    return (rng.uniform(-0.5, 0.5, size=(channels, frames))).astype(np.float32)


def to_framework(arr: np.ndarray, framework: str):
    if framework == "numpy":
        return arr
    if framework == "torch":
        return import_framework("torch").from_numpy(arr.copy())
    if framework == "jax":
        return import_framework("jax.numpy").asarray(arr)
    if framework == "tensorflow":
        return import_framework("tensorflow").constant(arr)
    raise ValueError(framework)


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
