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


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [128, 8192, 65536])
@pytest.mark.parametrize("lookahead_ms", [1.0, 5.0, 10.0])
def test_latency_compensation(
    sample_rate: float, buffer_size: int, lookahead_ms: float
):
    """Output must be time-aligned with input after latency compensation."""
    quiet_sine = (
        generate_sine_at(sample_rate, 440.0, num_seconds=1.0, num_channels=1) * 0.1
    )
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    output = plugin.process(quiet_sine, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, quiet_sine, atol=1e-5)


@pytest.mark.parametrize(
    "lookahead_ms",
    [2.30, 4.79, 5.06, 6.97],
    # At 44100 Hz these truncate to 101, 211, 223, 307 samples (all prime)
)
def test_latency_prime_sizes(lookahead_ms: float):
    """Latency compensation works for prime-number delay lengths."""
    sr = 44100
    expected_samples = int(sr * lookahead_ms / 1000)
    # Verify we actually hit a prime
    assert expected_samples > 1
    assert all(
        expected_samples % i != 0 for i in range(2, int(expected_samples**0.5) + 1)
    ), f"{lookahead_ms} ms at {sr} Hz gives {expected_samples} samples, which is not prime"
    quiet_sine = generate_sine_at(sr, 440.0, num_seconds=0.5, num_channels=1) * 0.1
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    output = plugin.process(quiet_sine, sr)
    np.testing.assert_allclose(output, quiet_sine, atol=1e-5)


def test_transient_hold():
    """Isolated transient must be attenuated by gain envelope, not just jlimit.

    Stereo signal: ch1 has a transient, ch2 is constant DC at -20 dBFS.
    If jlimit clips ch1 without the gain envelope, ch2 is untouched.
    If the gain envelope works, ch2 shows attenuation at the transient's
    delayed exit point (linked gain).
    """
    sr = 44100
    lookahead_ms = 5.0
    lookahead_samples = int(sr * lookahead_ms / 1000)
    n_samples = sr  # 1 second
    transient_pos = n_samples // 2

    # ch1: silence with one transient at +6 dBFS
    ch1 = np.zeros(n_samples, dtype=np.float32)
    ch1[transient_pos] = db_to_gain(6.0)

    # ch2: constant DC at -20 dBFS (not a sine — avoids zero-crossings
    # that make ratio assertions unstable)
    dc_level = db_to_gain(-20.0)
    ch2 = np.full(n_samples, dc_level, dtype=np.float32)

    stereo = np.stack([ch1, ch2])
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    output = plugin.process(stereo, sr)

    ceiling_linear = db_to_gain(-1.0)

    # ch1 transient must be below ceiling
    assert np.max(np.abs(output[0])) <= ceiling_linear + 1e-3

    # ch2 witness: at the delayed exit point of the transient, the gain
    # envelope should be active. The transient is detected at transient_pos
    # and exits the delay line at transient_pos (after latency compensation).
    # Check a few samples around that point on ch2.
    witness_idx = transient_pos  # after latency comp, this is where it exits
    for offset in range(-2, 3):
        idx = witness_idx + offset
        if 0 <= idx < n_samples:
            ratio = output[1, idx] / dc_level
            assert ratio < 0.95, (
                f"ch2 not attenuated at witness sample {idx} "
                f"(ratio={ratio:.4f}). Linked gain envelope not working."
            )


def test_hold_refresh():
    """Two transients: second arrives near end of first's hold period.

    Without hold-refresh, gain releases before the second peak exits
    the delay line. We use a DC witness channel to verify the gain
    stays reduced through both peaks' delayed exits.
    """
    sr = 44100
    lookahead_ms = 5.0
    lookahead_samples = int(sr * lookahead_ms / 1000)
    n_samples = sr  # 1 second

    first_pos = n_samples // 3
    second_pos = first_pos + lookahead_samples - 10

    # ch1: two transients
    ch1 = np.zeros(n_samples, dtype=np.float32)
    ch1[first_pos] = db_to_gain(6.0)
    ch1[second_pos] = db_to_gain(3.0)

    # ch2: constant DC witness at -20 dBFS
    dc_level = db_to_gain(-20.0)
    ch2 = np.full(n_samples, dc_level, dtype=np.float32)

    stereo = np.stack([ch1, ch2])
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    output = plugin.process(stereo, sr)

    ceiling_linear = db_to_gain(-1.0)

    # Neither transient should overshoot
    assert np.max(np.abs(output[0])) <= ceiling_linear + 1e-3

    # ch2 witness around the second peak's delayed exit must be attenuated
    second_exit = second_pos  # after latency compensation
    for offset in range(-2, 3):
        idx = second_exit + offset
        if 0 <= idx < n_samples:
            ratio = output[1, idx] / dc_level
            assert ratio < 0.95, (
                f"ch2 not attenuated at second peak's exit (idx={idx}, "
                f"ratio={ratio:.4f}). Hold-refresh may be broken."
            )


def test_reset_determinism():
    """Processing the same signal twice with reset() between must give identical output."""
    sine = generate_sine_at(44100, 440.0, num_seconds=0.5, num_channels=1)
    plugin = BrickwallLimiter(ceiling_db=-3.0, lookahead_ms=5.0)

    out1 = plugin.process(sine, 44100)
    plugin.reset()
    out2 = plugin.process(sine, 44100)
    np.testing.assert_array_equal(out1, out2)


@pytest.mark.parametrize("num_channels", [1, 2])
def test_streaming_consistency(num_channels: int):
    """Processing in one large block vs many small blocks should match."""
    sine = generate_sine_at(44100, 440.0, num_seconds=1.0, num_channels=num_channels)
    ceiling_db = -3.0

    plugin1 = BrickwallLimiter(ceiling_db=ceiling_db, lookahead_ms=5.0)
    out_single = plugin1.process(sine, 44100)

    plugin2 = BrickwallLimiter(ceiling_db=ceiling_db, lookahead_ms=5.0)
    out_chunked = plugin2.process(sine, 44100, buffer_size=128)

    np.testing.assert_allclose(out_single, out_chunked, atol=1e-4)


def test_stereo_linked():
    """Gain reduction must be linked across channels (stereo image preserved)."""
    sr = 44100
    n = sr // 2
    t = np.arange(n, dtype=np.float32) / sr

    ch1 = np.sin(2 * np.pi * 440 * t).astype(np.float32)  # loud
    ch2 = (np.sin(2 * np.pi * 880 * t) * 0.1).astype(np.float32)  # quiet
    stereo = np.stack([ch1, ch2])

    plugin = BrickwallLimiter(ceiling_db=-6.0, lookahead_ms=5.0)
    output = plugin.process(stereo, sr)

    # ch2 alone wouldn't need reduction, but linked gain should attenuate it
    assert (
        np.max(np.abs(output[1])) < np.max(np.abs(ch2)) - 0.001
    ), "ch2 should be attenuated by linked gain from ch1's peaks"


def test_streaming_reset_false():
    """Multi-call streaming with reset=False must preserve state."""
    sr = 44100
    lookahead_ms = 5.0
    lookahead_samples = int(sr * lookahead_ms / 1000)
    buffer_size = 512  # fixed buffer size throughout

    # Use a quiet signal that passes through unchanged
    full_signal = generate_sine_at(sr, 440.0, num_seconds=1.0, num_channels=1) * 0.1
    flat = full_signal.flatten() if full_signal.ndim > 1 else full_signal

    # Reference: one-shot processing
    plugin_ref = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    ref_output = plugin_ref.process(full_signal, sr, buffer_size=buffer_size)
    ref_flat = ref_output.flatten() if ref_output.ndim > 1 else ref_output

    # Multi-call: split into 3 chunks + tail flush
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=lookahead_ms)
    chunk_size = len(flat) // 3
    chunks = [
        flat[:chunk_size].copy(),
        flat[chunk_size : 2 * chunk_size].copy(),
        flat[2 * chunk_size :].copy(),
    ]

    # All chunks (including the first) use reset=False. Note that reset=True
    # (the process() default) tells pedalboard's process.h that this call is
    # "probably the last" for this audio, which causes it to flush the plugin's
    # entire internal latency by feeding extra silence through it before
    # returning. That's correct for one-shot processing (see plugin_ref above)
    # but would corrupt a multi-call stream: it would prematurely drain the
    # lookahead delay line with synthetic silence, discarding the real
    # audio/state needed to align with the next chunk. A fresh plugin's first
    # prepare() call always resets internal state on its own, so reset=False
    # is safe here even for the very first chunk.
    out1 = plugin.process(chunks[0], sr, buffer_size=buffer_size, reset=False)
    out2 = plugin.process(chunks[1], sr, buffer_size=buffer_size, reset=False)
    out3 = plugin.process(chunks[2], sr, buffer_size=buffer_size, reset=False)

    # Tail flush: feed at least lookahead_samples of silence to drain delay line
    tail = np.zeros(lookahead_samples + buffer_size, dtype=np.float32)
    out_tail = plugin.process(tail, sr, buffer_size=buffer_size, reset=False)

    # Concatenate and verify total length and content
    combined = np.concatenate(
        [out1.flatten(), out2.flatten(), out3.flatten(), out_tail.flatten()]
    )
    # Trim to reference length
    combined = combined[: len(ref_flat)]

    np.testing.assert_allclose(combined, ref_flat, atol=1e-4)


def test_property_mutation_ceiling():
    """Changing ceiling_db takes effect at the next block boundary."""
    sr = 44100
    sine = generate_sine_at(sr, 440.0, num_seconds=0.5, num_channels=1)

    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=5.0)
    out1 = plugin.process(sine, sr)

    plugin.ceiling_db = -6.0
    out2 = plugin.process(sine, sr, reset=False)

    assert np.max(np.abs(out1)) <= db_to_gain(-1.0) + 1e-3
    assert np.max(np.abs(out2)) <= db_to_gain(-6.0) + 1e-3


def test_property_mutation_release():
    """Different release times produce measurably different gain recovery shapes."""
    sr = 44100
    n = sr
    lookahead_ms = 5.0
    lookahead_samples = int(sr * lookahead_ms / 1000)

    # DC signal with one transient — the DC makes gain reduction visible
    dc_level = db_to_gain(-10.0)
    signal_fast = np.full(n, dc_level, dtype=np.float32)
    signal_fast[n // 4] = db_to_gain(6.0)

    signal_slow = signal_fast.copy()

    plugin = BrickwallLimiter(
        ceiling_db=-1.0, release_ms=10.0, lookahead_ms=lookahead_ms
    )
    out_fast = plugin.process(signal_fast, sr)

    plugin.release_ms = 500.0
    plugin.reset()
    out_slow = plugin.process(signal_slow, sr)

    # After the transient + hold period, fast release should recover sooner.
    # Check a point well after the hold expires but before the end.
    check_idx = n // 4 + 2 * lookahead_samples + int(sr * 0.05)  # 50ms after hold
    if check_idx < n:
        # Fast release should be closer to dc_level than slow release
        fast_recovery = out_fast[check_idx] / dc_level
        slow_recovery = out_slow[check_idx] / dc_level
        assert fast_recovery > slow_recovery + 0.01, (
            f"Fast release ({fast_recovery:.4f}) should recover more than "
            f"slow release ({slow_recovery:.4f})"
        )


def test_lookahead_ms_validation():
    """lookahead_ms must be in (0, 100]."""
    limiter = BrickwallLimiter()
    with pytest.raises(Exception):
        limiter.lookahead_ms = -1
    with pytest.raises(Exception):
        limiter.lookahead_ms = 0
    with pytest.raises(Exception):
        limiter.lookahead_ms = float("nan")


def _measure_true_peak_dbfs(audio_1d: np.ndarray) -> float:
    """Independent true-peak measurement via 4x oversampling (numpy-only).

    Uses FFT zero-padding for 4x interpolation — no scipy dependency.
    """
    audio = audio_1d.astype(np.float64)
    n = len(audio)
    if n == 0:
        return float("-inf")
    # FFT-based 4x upsampling via zero-padding in frequency domain
    spectrum = np.fft.rfft(audio)
    target_len = n * 4
    upsampled = np.fft.irfft(spectrum, n=target_len) * 4  # compensate energy
    peak = np.max(np.abs(upsampled))
    if peak == 0:
        return float("-inf")
    return float(20.0 * np.log10(peak))


def test_true_peak_detection():
    """true_peak=True must detect inter-sample peaks that sample-peak mode misses.

    The ISP signal [0, 0, 1, 1, 0, 0] has sample peak = 0 dBFS but
    true peak ≈ +2.1 dBFS. With ceiling_db=0.0:
    - sample-peak mode sees no violation → no gain reduction
    - true-peak mode sees the overshoot → applies gain reduction
    """
    sr = 44100
    n = sr
    signal = np.zeros(n, dtype=np.float32)
    mid = n // 2
    signal[mid : mid + 6] = np.array([0, 0, 1, 1, 0, 0], dtype=np.float32)

    ceiling_db = 0.0  # sample peak exactly at ceiling

    # Sample-peak mode: no violation → output ≈ input
    plugin_sp = BrickwallLimiter(ceiling_db=ceiling_db, true_peak=False)
    out_sp = plugin_sp.process(signal, sr)
    np.testing.assert_allclose(out_sp, signal, atol=0.01)

    # True-peak mode: ISP detected → gain reduction applied
    plugin_tp = BrickwallLimiter(ceiling_db=ceiling_db, true_peak=True)
    out_tp = plugin_tp.process(signal, sr)

    # True-peak output should have reduced sample values
    assert (
        np.max(np.abs(out_tp)) < np.max(np.abs(signal)) - 0.01
    ), "true_peak=True should reduce the ISP signal"

    # Independent measurement: tp output's true peak should be lower
    tp_of_sp = _measure_true_peak_dbfs(out_sp)
    tp_of_tp = _measure_true_peak_dbfs(out_tp)
    assert tp_of_tp < tp_of_sp, (
        f"true_peak output ({tp_of_tp:.2f} dB) should have lower true peak "
        f"than sample_peak output ({tp_of_sp:.2f} dB)"
    )


def test_true_peak_conformance():
    """true_peak mode must not produce WORSE true peaks than sample-peak mode."""
    sr = 44100
    t = np.arange(sr, dtype=np.float32) / sr
    signals = {
        "sine_1k": np.sin(2 * np.pi * 1000 * t).astype(np.float32),
        "near_nyquist": np.sin(2 * np.pi * 0.45 * sr * t).astype(np.float32),
    }

    ceiling_db = -1.0
    for name, sig in signals.items():
        plugin_sp = BrickwallLimiter(ceiling_db=ceiling_db, true_peak=False)
        plugin_tp = BrickwallLimiter(ceiling_db=ceiling_db, true_peak=True)
        out_sp = plugin_sp.process(sig, sr)
        out_tp = plugin_tp.process(sig, sr)

        tp_sp = _measure_true_peak_dbfs(out_sp)
        tp_tp = _measure_true_peak_dbfs(out_tp)

        # true_peak mode must not be worse
        assert tp_tp <= tp_sp, (
            f"{name}: true_peak output ({tp_tp:.2f} dB) worse than "
            f"sample_peak output ({tp_sp:.2f} dB)"
        )


def test_true_peak_across_block_boundary():
    """ISP signal straddling a block boundary should still be detected."""
    sr = 44100
    buffer_size = 128
    n = sr

    signal = np.zeros(n, dtype=np.float32)
    # Place ISP pattern right at a block boundary
    boundary = buffer_size * 5  # 640th sample
    signal[boundary - 1 : boundary + 5] = np.array([0, 0, 1, 1, 0, 0], dtype=np.float32)

    ceiling_db = 0.0
    plugin = BrickwallLimiter(ceiling_db=ceiling_db, true_peak=True)
    output = plugin.process(signal, sr, buffer_size=buffer_size)

    # True-peak mode should still detect and attenuate
    assert np.max(np.abs(output)) < np.max(np.abs(signal)) - 0.01


def test_true_peak_after_reset():
    """True-peak mode works correctly after reset()."""
    sr = 44100
    n = sr
    signal = np.zeros(n, dtype=np.float32)
    mid = n // 2
    signal[mid : mid + 6] = np.array([0, 0, 1, 1, 0, 0], dtype=np.float32)

    plugin = BrickwallLimiter(ceiling_db=0.0, true_peak=True)
    out1 = plugin.process(signal, sr)
    plugin.reset()
    out2 = plugin.process(signal, sr)

    np.testing.assert_array_equal(out1, out2)


def test_property_mutation_lookahead_and_true_peak():
    """Changing lookahead_ms and true_peak requires re-preparation.

    These take effect on the next .process() call (which calls prepare()).
    """
    sr = 44100
    sine = generate_sine_at(sr, 440.0, num_seconds=0.5, num_channels=1) * 0.1

    # Start with lookahead_ms=5.0, true_peak=False
    plugin = BrickwallLimiter(ceiling_db=-1.0, lookahead_ms=5.0, true_peak=False)
    out1 = plugin.process(sine, sr)
    np.testing.assert_allclose(out1, sine, atol=1e-5)

    # Change lookahead_ms — next process() triggers prepare() with new spec
    plugin.lookahead_ms = 10.0
    out2 = plugin.process(sine, sr)  # triggers prepare with new lookahead
    np.testing.assert_allclose(out2, sine, atol=1e-5)  # still passes through

    # Change true_peak — next process() triggers prepare() with oversampler
    plugin.true_peak = True
    out3 = plugin.process(sine, sr)
    np.testing.assert_allclose(out3, sine, atol=1e-5)  # quiet signal still passes


def test_upsampler_latency_calibration():
    """Verify the upsampler delay compensation is self-consistent.

    Feeds a single-sample impulse through a true_peak=True plugin with a
    ceiling far above the impulse's amplitude, so the gain envelope never
    engages and the impulse is unmodified except for the plugin's reported
    latency (base lookahead + upsampler delay). Pedalboard's automatic
    latency compensation should then re-align the impulse to its original
    input index.

    The delay compensation uses ceil(getLatencyInSamples()) — the full
    reported round-trip latency — as a conservative upper bound, since
    JUCE exposes no per-direction latency accessor and the up/down filters
    have different orders. This over-compensates slightly (the delay line
    is a bit longer than strictly needed) but guarantees gain reduction
    always arrives before the peak exits the delay line.

    NOTE: This test validates self-consistency (the impulse lands at the
    right index after latency compensation), not the accuracy of the
    delay split itself — Pedalboard's latency compensation makes any
    self-consistent delay value produce a correctly-aligned impulse.
    """
    sr = 44100
    n = sr
    impulse_pos = n // 2
    signal = np.zeros(n, dtype=np.float32)
    signal[impulse_pos] = 1.0

    # ceiling_db=+20.0 => ceiling well above the impulse's peak (0 dBFS),
    # so no gain reduction occurs; output should be an exact delayed copy
    # of the input, which pedalboard's automatic latency compensation
    # re-aligns to the original sample positions.
    plugin = BrickwallLimiter(ceiling_db=20.0, lookahead_ms=5.0, true_peak=True)
    output = plugin.process(signal, sr)

    out_flat = output.flatten() if output.ndim > 1 else output
    out_peak_idx = int(np.argmax(np.abs(out_flat)))

    assert out_peak_idx == impulse_pos, (
        f"Impulse shifted from input index {impulse_pos} to output index "
        f"{out_peak_idx}; upsampler delay compensation "
        "(ceil(getLatencyInSamples())) appears miscalibrated."
    )
    np.testing.assert_allclose(out_flat[out_peak_idx], 1.0, atol=1e-3)

    # No other sample should carry significant energy — a miscalibrated
    # delay would either misplace the impulse (caught above) or, if the
    # analysis/delay bookkeeping is inconsistent, smear/duplicate it.
    other_samples = np.delete(out_flat, out_peak_idx)
    assert np.max(np.abs(other_samples)) < 0.05, (
        "Impulse energy leaked into neighboring samples; upsampling-only "
        "latency compensation appears miscalibrated."
    )
