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

import copy
import os
import pickle
import re
import subprocess
import sys
import time
from typing import Any

import numpy as np
import pytest

import pedalboard
from pedalboard._pedalboard import _writable_property_names
from pedalboard_native._internal import AddLatency

from .test_external_plugins import (
    AVAILABLE_EFFECT_PLUGINS_IN_TEST_ENVIRONMENT,
    load_test_plugin,
)

SAMPLE_RATE = 44100
IMPULSE_RESPONSE_PATH = os.path.join(os.path.dirname(__file__), "impulse_response.wav")

# Non-default constructor arguments for every built-in plugin, to make sure that
# pickling preserves each plugin's configuration (rather than just its type).
NON_DEFAULT_PLUGINS = [
    pedalboard.Bitcrush(bit_depth=4),
    pedalboard.BrickwallLimiter(ceiling_db=-3, release_ms=50, lookahead_ms=2, true_peak=True),
    pedalboard.Chorus(rate_hz=2.5, depth=0.5, centre_delay_ms=9, feedback=0.3, mix=0.7),
    pedalboard.Clipping(threshold_db=-12),
    pedalboard.Compressor(threshold_db=-20, ratio=4, attack_ms=5, release_ms=200),
    pedalboard.Delay(delay_seconds=0.25, feedback=0.4, mix=0.3),
    pedalboard.Distortion(drive_db=12),
    pedalboard.GSMFullRateCompressor(quality=pedalboard.Resample.Quality.Linear),
    pedalboard.Gain(gain_db=-6),
    pedalboard.HighShelfFilter(cutoff_frequency_hz=2000, gain_db=3, q=1.5),
    pedalboard.HighpassFilter(cutoff_frequency_hz=120),
    pedalboard.Invert(),
    pedalboard.LadderFilter(
        mode=pedalboard.LadderFilter.Mode.HPF24, cutoff_hz=800, resonance=0.3, drive=2
    ),
    pedalboard.Limiter(threshold_db=-3, release_ms=50),
    pedalboard.LowShelfFilter(cutoff_frequency_hz=200, gain_db=-3, q=0.5),
    pedalboard.LowpassFilter(cutoff_frequency_hz=5000),
    pedalboard.MP3Compressor(vbr_quality=5.5),
    pedalboard.NoiseGate(threshold_db=-40, ratio=4, attack_ms=2, release_ms=50),
    pedalboard.PeakFilter(cutoff_frequency_hz=1000, gain_db=6, q=2),
    pedalboard.Phaser(rate_hz=0.5, depth=0.8, centre_frequency_hz=900, feedback=0.2, mix=0.6),
    pedalboard.PitchShift(semitones=3),
    pedalboard.Resample(
        target_sample_rate=16000, quality=pedalboard.Resample.Quality.ZeroOrderHold
    ),
    pedalboard.Reverb(room_size=0.8, damping=0.2, wet_level=0.5, dry_level=0.6, width=0.7),
    pedalboard.Convolution(IMPULSE_RESPONSE_PATH, mix=0.5),
    pedalboard.Convolution(
        np.random.default_rng(0).random((2, 1000), dtype=np.float32), sample_rate=22050, mix=0.8
    ),
]


def all_plugin_classes():
    def subclasses(klass):
        for subclass in klass.__subclasses__():
            yield subclass
            yield from subclasses(subclass)

    return sorted(set(subclasses(pedalboard.Plugin)), key=lambda klass: klass.__name__)


def constructor_argument_names(klass):
    """Parse the argument names out of a Pybind11-generated constructor signature."""
    names = set()
    for line in (klass.__init__.__doc__ or "").splitlines():
        if line.startswith("__init__(") or re.match(r"^\d+\. __init__\(", line):
            names.update(re.findall(r"[(,] ?(\w+): ", line))
    return names - {"self"}


def get_properties(plugin):
    return {name: getattr(plugin, name) for name in _writable_property_names(type(plugin))}


def noise(num_channels: int = 2, seconds: float = 1.0) -> np.ndarray:
    return (
        np.random.default_rng(1234)
        .uniform(-0.5, 0.5, (num_channels, int(SAMPLE_RATE * seconds)))
        .astype(np.float32)
    )


@pytest.mark.parametrize("plugin", NON_DEFAULT_PLUGINS, ids=lambda p: type(p).__name__)
def test_pickle_round_trip_preserves_parameters(plugin):
    restored = pickle.loads(pickle.dumps(plugin))
    assert type(restored) is type(plugin)
    assert restored is not plugin
    assert get_properties(restored) == get_properties(plugin)


@pytest.mark.parametrize("plugin", NON_DEFAULT_PLUGINS, ids=lambda p: type(p).__name__)
def test_pickled_plugin_produces_identical_audio(plugin):
    audio = noise()
    restored = pickle.loads(pickle.dumps(plugin))
    np.testing.assert_allclose(plugin(audio, SAMPLE_RATE), restored(audio, SAMPLE_RATE))


@pytest.mark.parametrize("protocol", range(pickle.HIGHEST_PROTOCOL + 1))
def test_pickle_protocols(protocol):
    board = pedalboard.Pedalboard([pedalboard.Gain(gain_db=-3), pedalboard.Reverb(room_size=0.9)])
    restored = pickle.loads(pickle.dumps(board, protocol=protocol))
    assert [get_properties(p) for p in restored] == [get_properties(p) for p in board]


def test_pickle_nested_containers():
    board = pedalboard.Pedalboard(
        [
            pedalboard.Gain(gain_db=-6),
            pedalboard.Mix(
                [
                    pedalboard.Chain([pedalboard.Delay(delay_seconds=0.1), pedalboard.Gain(-3)]),
                    pedalboard.PitchShift(semitones=-2),
                ]
            ),
            pedalboard.Compressor(threshold_db=-18, ratio=3),
        ]
    )
    restored: Any = pickle.loads(pickle.dumps(board))

    assert type(restored) is pedalboard.Pedalboard
    mix: Any = restored[1]
    chain: Any = mix[0]
    delay: Any = chain[0]
    pitch_shift: Any = mix[1]
    assert type(mix) is pedalboard.Mix
    assert type(chain) is pedalboard.Chain
    assert delay.delay_seconds == pytest.approx(0.1)
    assert pitch_shift.semitones == -2
    assert repr(restored).startswith("<Pedalboard with 3 plugins")

    audio = noise()
    np.testing.assert_allclose(board(audio, SAMPLE_RATE), restored(audio, SAMPLE_RATE))


def test_pickle_empty_containers():
    for container in [pedalboard.Pedalboard(), pedalboard.Chain([]), pedalboard.Mix([])]:
        restored = pickle.loads(pickle.dumps(container))
        assert type(restored) is type(container)
        assert len(restored) == 0


def test_pickle_preserves_shared_plugins():
    # If the same plugin appears twice, it should still be one object after unpickling:
    gain = pedalboard.Gain(gain_db=-1)
    board = pedalboard.Pedalboard([gain, pedalboard.Reverb(), gain])
    restored = pickle.loads(pickle.dumps(board))
    assert restored[0] is restored[2]
    assert restored[0] is not restored[1]


def test_pickle_convolution_from_array():
    impulse_response = np.random.default_rng(0).random((2, 512), dtype=np.float32)
    plugin = pedalboard.Convolution(impulse_response, sample_rate=22050, mix=0.25)
    restored = pickle.loads(pickle.dumps(plugin))
    assert restored.impulse_response_filename is None
    assert restored.impulse_response_sample_rate == 22050
    assert restored.mix == 0.25
    np.testing.assert_array_equal(restored.impulse_response, plugin.impulse_response)


def test_pickle_convolution_from_file():
    plugin = pedalboard.Convolution(IMPULSE_RESPONSE_PATH)
    assert plugin.impulse_response_sample_rate is None
    restored = pickle.loads(pickle.dumps(plugin))
    assert restored.impulse_response_filename == IMPULSE_RESPONSE_PATH
    assert restored.impulse_response_sample_rate is None


def test_copy_and_deepcopy():
    board = pedalboard.Pedalboard([pedalboard.Gain(gain_db=-6), pedalboard.Reverb()])

    shallow = copy.copy(board)
    assert shallow is not board
    assert shallow[0] is board[0]

    deep: Any = copy.deepcopy(board)
    assert deep is not board
    assert deep[0] is not board[0]
    deep[0].gain_db = 6
    assert board[0].gain_db == -6  # type: ignore


def test_unpickled_plugin_starts_from_a_clean_state():
    # Only configuration is pickled, not internal DSP state like a delay line's contents:
    delay = pedalboard.Delay(delay_seconds=0.1, feedback=0.5, mix=1.0)
    delay(noise(seconds=0.5), SAMPLE_RATE, reset=False)
    restored = pickle.loads(pickle.dumps(delay))
    silence = np.zeros((2, SAMPLE_RATE // 2), dtype=np.float32)
    assert np.max(np.abs(restored(silence, SAMPLE_RATE, reset=False))) == 0


def test_internal_test_plugins_are_not_pickleable():
    with pytest.raises(TypeError, match="cannot be pickled"):
        pickle.dumps(AddLatency(100))


@pytest.mark.parametrize("plugin_class", all_plugin_classes(), ids=lambda k: k.__name__)
def test_every_constructor_argument_is_pickled(plugin_class):
    """
    Guard against new plugins with constructor-only arguments, which
    would silently be lost when pickling.
    """
    if plugin_class.__module__ == "pedalboard_native._internal":
        pytest.skip("Internal test plugins are not pickleable.")
    handled_by_constructor = {
        pedalboard.Convolution: {"impulse_response_filename", "sample_rate"},
        pedalboard.Chain: {"plugins"},
        pedalboard.Mix: {"plugins"},
        pedalboard.PluginContainer: {"plugins"},
    }
    for klass in pedalboard._AVAILABLE_PLUGIN_CLASSES:
        handled_by_constructor[klass] = {
            "path_to_plugin_file",
            "plugin_name",
            "parameter_values",  # Restored via raw_state
            "initialization_timeout",  # Only affects loading, not the plugin itself
        }
    unhandled = (
        constructor_argument_names(plugin_class)
        - set(_writable_property_names(plugin_class))
        - handled_by_constructor.get(plugin_class, set())
    )
    assert not unhandled, (
        f"{plugin_class.__name__} constructor arguments aren't pickled: {unhandled}"
    )


def test_pedalboard_can_be_unpickled_in_a_new_process():
    # Process pools (i.e.: multiprocessing with the "spawn" start method, the default on
    # macOS and Windows) pickle their arguments and unpickle them in a fresh interpreter:
    board = pedalboard.Pedalboard(
        [pedalboard.Compressor(threshold_db=-20, ratio=4), pedalboard.Reverb(room_size=0.7)]
    )
    audio = noise(seconds=0.5)
    script = (
        "import pickle, sys\n"
        "board, audio, sample_rate = pickle.loads(sys.stdin.buffer.read())\n"
        "sys.stdout.buffer.write(pickle.dumps((repr(board), board(audio, sample_rate))))\n"
    )
    result = subprocess.run(
        [sys.executable, "-c", script],
        input=pickle.dumps((board, audio, SAMPLE_RATE)),
        capture_output=True,
        check=True,
    )
    board_repr, output = pickle.loads(result.stdout)
    assert board_repr.startswith("<Pedalboard with 2 plugins: [<pedalboard.Compressor")
    np.testing.assert_allclose(output, board(audio, SAMPLE_RATE))


def loads_with_retries(data: bytes, attempts: int = 5) -> Any:
    # Loading external plugins occasionally fails in test; see load_test_plugin.
    for attempt in range(attempts - 1):
        try:
            return pickle.loads(data)
        except ImportError:
            time.sleep(attempt)
    return pickle.loads(data)


@pytest.mark.parametrize("plugin_filename", AVAILABLE_EFFECT_PLUGINS_IN_TEST_ENVIRONMENT)
def test_pickle_external_plugin(plugin_filename):
    plugin: Any = load_test_plugin(plugin_filename, disable_caching=True)
    assert os.path.exists(plugin.path_to_plugin_file)

    # Change a parameter, so that we can tell whether the state was restored:
    name, parameter = next(
        (name, parameter)
        for name, parameter in plugin.parameters.items()
        if parameter.type is float
    )
    parameter.raw_value = 0.2 if abs(parameter.raw_value - 0.2) > 0.05 else 0.8
    expected_value = parameter.raw_value

    restored = loads_with_retries(pickle.dumps(plugin))
    assert type(restored) is type(plugin)
    assert restored is not plugin
    assert restored.name == plugin.name
    assert restored.path_to_plugin_file == plugin.path_to_plugin_file
    assert restored.parameters[name].raw_value == pytest.approx(expected_value, abs=1e-4)


@pytest.mark.parametrize("plugin_filename", AVAILABLE_EFFECT_PLUGINS_IN_TEST_ENVIRONMENT)
def test_pickle_pedalboard_containing_external_plugin(plugin_filename):
    plugin = load_test_plugin(plugin_filename, disable_caching=True)
    board = pedalboard.Pedalboard([pedalboard.Gain(gain_db=-3), plugin])
    restored: Any = loads_with_retries(pickle.dumps(board))
    assert type(restored[1]) is type(plugin)
    assert restored[1].raw_state == plugin.raw_state
