"""Tests for the BrickwallLimiter plugin."""

import numpy as np
import pytest

from pedalboard import BrickwallLimiter

from .utils import db_to_gain, generate_sine_at


@pytest.mark.parametrize(
    "ceiling_db", [0.0, -0.1, -0.3, -1.0, -3.0, -6.0, -12.0, -20.0]
)
@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_ceiling_enforced(ceiling_db: float, sample_rate: float, num_channels: int):
    """Output sample peak must not exceed the ceiling."""
    sine_wave = generate_sine_at(
        sample_rate, 440.0, num_seconds=0.5, num_channels=num_channels
    )
    plugin = BrickwallLimiter(ceiling_db=ceiling_db)
    output = plugin.process(sine_wave, sample_rate)

    assert np.all(np.isfinite(output))
    assert output.shape == sine_wave.shape
    ceiling_linear = db_to_gain(ceiling_db)
    assert (
        np.max(np.abs(output)) <= ceiling_linear + 1e-3
    ), f"Output peak {np.max(np.abs(output)):.6f} exceeds ceiling {ceiling_linear:.6f}"


def test_passthrough_below_ceiling():
    """A signal well below the ceiling should pass through unchanged."""
    quiet_sine = generate_sine_at(44100, 440.0, num_seconds=0.5, num_channels=1) * 0.1
    plugin = BrickwallLimiter(ceiling_db=-1.0)
    output = plugin.process(quiet_sine, 44100)
    np.testing.assert_allclose(output, quiet_sine, atol=1e-6)


def test_no_makeup_gain():
    """Unlike the built-in Limiter, BrickwallLimiter must NOT boost quiet signals."""
    quiet_sine = generate_sine_at(44100, 440.0, num_seconds=0.5, num_channels=1) * 0.1
    plugin = BrickwallLimiter(ceiling_db=-0.3)
    output = plugin.process(quiet_sine, 44100)
    assert np.max(np.abs(output)) <= np.max(np.abs(quiet_sine)) + 1e-6


def test_extreme_levels():
    """Signals well above 0 dBFS (e.g., after loudness normalization) must be limited."""
    sine_wave = generate_sine_at(44100, 440.0, num_seconds=0.5, num_channels=1)
    boosted = sine_wave * db_to_gain(20.0)  # +20 dBFS
    ceiling_db = -1.0
    plugin = BrickwallLimiter(ceiling_db=ceiling_db)
    output = plugin.process(boosted, 44100)
    ceiling_linear = db_to_gain(ceiling_db)
    assert np.max(np.abs(output)) <= ceiling_linear + 1e-3


def test_parameter_validation():
    """Invalid parameter values must raise; valid edge cases must not."""
    limiter = BrickwallLimiter()

    with pytest.raises(Exception):
        limiter.release_ms = 0
    with pytest.raises(Exception):
        limiter.release_ms = -1
    with pytest.raises(Exception):
        limiter.ceiling_db = float("nan")
    with pytest.raises(Exception):
        limiter.ceiling_db = float("inf")

    # Valid edge cases — must NOT raise
    limiter.ceiling_db = 0.0  # 0 dBFS ceiling
    limiter.ceiling_db = 3.0  # positive headroom
    limiter.ceiling_db = -40.0  # very quiet ceiling
