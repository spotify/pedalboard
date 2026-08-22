#! /usr/bin/env python
#
# Copyright 2023 Spotify AB
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

from typing import List, Tuple

import mido
import pytest

from pedalboard.midi_utils import normalize_midi_messages


@pytest.mark.parametrize(
    "_input,expected",
    [
        (
            [
                mido.Message("note_on", note=100, velocity=3, time=0),
                mido.Message("note_off", note=100, time=5.0),
            ],
            [(bytes([144, 100, 3]), 0.0), (bytes([128, 100, 64]), 5.0)],
        )
    ],
)
def test_mido_normalization(_input, expected: List[Tuple[bytes, float]]):
    assert normalize_midi_messages(_input) == expected


@pytest.mark.parametrize(
    "malformed",
    [
        pytest.param((bytes([0x90, 60, 64]),), id="one_tuple_missing_timestamp"),
        pytest.param((bytes([0x90, 60, 64]), 0.0, 99), id="three_tuple_extra_element"),
        pytest.param(bytes([0x90, 60, 64]), id="bare_bytes_without_timestamp"),
        pytest.param(None, id="none"),
        pytest.param(42, id="bare_int"),
    ],
)
def test_malformed_messages_raise(malformed):
    """Malformed messages must raise rather than being silently dropped."""
    messages = [
        (bytes([0x90, 60, 64]), 0.0),
        malformed,
        (bytes([0x80, 60, 64]), 1.0),
    ]
    with pytest.raises(TypeError, match="index 1"):
        normalize_midi_messages(messages)


def test_valid_messages_are_unaffected():
    messages = [(bytes([0x90, 60, 64]), 0.0), (bytes([0x80, 60, 64]), 1.0)]
    assert normalize_midi_messages(messages) == messages
