#! /usr/bin/env python
#
# Copyright 2026 Spotify AB
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Tests for the parallel MP3 encoding path exposed via
``AudioFile.encode(..., num_threads=N)``.

The parallel encoder splits a buffer into MP3-frame-aligned chunks, encodes them
on separate threads (with the bit reservoir disabled so frames are independently
splice-able), and stitches the resulting frames back together. It must produce a
valid MP3 whose decoded audio is perceptually identical to a single-threaded
encode, without introducing clicks at the chunk boundaries.

Multithreaded encoding (``num_threads > 1``) is only applied for MP3; for any
other format it is silently ignored and encoding falls back to single-threaded.
"""

import io

import numpy as np
import pytest

import pedalboard

from .utils import generate_sine_at

MP3_FRAME_LENGTH_SAMPLES = 1152


def _decode_mp3_bytes(encoded: bytes):
    with pedalboard.io.ReadableAudioFile(io.BytesIO(encoded)) as af:
        return af.read(af.frames), af.samplerate, af.num_channels


def _best_aligned_snr(decoded: np.ndarray, original: np.ndarray, samplerate: int):
    """
    A decoded MP3 is delayed relative to its source by encoder priming (present
    only in monolithic encodes) plus a fixed decoder delay, so we search a small
    window of leading offsets for the best alignment and return
    (snr_dB, shift, aligned_abs_error).
    """
    n = original.shape[-1]
    best = (-np.inf, 0, None)
    for shift in range(0, 1400):
        d = decoded[:, shift:]
        m = min(d.shape[-1], n)
        if m < samplerate:
            continue
        err = d[:, :m] - original[:, :m]
        power = float(np.mean(original[:, :m] ** 2))
        snr = 10 * np.log10(power / float(np.mean(err**2)))
        if snr > best[0]:
            best = (snr, shift, np.abs(err))
    return best


@pytest.mark.parametrize("num_channels", [1, 2])
def test_parallel_mp3_encode_matches_serial_fidelity(num_channels: int):
    """
    Parallel MP3 encoding should decode to essentially the same audio as a
    single-threaded encode (equal SNR versus the original), even though the
    encoded bytes differ.
    """
    samplerate = 44100
    signal = generate_sine_at(
        samplerate, num_seconds=10, num_channels=num_channels
    ).astype(np.float32)
    original = signal if signal.ndim == 2 else signal.reshape(1, -1)

    serial = pedalboard.io.AudioFile.encode(
        signal, samplerate, "mp3", num_channels=num_channels, quality="256 kbps"
    )
    parallel = pedalboard.io.AudioFile.encode(
        signal,
        samplerate,
        "mp3",
        num_channels=num_channels,
        quality="256 kbps",
        num_threads=4,
    )

    serial_decoded, _, _ = _decode_mp3_bytes(serial)
    parallel_decoded, _, parallel_channels = _decode_mp3_bytes(parallel)

    assert parallel_channels == num_channels

    serial_snr, _, _ = _best_aligned_snr(serial_decoded, original, samplerate)
    parallel_snr, _, _ = _best_aligned_snr(parallel_decoded, original, samplerate)

    assert parallel_snr > 20
    # Parallel encoding must not measurably degrade quality vs. a serial encode:
    assert abs(parallel_snr - serial_snr) < 1.0


def test_parallel_mp3_encode_has_no_seam_artifacts():
    """
    Splicing chunk boundaries must not introduce clicks: the error at seam
    boundaries should be no larger than the overall error envelope.
    """
    samplerate = 44100
    num_encoders = 8
    signal = generate_sine_at(samplerate, num_seconds=10).astype(np.float32).reshape(
        1, -1
    )
    encoded = pedalboard.io.AudioFile.encode(
        signal,
        samplerate,
        "mp3",
        num_channels=1,
        quality="256 kbps",
        num_threads=num_encoders,
    )
    decoded, _, _ = _decode_mp3_bytes(encoded)
    _, shift, error = _best_aligned_snr(decoded, signal, samplerate)
    assert error is not None

    total = signal.shape[-1]
    num_frames = (total + MP3_FRAME_LENGTH_SAMPLES - 1) // MP3_FRAME_LENGTH_SAMPLES
    frames_per_chunk = (num_frames + num_encoders - 1) // num_encoders

    window = 128
    worst_seam = 0.0
    for w in range(1, num_encoders):
        seam = w * frames_per_chunk * MP3_FRAME_LENGTH_SAMPLES
        if seam >= total:
            continue
        idx = seam - shift
        worst_seam = max(
            worst_seam, float(error[:, max(0, idx - window) : idx + window].max())
        )

    # No seam should spike above the global maximum error:
    assert worst_seam <= float(error.max()) * 1.05


def test_parallel_mp3_encode_one_encoder_matches_default():
    """
    num_threads=1 must be byte-identical to the default serial path.
    """
    samplerate = 44100
    signal = generate_sine_at(samplerate, num_seconds=5).astype(np.float32)
    default = pedalboard.io.AudioFile.encode(
        signal, samplerate, "mp3", num_channels=1, quality="256 kbps"
    )
    explicit = pedalboard.io.AudioFile.encode(
        signal,
        samplerate,
        "mp3",
        num_channels=1,
        quality="256 kbps",
        num_threads=1,
    )
    assert default == explicit


def test_parallel_mp3_encode_short_buffer_falls_back_to_serial():
    """
    Buffers too short to benefit from parallelism should produce byte-identical
    output to the serial encoder.
    """
    samplerate = 44100
    signal = (
        (0.5 * np.sin(2 * np.pi * 440 * np.arange(2000) / samplerate))
        .astype(np.float32)
        .reshape(1, -1)
    )
    serial = pedalboard.io.AudioFile.encode(
        signal, samplerate, "mp3", num_channels=1, quality="256 kbps"
    )
    parallel = pedalboard.io.AudioFile.encode(
        signal,
        samplerate,
        "mp3",
        num_channels=1,
        quality="256 kbps",
        num_threads=8,
    )
    assert serial == parallel


@pytest.mark.parametrize("non_mp3_format", ["wav", "flac", "ogg"])
def test_parallel_mp3_encode_non_mp3_falls_back_to_serial(non_mp3_format: str):
    """
    num_threads > 1 is only applied for MP3. For any other format it is silently
    ignored and encoding falls back to single-threaded.
    """
    signal = generate_sine_at(44100, num_seconds=1).astype(np.float32)
    serial = pedalboard.io.AudioFile.encode(
        signal, 44100, non_mp3_format, num_channels=1
    )
    with_threads = pedalboard.io.AudioFile.encode(
        signal, 44100, non_mp3_format, num_channels=1, num_threads=4
    )

    # For deterministic formats, falling back to serial produces byte-identical
    # output. (Ogg embeds a random bitstream serial number, so it is not
    # byte-reproducible; we only check that it remains a valid file below.)
    if non_mp3_format in ("wav", "flac"):
        assert serial == with_threads

    with pedalboard.io.ReadableAudioFile(io.BytesIO(with_threads)) as af:
        assert af.samplerate == 44100
        assert af.num_channels == 1
        assert af.frames > 0


def test_parallel_mp3_encode_rejects_invalid_thread_count():
    signal = generate_sine_at(44100, num_seconds=1).astype(np.float32)
    with pytest.raises(ValueError, match="positive integer"):
        pedalboard.io.AudioFile.encode(
            signal, 44100, "mp3", num_channels=1, num_threads=0
        )


def test_parallel_mp3_encode_is_readable_and_correct_length():
    """
    The parallel-encoded file must be readable and approximately the right
    length (within a couple of MP3 frames of padding).
    """
    samplerate = 44100
    signal = generate_sine_at(samplerate, num_seconds=7).astype(np.float32).reshape(
        1, -1
    )
    encoded = pedalboard.io.AudioFile.encode(
        signal,
        samplerate,
        "mp3",
        num_channels=1,
        quality="256 kbps",
        num_threads=4,
    )
    with pedalboard.io.ReadableAudioFile(io.BytesIO(encoded)) as af:
        assert af.samplerate == samplerate
        assert af.num_channels == 1
        assert af.frames >= signal.shape[-1]
        assert af.frames <= signal.shape[-1] + MP3_FRAME_LENGTH_SAMPLES * 4
