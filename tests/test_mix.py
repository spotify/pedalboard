#! /usr/bin/env python
#
# Copyright 2021 Spotify AB
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


import pytest
import numpy as np
from pedalboard import Pedalboard, Compressor, Delay, Distortion, Gain, Mix, Chain, PitchShift, Reverb
from pedalboard_native._internal import AddLatency


NUM_SECONDS = 4


def test_chain_syntactic_sugar():
    sr = 44100
    _input = np.random.rand(int(NUM_SECONDS * sr), 2).astype(np.float32)

    # Nested chains should work fine when passed as lists:
    pb = Pedalboard([Gain(6), [Gain(-6), Gain(1)], Gain(-1)])
    output = pb(_input, sr)

    assert isinstance(pb[0], Gain)
    assert pb[0].gain_db == 6

    assert isinstance(pb[1], Chain)
    assert pb[1].plugins[0].gain_db == -6
    assert pb[1].plugins[1].gain_db == 1

    assert isinstance(pb[2], Gain)
    assert pb[2].gain_db == -1

    np.testing.assert_allclose(_input, output, rtol=0.01)


def test_mix_syntactic_sugar():
    sr = 44100
    _input = np.random.rand(int(NUM_SECONDS * sr), 2).astype(np.float32)

    # Nested mixes should work fine when passed as sets:
    pb = Pedalboard([Gain(6), {Gain(-6), Gain(-6)}, Gain(-6)])
    output = pb(_input, sr)

    assert isinstance(pb[0], Gain)
    assert pb[0].gain_db == 6

    assert isinstance(pb[1], Mix)
    assert set([plugin.gain_db for plugin in pb[1].plugins]) == set([-6, -6])

    assert isinstance(pb[2], Gain)
    assert pb[2].gain_db == -6

    np.testing.assert_allclose(_input, output, rtol=0.01)


def test_deep_nesting():
    sr = 44100
    _input = np.random.rand(int(NUM_SECONDS * sr), 2).astype(np.float32)

    pb = Pedalboard(
        [
            # This parallel chain should boost by 6dB, but due to
            # there being 2 outputs, the result will be +12dB.
            {tuple([Gain(1) for _ in range(6)]), tuple([Gain(1) for _ in range(6)])},
            # This parallel chain should cut by -24dB, but due to
            # there being 2 outputs, the result will be -18dB.
            {tuple([Gain(-1) for _ in range(24)]), tuple([Gain(-1) for _ in range(24)])},
        ]
    )
    output = pb(_input, sr)
    np.testing.assert_allclose(_input * 0.5, output, rtol=0.01)


def test_nesting_pedalboards():
    sr = 44100
    _input = np.random.rand(int(NUM_SECONDS * sr), 2).astype(np.float32)

    pb = Pedalboard(
        [
            # This parallel chain should boost by 6dB, but due to
            # there being 2 outputs, the result will be +12dB.
            {Pedalboard([Gain(1) for _ in range(6)]), Pedalboard([Gain(1) for _ in range(6)])},
            # This parallel chain should cut by -24dB, but due to
            # there being 2 outputs, the result will be -18dB.
            {Pedalboard([Gain(-1) for _ in range(24)]), Pedalboard([Gain(-1) for _ in range(24)])},
        ]
    )
    output = pb(_input, sr)
    np.testing.assert_allclose(_input * 0.5, output, rtol=0.01)


def test_chain_latency_compensation():
    sr = 44100
    _input = np.random.rand(int(NUM_SECONDS * sr), 2).astype(np.float32)

    pb = Pedalboard(
        [
            # This parallel chain should boost by 6dB, but due to
            # there being 2 outputs, the result will be +12dB.
            {tuple([Gain(1) for _ in range(6)]), tuple([Gain(1) for _ in range(6)])},
            # This parallel chain should cut by -24dB, but due to
            # there being 2 outputs, the result will be -18dB.
            {tuple([Gain(-1) for _ in range(24)]), tuple([Gain(-1) for _ in range(24)])},
        ]
    )
    output = pb(_input, sr)
    np.testing.assert_allclose(_input * 0.5, output, rtol=0.01)


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [128, 8192, 22050, 65536])
@pytest.mark.parametrize("latency_a_seconds", [0.25, 1, NUM_SECONDS / 2])
@pytest.mark.parametrize("latency_b_seconds", [0.25, 1, NUM_SECONDS / 2])
def test_mix_latency_compensation(sample_rate, buffer_size, latency_a_seconds, latency_b_seconds):
    noise = np.random.rand(int(NUM_SECONDS * sample_rate))
    pb = Pedalboard(
        [
            {
                AddLatency(int(latency_a_seconds * sample_rate)),
                AddLatency(int(latency_b_seconds * sample_rate)),
            },
        ]
    )
    output = pb(noise, sample_rate, buffer_size=buffer_size)

    # * 2 here as the mix plugin mixes each plugin at 100% by default
    np.testing.assert_allclose(output, noise * 2, rtol=0.01)


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [128, 8192, 65536])
@pytest.mark.parametrize("latency_a_seconds", [0.25, 1, 2, 10])
@pytest.mark.parametrize("latency_b_seconds", [0.25, 1, 2, 10])
def test_chain_latency_compensation(sample_rate, buffer_size, latency_a_seconds, latency_b_seconds):
    noise = np.random.rand(int(NUM_SECONDS * sample_rate))
    pb = Pedalboard(
        [
            [
                AddLatency(int(latency_a_seconds * sample_rate)),
                AddLatency(int(latency_b_seconds * sample_rate)),
            ],
            [
                AddLatency(int(latency_a_seconds * sample_rate)),
                AddLatency(int(latency_b_seconds * sample_rate)),
            ],
        ]
    )
    output = pb(noise, sample_rate, buffer_size=buffer_size)
    np.testing.assert_allclose(output, noise, rtol=0.01)


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
@pytest.mark.parametrize("buffer_size", [128, 8192, 65536])
def test_readme_example_does_not_crash(sample_rate, buffer_size):
    noise = np.random.rand(int(NUM_SECONDS * sample_rate))

    passthrough = Gain(gain_db=0)

    delay_and_pitch_shift = Pedalboard(
        [
            Delay(delay_seconds=0.25, mix=1.0),
            PitchShift(semitones=7),
            Gain(gain_db=-3),
        ]
    )

    delay_longer_and_more_pitch_shift = Pedalboard(
        [
            Delay(delay_seconds=0.25, mix=1.0),
            PitchShift(semitones=12),
            Gain(gain_db=-6),
        ]
    )

    original_plus_delayed_harmonies = Pedalboard(
        {passthrough, delay_and_pitch_shift, delay_longer_and_more_pitch_shift}
    )
    # TODO: Allow passing a Pedalboard into Mix (which will require Pedalboard to be a subclass of Plugin)
    original_plus_delayed_harmonies(noise, sample_rate=sample_rate, buffer_size=buffer_size)

    # or mix and match in more complex ways:
    original_plus_delayed_harmonies = Pedalboard([
        # Put a compressor at the front of the chain:
        Compressor(),
        # Split the chain and mix three different effects equally:
        {
            (passthrough, Distortion(drive_db=36)),
            (delay_and_pitch_shift, Reverb(room_size=1)),
            delay_longer_and_more_pitch_shift
        },
        # Add a reverb on the final mix:
        Reverb()
    ])
    original_plus_delayed_harmonies(noise, sample_rate=sample_rate, buffer_size=buffer_size)