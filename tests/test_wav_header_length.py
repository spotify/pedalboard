"""Finite WAV streams must never expose samples beyond their real contents."""

import struct
from io import BytesIO
from pathlib import Path
from typing import BinaryIO, Optional

import numpy as np
import pytest

from pedalboard import Resample
from pedalboard.io import AudioFile

SAMPLE_RATE = 48_000
NUM_CHANNELS = 2
NUM_FRAMES = 4_800
TARGET_SAMPLE_RATE = 22_050
TARGET_NUM_FRAMES = 2_205


def _chunk(tag, payload, declared_size=None):
    return (
        tag
        + struct.pack("<I", len(payload) if declared_size is None else declared_size)
        + payload
        + (b"\0" if len(payload) % 2 else b"")
    )


def _fmt(*, tag=1, bits=16, block_align=None, guid: Optional[bytes] = None):
    alignment = NUM_CHANNELS * ((bits + 7) // 8) if block_align is None else block_align
    result = struct.pack(
        "<HHIIHH", tag, NUM_CHANNELS, SAMPLE_RATE, SAMPLE_RATE * alignment, alignment, bits
    )
    if tag == 0xFFFE:
        assert guid is not None
        result += struct.pack("<HHI", 22, bits, 3) + guid
    return result


def _wav(
    payload, *, fmt=None, data_size=None, riff_size=None, before=b"", after=b"", rf64_size=None
):
    chunks = (
        before
        + _chunk(b"fmt ", _fmt() if fmt is None else fmt)
        + _chunk(b"data", payload, data_size)
        + after
    )
    if rf64_size is not None:
        ds64 = _chunk(
            b"ds64", struct.pack("<QQQI", len(chunks) + 40, rf64_size, len(payload) // 4, 0)
        )
        return b"RF64\xff\xff\xff\xffWAVE" + ds64 + chunks
    return (
        b"RIFF"
        + struct.pack("<I", len(chunks) + 4 if riff_size is None else riff_size)
        + b"WAVE"
        + chunks
    )


def _pcm16(frames):
    return struct.pack("<hh", 8192, -8192) * frames


def _varying_pcm16():
    return b"".join(
        struct.pack("<hh", 3 * frame - 7200, 6000 - 2 * frame) for frame in range(NUM_FRAMES)
    )


@pytest.fixture(params=["path", "bytes_io", "offset_bytes_io", "callback_stream"])
def wav_source(request, tmp_path):
    def make_source(contents):
        if request.param == "path":
            path = tmp_path / "header-length.wav"
            path.write_bytes(contents)
            return str(path)
        if request.param == "callback_stream":
            return _BoundedStream(contents, operation_budget=2048)
        if request.param == "offset_bytes_io":
            stream = BytesIO(b"prefix!" + contents)
            stream.seek(7)
            return stream
        return BytesIO(contents)

    return make_source


@pytest.mark.parametrize("oversized", [False, True], ids=["healthy", "oversized-data"])
def test_wav_length_reads_and_seeks_stop_at_real_frames(wav_source, oversized):
    contents = _wav(_varying_pcm16(), data_size=0xFFFFFFFF if oversized else None)
    with AudioFile(wav_source(contents)) as audio:
        assert audio.frames == NUM_FRAMES
        assert audio.duration == pytest.approx(0.1)
        samples = audio.read(2 * NUM_FRAMES)
        assert samples.shape == (2, NUM_FRAMES)
        expected_raw = np.array(
            [3 * np.arange(NUM_FRAMES) - 7200, 6000 - 2 * np.arange(NUM_FRAMES)],
            dtype=np.int16,
        )
        np.testing.assert_allclose(samples, expected_raw / 32767, atol=1e-7)
        assert audio.read(1).shape == (2, 0)
        audio.seek(NUM_FRAMES - 2)
        # Frames 4798 and 4799 have distinct, independently calculated PCM values.
        expected_tail = np.array([[7194, 7197], [-3596, -3598]]) / 32767
        np.testing.assert_allclose(audio.read(8), expected_tail, atol=1e-7)
        audio.seek(NUM_FRAMES)
        assert audio.read(1).shape == (2, 0)
        audio.seek(0)
        raw = audio.read_raw(2 * NUM_FRAMES)
        assert raw.shape == (2, NUM_FRAMES)
        np.testing.assert_array_equal(raw, expected_raw)
        assert audio.read_raw(1).shape == (2, 0)
        audio.seek(NUM_FRAMES - 2)
        np.testing.assert_array_equal(audio.read_raw(8), [[7194, 7197], [-3596, -3598]])
        assert audio.read_raw(1).shape == (2, 0)


def test_oversized_wav_resampling_matches_healthy_control(wav_source):
    decoded = []
    for size in (None, 0xFFFFFFFF):
        with AudioFile(wav_source(_wav(_varying_pcm16(), data_size=size))).resampled_to(
            TARGET_SAMPLE_RATE, Resample.Quality.WindowedSinc256
        ) as audio:
            assert audio.frames == TARGET_NUM_FRAMES
            assert audio.duration == pytest.approx(0.1)
            samples = audio.read(2 * TARGET_NUM_FRAMES)
            assert samples.shape == (2, TARGET_NUM_FRAMES)
            assert audio.read(1).shape == (2, 0)
            audio.seek(TARGET_NUM_FRAMES - 3)
            # The varying signal makes an earlier seek return different values.
            assert np.ptp(samples[0, -3:]) > 1e-5
            np.testing.assert_allclose(audio.read(10), samples[:, -3:], atol=1e-6)
            decoded.append(samples)
    np.testing.assert_array_equal(*decoded)


def test_oversized_wav_resample_to_wav_roundtrip_is_bounded(wav_source):
    output = BytesIO()
    with AudioFile(wav_source(_wav(_pcm16(NUM_FRAMES), data_size=0xFFFFFFFF))).resampled_to(
        TARGET_SAMPLE_RATE, Resample.Quality.WindowedSinc256
    ) as reader:
        with AudioFile(output, "w", TARGET_SAMPLE_RATE, 2, format="wav") as writer:
            for _ in range(12):
                samples = reader.read(256)
                if not samples.shape[1]:
                    break
                writer.write(samples)
            else:
                pytest.fail("resampling did not terminate within the real audio duration")
    assert output.getbuffer().nbytes < 20_000
    output.seek(0)
    with AudioFile(output) as audio:
        assert audio.frames == TARGET_NUM_FRAMES
        assert audio.duration == pytest.approx(0.1)
        samples = audio.read(2 * TARGET_NUM_FRAMES)
        assert samples.shape == (2, TARGET_NUM_FRAMES)
        assert np.max(np.abs(samples)) > 0.2


@pytest.mark.parametrize(
    "case",
    [
        "riff",
        "data",
        "both",
        "odd-junk",
        "partial-frame",
        "short-data-list",
        "rf64",
        "oversized-rf64",
    ],
)
def test_wav_chunk_boundaries_preserve_only_real_complete_frames(wav_source, case):
    payload = _pcm16(6)
    options = {}
    if case in ("riff", "both"):
        options["riff_size"] = 0xFFFFFFF0
    if case in ("data", "both", "odd-junk", "partial-frame"):
        options["data_size"] = 0xFFFFFFF0
    if case == "odd-junk":
        options["before"] = _chunk(b"JUNK", b"x")
    if case == "partial-frame":
        payload += b"\x12\x34"
    if case == "short-data-list":
        options["after"] = _chunk(b"LIST", b"INFO" + b"metadata")
    if case in ("rf64", "oversized-rf64"):
        options.update(data_size=0xFFFFFFFF, rf64_size=24 if case == "rf64" else 0x100000000)
    with AudioFile(wav_source(_wav(payload, **options))) as audio:
        assert audio.frames == 6
        assert audio.duration == pytest.approx(6 / SAMPLE_RATE)
        samples = audio.read(20)
        assert samples.shape == (2, 6)
        np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
        assert audio.read(1).shape == (2, 0)


def test_oversized_data_with_trailing_chunk_remains_physically_bounded(wav_source):
    contents = _wav(_pcm16(6), data_size=0xFFFFFFFF, after=_chunk(b"LIST", b"INFOmetadata"))
    # With a corrupt data size the trailing bytes cannot be distinguished from PCM.
    with AudioFile(wav_source(contents)) as audio:
        assert audio.frames == 11
        samples = audio.read(32)
        assert samples.shape == (2, 11)
        assert audio.read(1).shape == (2, 0)


@pytest.mark.parametrize("block_align", [2, 8])
def test_wav_inconsistent_block_alignment_uses_actual_pcm_frame_width(wav_source, block_align):
    with AudioFile(
        wav_source(_wav(_pcm16(6), fmt=_fmt(block_align=block_align), data_size=0xFFFFFFFF))
    ) as audio:
        assert audio.frames == 6
        samples = audio.read(20)
        assert samples.shape == (2, 6)
        np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
        np.testing.assert_allclose(samples[1], -0.25, atol=1 / 32767)


_PCM_GUID = bytes.fromhex("0100000000001000800000aa00389b71")
_FLOAT_GUID = bytes.fromhex("0300000000001000800000aa00389b71")
_AMBISONIC_PCM_GUID = bytes.fromhex("010000002107d3118644c8c1ca000000")
_AMBISONIC_FLOAT_GUID = bytes.fromhex("030000002107d3118644c8c1ca000000")


@pytest.mark.parametrize(
    "encoding", ["pcm24", "float32", "extensible-pcm", "extensible-float", "ambisonic-pcm"]
)
def test_oversized_wav_supported_sample_formats_match_healthy_control(wav_source, encoding):
    if encoding == "pcm24":
        fmt = _fmt(bits=24)
        payload = bytes.fromhex("0000200000e0") * 6
    elif encoding == "float32":
        fmt = _fmt(tag=3, bits=32)
        payload = struct.pack("<ff", 0.25, -0.25) * 6
    else:
        is_float = encoding.endswith("float")
        guid = {
            "extensible-pcm": _PCM_GUID,
            "extensible-float": _FLOAT_GUID,
            "ambisonic-pcm": _AMBISONIC_PCM_GUID,
            "ambisonic-float": _AMBISONIC_FLOAT_GUID,
        }[encoding]
        fmt = _fmt(tag=0xFFFE, bits=32 if is_float else 16, guid=guid)
        payload = struct.pack("<ff", 0.25, -0.25) * 6 if is_float else _pcm16(6)
    decoded = []
    for size in (None, 0xFFFFFFFF):
        with AudioFile(wav_source(_wav(payload, fmt=fmt, data_size=size))) as audio:
            assert audio.frames == 6
            samples = audio.read(20)
            assert samples.shape == (2, 6)
            np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
            np.testing.assert_allclose(samples[1], -0.25, atol=1 / 32767)
            assert audio.read(1).shape == (2, 0)
            decoded.append(samples)
    np.testing.assert_array_equal(*decoded)


@pytest.mark.parametrize("data_size", [None, 0xFFFFFFFF])
def test_wav_unsupported_ambisonic_float_guid_raises(data_size):
    contents = _wav(
        struct.pack("<ff", 0.25, -0.25) * 6,
        fmt=_fmt(tag=0xFFFE, bits=32, guid=_AMBISONIC_FLOAT_GUID),
        data_size=data_size,
    )
    with pytest.raises((RuntimeError, ValueError), match="(?i)(wav|format|unsupported)"):
        AudioFile(BytesIO(contents))


@pytest.mark.parametrize("data_size", [None, 0xFFFFFFFF])
def test_wav_legacy_oversized_bit_depth_preserves_accepted_pcm(data_size):
    # Legacy WAVs can report a nonsensical bit depth while their byte rate and
    # block alignment still describe ordinary stereo PCM16.
    legacy_fmt = struct.pack("<HHIIHH", 1, 2, SAMPLE_RATE, SAMPLE_RATE * 4, 4, 128)
    decoded = []
    for fmt in (_fmt(), legacy_fmt):
        with AudioFile(BytesIO(_wav(_pcm16(6), fmt=fmt, data_size=data_size))) as audio:
            assert audio.frames == 6
            samples = audio.read(20)
            assert samples.shape == (2, 6)
            np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
            decoded.append(samples)
    np.testing.assert_array_equal(*decoded)


class _BoundedStream(BinaryIO):
    """Fail the test instead of hanging if parsing stops making progress."""

    def __init__(self, contents, operation_budget=256):
        self._stream = BytesIO(contents)
        self.remaining_operations = operation_budget

    def _tick(self):
        self.remaining_operations -= 1
        if self.remaining_operations < 0:
            raise AssertionError("WAV parsing exceeded its bounded I/O budget")

    def read(self, size=-1):
        self._tick()
        if size > 1_000_000:
            raise AssertionError("WAV parsing requested a reported oversized payload")
        return self._stream.read(size)

    def seek(self, offset, whence=0):
        self._tick()
        return self._stream.seek(offset, whence)

    def tell(self):
        self._tick()
        return self._stream.tell()

    def seekable(self):
        return True


@pytest.mark.parametrize(
    "malformation",
    ["truncated-chunk-header", "truncated-fmt", "duplicate-fmt", "duplicate-data"]
    + [f"trailing-header-{size}" for size in range(1, 8)]
    + [f"short-extensible-{size}" for size in (17, 18, 24, 39)],
)
def test_malformed_wav_headers_raise_without_hanging(malformation):
    if malformation == "truncated-chunk-header":
        contents = b"RIFF" + struct.pack("<I", 100) + b"WAVEJUNK\x01\x00"
    elif malformation == "truncated-fmt":
        contents = _wav(_pcm16(6), fmt=_fmt()[:12])
    elif malformation == "duplicate-fmt":
        contents = _wav(_pcm16(6), before=_chunk(b"fmt ", _fmt()))
    elif malformation == "duplicate-data":
        contents = _wav(_pcm16(6), after=_chunk(b"data", _pcm16(6)))
    elif malformation.startswith("short-extensible-"):
        length = int(malformation.rsplit("-", 1)[1])
        contents = _wav(_pcm16(6), fmt=_fmt(tag=0xFFFE, guid=_PCM_GUID)[:length])
    else:
        length = int(malformation.rsplit("-", 1)[1])
        contents = _wav(_pcm16(6), after=(b"JUNK" + struct.pack("<I", 0))[:length])
    with pytest.raises(
        (RuntimeError, ValueError), match="(?i)(wav|format|chunk|fmt|data|unsupported)"
    ):
        with AudioFile(_BoundedStream(contents)) as audio:
            audio.read(20)


@pytest.mark.parametrize("failure", ["returns-failure", "does-not-advance"])
def test_wav_failed_seeks_raise_without_hanging(failure):
    class BrokenSeek(_BoundedStream):
        def seek(self, offset, whence=0):
            self._tick()
            # Initial sizing and restoration work; actual forward chunk skips fail.
            current = self.tell()
            if (whence == 1 and offset > 0) or (whence == 0 and offset > current >= 12):
                return -1 if failure == "returns-failure" else current
            return super().seek(offset, whence)

    with pytest.raises(
        (RuntimeError, ValueError, OSError), match="(?i)(wav|format|seek|stream|file|unsupported)"
    ):
        with AudioFile(BrokenSeek(_wav(_pcm16(6), before=_chunk(b"JUNK", b"skip me")))) as audio:
            audio.read(20)


def test_wav_seek_to_end_preserves_original_python_exception():
    class EndSeekFailure(_BoundedStream):
        def seek(self, offset, whence=0):
            if whence == 2:
                raise NotImplementedError("original seek-to-end exception")
            return super().seek(offset, whence)

    with pytest.raises(NotImplementedError, match="original seek-to-end exception"):
        AudioFile(EndSeekFailure(_wav(_pcm16(6), data_size=0xFFFFFFFF)))


@pytest.mark.parametrize("data_size", [None, 0xFFFFFFFF])
def test_ogg_in_wav_uses_compressed_decoder(data_size):
    source = Path(__file__).parent / "audio" / "correct" / "mono_sine_at_48000Hz.ogg"
    with AudioFile(str(source)) as control:
        expected = control.read(256)
    contents = _wav(source.read_bytes(), fmt=_fmt(tag=0x674F), data_size=data_size)
    with AudioFile(BytesIO(contents)) as audio:
        assert audio.frames == 240_000
        assert audio.samplerate == 48_000
        assert audio.num_channels == 1
        np.testing.assert_array_equal(audio.read(256), expected)


@pytest.mark.parametrize("duplicate_chunk", ["fmt", "data"])
@pytest.mark.parametrize("data_size", [0xFFFFFFFF, 4], ids=["sentinel", "short-data-header"])
def test_rf64_sentinel_data_rejects_following_duplicate_chunks(
    wav_source, duplicate_chunk, data_size
):
    trailing = _chunk(b"fmt ", _fmt()) if duplicate_chunk == "fmt" else _chunk(b"data", _pcm16(6))
    contents = _wav(_pcm16(6), data_size=data_size, rf64_size=24, after=trailing)
    with pytest.raises(
        (RuntimeError, ValueError), match="(?i)(wav|format|chunk|fmt|data|unsupported)"
    ):
        with AudioFile(wav_source(contents)) as audio:
            audio.read(20)


@pytest.mark.parametrize("data_size", [None, 0xFFFFFFFF, 4])
def test_rf64_trailing_list_preserves_declared_data_length(wav_source, data_size):
    contents = _wav(
        _pcm16(6),
        data_size=data_size,
        rf64_size=24,
        after=_chunk(b"LIST", b"INFOmetadata"),
    )
    with AudioFile(wav_source(contents)) as audio:
        assert audio.frames == 6
        assert audio.duration == pytest.approx(6 / SAMPLE_RATE)
        samples = audio.read(20)
        assert samples.shape == (2, 6)
        np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
        np.testing.assert_allclose(samples[1], -0.25, atol=1 / 32767)
        assert audio.read(1).shape == (2, 0)


@pytest.mark.parametrize("duplicate_chunk", ["fmt", "data"])
def test_rf64_ignores_duplicate_chunks_outside_declared_container(wav_source, duplicate_chunk):
    contents = _wav(_pcm16(6), data_size=0xFFFFFFFF, rf64_size=24)
    # Appending bytes does not extend the ds64 RIFF size: these bytes are outside
    # the RF64 container and must not become audio or duplicate container chunks.
    contents += _chunk(b"fmt ", _fmt()) if duplicate_chunk == "fmt" else _chunk(b"data", _pcm16(6))
    with AudioFile(wav_source(contents)) as audio:
        assert audio.frames == 6
        assert audio.duration == pytest.approx(6 / SAMPLE_RATE)
        samples = audio.read(20)
        assert samples.shape == (2, 6)
        np.testing.assert_allclose(samples[0], 0.25, atol=1 / 32767)
        np.testing.assert_allclose(samples[1], -0.25, atol=1 / 32767)
        assert audio.read(1).shape == (2, 0)
