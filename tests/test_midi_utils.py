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
    "bad_message,expected_index",
    [
        # 1-tuple — timestamp accidentally omitted
        ([(bytes([0x90, 60, 64]),)], 0),
        # 3-tuple — extra unexpected field
        ([(bytes([0x90, 60, 64]), 0.0, "extra")], 0),
        # bare bytes — no timestamp provided
        ([bytes([0x90, 60, 64])], 0),
        # bare integer — not a message at all
        ([(bytes([0x90, 60, 64]), 0.0), 42], 1),
        # None
        ([None], 0),
    ],
)
def test_malformed_message_raises_type_error(bad_message, expected_index):
    """Malformed messages must raise TypeError, not be silently dropped."""
    with pytest.raises(TypeError, match=f"index {expected_index}"):
        normalize_midi_messages(bad_message)


def test_malformed_message_error_contains_repr():
    """The TypeError message must include the offending value's repr."""
    bad = (bytes([0x90, 60, 64]),)  # 1-tuple
    with pytest.raises(TypeError, match=r"b'\\x90<@'"):
        normalize_midi_messages([bad])


def test_valid_messages_unaffected_by_fix():
    """The happy path must still work correctly after the fix."""
    result = normalize_midi_messages([
        (bytes([0x90, 60, 64]), 0.0),
        (bytes([0x80, 60, 64]), 1.0),
    ])
    assert len(result) == 2
    assert result[0] == (bytes([0x90, 60, 64]), 0.0)
    assert result[1] == (bytes([0x80, 60, 64]), 1.0)
