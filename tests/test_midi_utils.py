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
        # 1-tuple: timestamp accidentally omitted
        ((bytes([0x90, 60, 64]),), 1),
        # 3-tuple: extra unexpected field
        ((bytes([0x90, 60, 64]), 0.0, "extra"), 0),
        # raw bytes with no timestamp
        (bytes([0x90, 60, 64]), 0),
        # bare integer
        (42, 0),
        # None
        (None, 0),
    ],
)
def test_normalize_midi_messages_raises_on_malformed(bad_message, expected_index):
    valid = (bytes([0x90, 60, 64]), 0.0)
    messages = [valid] * expected_index + [bad_message]
    with pytest.raises(TypeError, match=f"index {expected_index}"):
        normalize_midi_messages(messages)
