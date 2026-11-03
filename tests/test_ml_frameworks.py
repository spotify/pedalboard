#! /usr/bin/env python
#
# Copyright 2026 Spotify AB
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

"""
Tests for pedalboard.torch, pedalboard.jax and pedalboard.tensorflow: drop-in
replacements for pedalboard (and pedalboard.io) that return torch.Tensors /
jax.Arrays / tf.Tensors. Torch-only behaviour lives in test_torch.py.
"""

import importlib
import io
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType
from typing import Any, Callable

import numpy as np
import pytest

import pedalboard
import pedalboard.io

from .utils import ML_FRAMEWORKS, import_framework, make_audio, to_framework

SAMPLE_RATE = 44100
NUM_CHANNELS = 2
NUM_FRAMES = 4410


@dataclass
class OutputFramework:
    name: str
    module: ModuleType  # pedalboard.torch, pedalboard.jax or pedalboard.tensorflow
    is_output_type: Callable[[Any], bool]
    to_numpy: Callable[[Any], np.ndarray]
    concat: Callable[[list], Any]


def _torch_output():
    torch = import_framework("torch")
    module = importlib.import_module("pedalboard.torch")
    return OutputFramework(
        "torch",
        module,
        lambda x: isinstance(x, torch.Tensor) and x.device.type == "cpu",
        lambda x: x.detach().numpy(),
        lambda xs: torch.cat(xs, dim=1),
    )


def _jax_output():
    jax = import_framework("jax")
    jnp = import_framework("jax.numpy")
    module = importlib.import_module("pedalboard.jax")

    def is_output_type(x):
        return isinstance(x, jax.Array) and all(d.platform == "cpu" for d in x.devices())

    return OutputFramework(
        "jax", module, is_output_type, np.asarray, lambda xs: jnp.concatenate(xs, axis=1)
    )


def _tensorflow_output():
    tf = import_framework("tensorflow")
    module = importlib.import_module("pedalboard.tensorflow")
    return OutputFramework(
        "tensorflow",
        module,
        lambda x: isinstance(x, tf.Tensor),
        lambda x: x.numpy(),
        lambda xs: tf.concat(xs, axis=1),
    )


OUTPUT_FACTORIES = {
    "torch": _torch_output,
    "jax": _jax_output,
    "tensorflow": _tensorflow_output,
}


@pytest.fixture(params=sorted(OUTPUT_FACTORIES))
def out(request):
    return OUTPUT_FACTORIES[request.param]()


@pytest.fixture
def wav_path(tmp_path):
    path = str(tmp_path / "fixture.wav")
    audio = make_audio()
    with pedalboard.io.AudioFile(path, "w", SAMPLE_RATE, NUM_CHANNELS) as f:
        f.write(audio)
    return path, audio


def assert_output_matches(out: OutputFramework, result, expected: np.ndarray):
    assert out.is_output_type(result), f"expected a {out.name} array, got {type(result)}"
    assert tuple(result.shape) == expected.shape
    as_numpy = out.to_numpy(result)
    assert as_numpy.dtype == expected.dtype
    np.testing.assert_allclose(as_numpy, expected, rtol=1e-6, atol=1e-6)


# --- AudioFile reading ---


def test_audiofile_read_returns_framework_array_matching_numpy(out, wav_path):
    path, _ = wav_path
    with pedalboard.io.AudioFile(path) as f:
        expected = f.read(f.frames)
    with out.module.AudioFile(path) as f:
        result = f.read(f.frames)
    assert_output_matches(out, result, expected)


def test_audiofile_metadata_unchanged(out, wav_path):
    path, _ = wav_path
    with out.module.AudioFile(path) as f:
        assert f.samplerate == SAMPLE_RATE
        assert f.num_channels == NUM_CHANNELS
        assert f.frames == NUM_FRAMES
        assert f.tell() == 0
        f.read(100)
        assert f.tell() == 100


def test_audiofile_without_context_manager(out, wav_path):
    path, _ = wav_path
    f = out.module.AudioFile(path)
    try:
        assert out.is_output_type(f.read(10))
    finally:
        f.close()
    assert f.closed


def test_chunked_read_loop_matches_numpy(out, wav_path):
    path, _ = wav_path
    with pedalboard.io.AudioFile(path) as f:
        expected = f.read(f.frames)
    chunks = []
    with out.module.AudioFile(path) as f:
        while f.tell() < f.frames:
            chunk = f.read(1000)
            assert out.is_output_type(chunk)
            chunks.append(chunk)
    assert_output_matches(out, out.concat(chunks), expected)


def test_resampled_to_returns_framework_array_matching_numpy(out, wav_path):
    path, _ = wav_path
    with pedalboard.io.AudioFile(path).resampled_to(22050) as f:
        expected = f.read(f.frames)
    with out.module.AudioFile(path).resampled_to(22050) as f:
        assert f.samplerate == 22050
        result = f.read(f.frames)
    assert_output_matches(out, result, expected)


# --- AudioFile writing ---


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_audiofile_write_accepts_any_framework(out, tmp_path, framework):
    audio = make_audio()
    path = str(tmp_path / f"out_{framework}.wav")
    with out.module.AudioFile(path, "w", SAMPLE_RATE, NUM_CHANNELS) as f:
        f.write(to_framework(audio, framework))
    with pedalboard.io.AudioFile(path) as f:
        np.testing.assert_allclose(f.read(f.frames), audio, atol=1e-4)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_audiofile_write_accepts_samples_by_channels_layout(out, tmp_path, framework):
    audio = make_audio()
    path = str(tmp_path / "out.wav")
    with out.module.AudioFile(path, "w", SAMPLE_RATE, NUM_CHANNELS) as f:
        f.write(to_framework(audio.T.copy(), framework))
    with pedalboard.io.AudioFile(path) as f:
        assert f.num_channels == NUM_CHANNELS
        np.testing.assert_allclose(f.read(f.frames), audio, atol=1e-4)


def test_audiofile_write_rejects_string(out, tmp_path):
    path = str(tmp_path / "out.wav")
    with out.module.AudioFile(path, "w", SAMPLE_RATE, NUM_CHANNELS) as f, pytest.raises(TypeError):
        f.write("not audio")


# --- Plugins and boards ---


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_plugin_accepts_any_framework(out, framework):
    audio = make_audio()
    expected = pedalboard.Reverb(room_size=0.5)(audio, SAMPLE_RATE)
    result = out.module.Reverb(room_size=0.5)(to_framework(audio, framework), SAMPLE_RATE)
    assert_output_matches(out, result, expected)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_pedalboard_accepts_any_framework(out, framework):
    audio = make_audio()
    expected = pedalboard.Pedalboard(
        [pedalboard.Gain(gain_db=-6), pedalboard.Compressor(), pedalboard.Reverb()]
    )(audio, SAMPLE_RATE)
    m = out.module
    board = m.Pedalboard([m.Gain(gain_db=-6), m.Compressor(), m.Reverb()])
    result = board(to_framework(audio, framework), SAMPLE_RATE)
    assert_output_matches(out, result, expected)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_plugin_accepts_samples_by_channels_layout(out, framework):
    audio = make_audio().T.copy()
    expected = pedalboard.Gain(gain_db=3)(audio, SAMPLE_RATE)
    result = out.module.Gain(gain_db=3)(to_framework(audio, framework), SAMPLE_RATE)
    assert_output_matches(out, result, expected)


def test_plugin_process_method_returns_framework_array(out):
    audio = make_audio()
    expected = pedalboard.Gain(gain_db=3).process(audio, SAMPLE_RATE)
    result = out.module.Gain(gain_db=3).process(audio, SAMPLE_RATE)
    assert_output_matches(out, result, expected)


def test_plugin_rejects_string(out):
    with pytest.raises(TypeError):
        out.module.Gain(gain_db=3)("not audio", SAMPLE_RATE)


def test_plugin_parameters_get_and_set(out):
    gain = out.module.Gain(gain_db=3)
    assert gain.gain_db == pytest.approx(3)
    gain.gain_db = -12
    assert gain.gain_db == pytest.approx(-12)


def test_plugin_isinstance_checks(out):
    reverb = out.module.Reverb()
    assert isinstance(reverb, pedalboard.Plugin)
    assert isinstance(reverb, out.module.Reverb)
    assert isinstance(out.module.Pedalboard(), pedalboard.Plugin)


def test_board_list_operations_and_param_change(out):
    m = out.module
    board = m.Pedalboard()
    board.append(m.Gain(gain_db=0))
    board.append(m.Reverb())
    assert len(board) == 2
    board[0].gain_db = -6
    assert board[0].gain_db == pytest.approx(-6)

    audio = make_audio()
    expected = pedalboard.Pedalboard([pedalboard.Gain(gain_db=-6), pedalboard.Reverb()])(
        audio, SAMPLE_RATE
    )
    assert_output_matches(out, board(audio, SAMPLE_RATE), expected)


def test_native_plugins_inside_framework_pedalboard(out):
    audio = make_audio()
    expected = pedalboard.Gain(gain_db=-6)(audio, SAMPLE_RATE)
    board = out.module.Pedalboard([pedalboard.Gain(gain_db=-6)])
    assert_output_matches(out, board(audio, SAMPLE_RATE), expected)


# --- Free functions ---


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_time_stretch_accepts_any_framework(out, framework):
    audio = make_audio(frames=SAMPLE_RATE)
    expected = pedalboard.time_stretch(audio, SAMPLE_RATE, 1.5)
    result = out.module.time_stretch(to_framework(audio, framework), SAMPLE_RATE, 1.5)
    assert_output_matches(out, result, expected)


# --- Isolation and cross-module use ---


def test_importing_module_does_not_change_pedalboard(out, wav_path):
    path, _ = wav_path
    audio = make_audio()
    assert isinstance(pedalboard.Gain(gain_db=3)(audio, SAMPLE_RATE), np.ndarray)
    assert isinstance(pedalboard.time_stretch(audio, SAMPLE_RATE, 1.5), np.ndarray)
    with pedalboard.io.AudioFile(path) as f:
        assert isinstance(f.read(10), np.ndarray)
    torch = import_framework("torch")
    with pytest.raises(TypeError):
        pedalboard.Gain(gain_db=3)(torch.from_numpy(audio), SAMPLE_RATE)


def test_cross_module_file_and_plugin(out, wav_path):
    import_framework("torch")
    pt = importlib.import_module("pedalboard.torch")

    path, _ = wav_path
    with pedalboard.io.AudioFile(path) as f:
        expected = pedalboard.Reverb()(f.read(f.frames), SAMPLE_RATE)
    with pt.AudioFile(path) as f:
        audio = f.read(f.frames)
    assert_output_matches(out, out.module.Reverb()(audio, SAMPLE_RATE), expected)


# --- Additional audio surface ---


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_free_process_function_accepts_any_framework(out, framework):
    audio = make_audio()
    plugins = [pedalboard.Gain(gain_db=-6), pedalboard.Reverb()]
    expected = pedalboard.process(audio, SAMPLE_RATE, plugins)
    m = out.module
    result = m.process(
        to_framework(audio, framework), SAMPLE_RATE, [m.Gain(gain_db=-6), m.Reverb()]
    )
    assert_output_matches(out, result, expected)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_chain_and_mix_accept_any_framework(out, framework):
    audio = make_audio()
    m = out.module
    expected_chain = pedalboard.Chain([pedalboard.Gain(gain_db=-3), pedalboard.Reverb()])(
        audio, SAMPLE_RATE
    )
    result = m.Chain([m.Gain(gain_db=-3), m.Reverb()])(to_framework(audio, framework), SAMPLE_RATE)
    assert_output_matches(out, result, expected_chain)

    expected_mix = pedalboard.Mix([pedalboard.Gain(gain_db=-3), pedalboard.Gain(gain_db=-9)])(
        audio, SAMPLE_RATE
    )
    result = m.Mix([m.Gain(gain_db=-3), m.Gain(gain_db=-9)])(
        to_framework(audio, framework), SAMPLE_RATE
    )
    assert_output_matches(out, result, expected_mix)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_board_process_method_accepts_any_framework(out, framework):
    audio = make_audio()
    expected = pedalboard.Pedalboard([pedalboard.Gain(gain_db=2)]).process(audio, SAMPLE_RATE)
    result = out.module.Pedalboard([out.module.Gain(gain_db=2)]).process(
        to_framework(audio, framework), SAMPLE_RATE
    )
    assert_output_matches(out, result, expected)


def test_read_raw_returns_framework_array(out, wav_path):
    path, _ = wav_path
    with pedalboard.io.AudioFile(path) as f:
        expected = f.read_raw(500)
    with out.module.AudioFile(path) as f:
        result = f.read_raw(500)
    assert out.is_output_type(result)
    assert tuple(result.shape) == expected.shape
    np.testing.assert_array_equal(out.to_numpy(result), expected)


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_encode_accepts_any_framework_and_matches_bytes(out, framework):
    audio = make_audio()
    expected = pedalboard.io.AudioFile.encode(audio, SAMPLE_RATE, "wav", NUM_CHANNELS)
    result = out.module.AudioFile.encode(
        to_framework(audio, framework), SAMPLE_RATE, "wav", NUM_CHANNELS
    )
    assert isinstance(result, bytes)
    assert result == expected


def test_file_metadata_identical_to_pedalboard(out, wav_path):
    path, _ = wav_path
    with pedalboard.io.AudioFile(path) as f, out.module.AudioFile(path) as g:
        for name in ("samplerate", "num_channels", "frames", "duration"):
            a, b = getattr(f, name), getattr(g, name)
            assert type(a) is type(b), name
            assert a == b, name
        assert type(f.tell()) is type(g.tell()) and f.tell() == g.tell()


# --- Requirement 1: Python protocols ---


def test_bool_of_boards_matches_pedalboard(out):
    m = out.module
    assert bool(m.Pedalboard([])) is False
    assert bool(m.Pedalboard([m.Gain()])) is True


def test_bool_of_plugin_matches_pedalboard(out):
    assert bool(out.module.Gain()) == bool(pedalboard.Gain())


# --- Requirement 2: iteration ---


def _assert_plugin_yields_output(out, plugin):
    audio = make_audio()
    expected = pedalboard.Gain(gain_db=-6)(audio, SAMPLE_RATE)
    for framework in ML_FRAMEWORKS:
        assert_output_matches(out, plugin(to_framework(audio, framework), SAMPLE_RATE), expected)


def test_plugins_from_for_loop_return_framework_type(out):
    board = out.module.Pedalboard([out.module.Gain(gain_db=-6)])
    for plugin in board:
        _assert_plugin_yields_output(out, plugin)


def test_plugin_from_next_iter_returns_framework_type(out):
    board = out.module.Pedalboard([out.module.Gain(gain_db=-6)])
    _assert_plugin_yields_output(out, next(iter(board)))


def test_plugins_from_list_return_framework_type(out):
    board = out.module.Pedalboard([out.module.Gain(gain_db=-6)])
    (plugin,) = list(board)
    _assert_plugin_yields_output(out, plugin)


def test_plugin_from_indexing_returns_framework_type(out):
    board = out.module.Pedalboard([out.module.Gain(gain_db=-6)])
    _assert_plugin_yields_output(out, board[0])


def test_native_plugin_retrieved_from_board_is_unchanged(out):
    # Only the board's own output is converted: a plain Pedalboard plugin put into the board
    # comes back out as the same plain plugin.
    native = pedalboard.Gain(gain_db=-6)
    board = out.module.Pedalboard([native])
    assert board[0] is native
    assert next(iter(board)) is native
    assert out.is_output_type(board(make_audio(), SAMPLE_RATE))


# --- Requirement 3: identity, equality, hashing ---


def test_indexing_returns_same_object(out):
    board = out.module.Pedalboard([out.module.Gain()])
    assert board[0] is board[0]
    assert board[0] == board[0]
    assert len({board[0], board[0]}) == 1


def test_iteration_returns_same_object_as_indexing(out):
    board = out.module.Pedalboard([out.module.Gain()])
    assert next(iter(board)) is board[0]


def test_plugin_in_board_is_original_object(out):
    plugin = out.module.Gain()
    board = out.module.Pedalboard([plugin])
    assert board[0] is plugin


def test_appended_plugin_is_original_object(out):
    plugin = out.module.Gain()
    board = out.module.Pedalboard()
    board.append(plugin)
    assert board[0] is plugin
    assert plugin in board


# --- Requirement 4: real classes ---


def test_classes_are_subclasses_of_pedalboard_classes(out):
    m = out.module
    assert issubclass(m.Reverb, pedalboard.Reverb)
    assert isinstance(m.Reverb(), pedalboard.Reverb)
    assert isinstance(m.Reverb(), m.Reverb)
    assert isinstance(m.Pedalboard([]), pedalboard.Pedalboard)
    assert isinstance(m.Pedalboard([]), m.Pedalboard)


def test_user_subclass_returns_framework_type(out):
    class Louder(out.module.Gain):
        pass

    audio = make_audio()
    expected = pedalboard.Gain(gain_db=6)(audio, SAMPLE_RATE)
    for framework in ML_FRAMEWORKS:
        result = Louder(gain_db=6)(to_framework(audio, framework), SAMPLE_RATE)
        assert_output_matches(out, result, expected)


def test_class_names_and_docstrings_preserved(out):
    m = out.module
    assert m.Reverb.__name__ == "Reverb"
    assert m.Pedalboard.__name__ == "Pedalboard"
    assert m.Reverb.__doc__ == pedalboard.Reverb.__doc__
    assert m.time_stretch.__doc__ == pedalboard.time_stretch.__doc__
    assert m.AudioFile.__doc__ == pedalboard.io.AudioFile.__doc__


# --- Requirement 5: non-audio values untouched ---


@pytest.mark.parametrize(
    "factory, attribute",
    [
        (lambda m: m.Gain(gain_db=-6), "gain_db"),
        (lambda m: m.Reverb(), "room_size"),
        (lambda m: m.Reverb(), "wet_level"),
        (lambda m: m.Compressor(), "threshold_db"),
    ],
)
def test_parameter_attributes_match_pedalboard(out, factory, attribute):
    ours = getattr(factory(out.module), attribute)
    theirs = getattr(factory(pedalboard), attribute)
    assert type(ours) is type(theirs)
    assert ours == theirs
    assert float(ours) == float(theirs)
    assert ours + 1 == theirs + 1
    assert ours * 2 == theirs * 2


# --- Requirement 6: mixing modules ---


@pytest.mark.parametrize("other_name", sorted(OUTPUT_FACTORIES))
def test_other_module_plugin_inside_board_uses_board_type(out, other_name):
    other = OUTPUT_FACTORIES[other_name]()
    audio = make_audio()
    expected = pedalboard.Gain(gain_db=-6)(audio, SAMPLE_RATE)
    board = out.module.Pedalboard([other.module.Gain(gain_db=-6)])
    assert_output_matches(out, board(to_framework(audio, other_name), SAMPLE_RATE), expected)
    assert_output_matches(
        other, other.module.Gain(gain_db=-6)(to_framework(audio, out.name), SAMPLE_RATE), expected
    )


# --- Requirement 7: errors ---


def test_board_rejects_string(out):
    with pytest.raises(TypeError):
        out.module.Pedalboard([out.module.Gain()])("not audio", SAMPLE_RATE)


# --- Narrowed public surface ---

REQUIRED_EXPORTS = (
    "AudioFile",
    "process",
    "time_stretch",
    "load_plugin",
    "Pedalboard",
    "Chain",
    "Mix",
    "Gain",
    "Reverb",
)


def test_all_lists_audio_entry_points(out):
    exported = set(out.module.__all__)
    for name in REQUIRED_EXPORTS:
        assert name in exported, name
    for name in out.module.__all__:
        assert hasattr(out.module, name), name
    assert "AudioStream" not in exported
    assert "ReadableAudioFile" not in exported


def test_audiofile_isinstance_in_every_mode(out, wav_path, tmp_path):
    path, _ = wav_path
    m = out.module
    with m.AudioFile(path) as f:
        assert isinstance(f, m.AudioFile)
        assert f.resampled_to(f.samplerate) is f
        with f.resampled_to(22050) as r:
            assert isinstance(r, m.AudioFile)
    with m.AudioFile(str(tmp_path / "w.wav"), "w", SAMPLE_RATE, NUM_CHANNELS) as w:
        assert isinstance(w, m.AudioFile)


READ_FORMS = {
    "positional": lambda cls, p: cls(p),
    "mode_positional": lambda cls, p: cls(p, "r"),
    "mode_keyword": lambda cls, p: cls(p, mode="r"),
    "filename_keyword": lambda cls, p: cls(filename=p),
    "file_like_keyword": lambda cls, p: cls(file_like=io.BytesIO(Path(p).read_bytes())),
}


@pytest.mark.parametrize("form", sorted(READ_FORMS))
def test_audiofile_read_constructor_forms_match_native(out, wav_path, form):
    path, _ = wav_path
    with READ_FORMS[form](pedalboard.io.AudioFile, path) as f:
        expected = f.read(f.frames)
    with READ_FORMS[form](out.module.AudioFile, path) as f:
        assert isinstance(f, out.module.AudioFile)
        result = f.read(f.frames)
    assert_output_matches(out, result, expected)


WRITE_FORMS = {
    "positional": lambda cls, p: cls(p, "w", SAMPLE_RATE, NUM_CHANNELS),
    "keyword": lambda cls, p: cls(p, "w", samplerate=SAMPLE_RATE, num_channels=NUM_CHANNELS),
    "inferred_mode": lambda cls, p: cls(p, samplerate=SAMPLE_RATE, num_channels=NUM_CHANNELS),
    "filename_keyword": lambda cls, p: cls(
        filename=p, mode="w", samplerate=SAMPLE_RATE, num_channels=NUM_CHANNELS
    ),
}


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
@pytest.mark.parametrize("form", sorted(WRITE_FORMS))
def test_audiofile_write_constructor_forms_match_native(out, tmp_path, form, framework):
    audio = make_audio()
    native_path = str(tmp_path / "native.wav")
    ours_path = str(tmp_path / "ours.wav")
    with WRITE_FORMS[form](pedalboard.io.AudioFile, native_path) as f:
        f.write(audio)
    with WRITE_FORMS[form](out.module.AudioFile, ours_path) as f:
        assert isinstance(f, out.module.AudioFile)
        assert f.samplerate == SAMPLE_RATE
        assert f.num_channels == NUM_CHANNELS
        f.write(to_framework(audio, framework))
    with pedalboard.io.AudioFile(native_path) as a, pedalboard.io.AudioFile(ours_path) as b:
        assert (a.samplerate, a.num_channels, a.frames) == (b.samplerate, b.num_channels, b.frames)
        np.testing.assert_array_equal(a.read(a.frames), b.read(b.frames))


@pytest.mark.parametrize("framework", ML_FRAMEWORKS)
def test_audiofile_file_like_round_trip_with_keywords(out, framework):
    audio = make_audio()
    native_buf, ours_buf = io.BytesIO(), io.BytesIO()
    with pedalboard.io.AudioFile(
        file_like=native_buf,
        mode="w",
        samplerate=SAMPLE_RATE,
        num_channels=NUM_CHANNELS,
        format="wav",
    ) as f:
        f.write(audio)
    with out.module.AudioFile(
        file_like=ours_buf,
        mode="w",
        samplerate=SAMPLE_RATE,
        num_channels=NUM_CHANNELS,
        format="wav",
    ) as f:
        assert isinstance(f, out.module.AudioFile)
        f.write(to_framework(audio, framework))
    assert ours_buf.getvalue() == native_buf.getvalue()

    with pedalboard.io.AudioFile(io.BytesIO(native_buf.getvalue())) as f:
        expected = f.read(f.frames)
    with out.module.AudioFile(io.BytesIO(ours_buf.getvalue())) as f:
        assert isinstance(f, out.module.AudioFile)
        result = f.read(f.frames)
    assert_output_matches(out, result, expected)

    with pedalboard.io.AudioFile(io.BytesIO(native_buf.getvalue())).resampled_to(22050) as f:
        expected_resampled = f.read(f.frames)
    with out.module.AudioFile(io.BytesIO(ours_buf.getvalue())).resampled_to(22050) as f:
        assert f.samplerate == 22050
        resampled = f.read(f.frames)
    assert_output_matches(out, resampled, expected_resampled)


def test_audiofile_subclass_initializes_once_and_retains_local_metadata(out, wav_path):
    path, _ = wav_path
    initialized = []

    class TaggedFile(out.module.AudioFile):
        def __init__(self, *args, **kwargs):
            initialized.append(self)
            super().__init__(*args, **kwargs)
            self.label = "fixture"

    with TaggedFile(path) as f:
        assert isinstance(f, TaggedFile)
        assert initialized == [f]
        assert vars(f)["label"] == "fixture"
        assert f.label == "fixture"
        assert f.samplerate == SAMPLE_RATE
        assert out.is_output_type(f.read(100))
    assert f.closed


def test_audiofile_invalid_mode_raises_same_exception_type_as_native(out, wav_path):
    path, _ = wav_path
    with pytest.raises(Exception) as native:
        pedalboard.io.AudioFile(path, "q")  # type: ignore
    with pytest.raises(native.type):
        out.module.AudioFile(path, "q")
