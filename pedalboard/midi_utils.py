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

from typing import List, Tuple, Union


def parse_midi_message_part(byte: Union[int, str, bytes]) -> int:
    if isinstance(byte, int):
        return byte
    elif isinstance(byte, bytes):
        if len(byte) == 1:
            return byte[0]
        else:
            raise NotImplementedError("Not sure how to interpret provided MIDI message.")
    elif isinstance(byte, str):
        try:
            return int(byte)
        except ValueError as e:
            raise NotImplementedError("Not sure how to interpret provided MIDI message.") from e
    raise NotImplementedError("MIDI messages must currently be bytes or lists of byte values.")


def parse_midi_message_string(midi_message_string: str) -> bytes:
    raise NotImplementedError("MIDI messages must currently be bytes or lists of byte values.")


def normalize_midi_messages(_input) -> List[Tuple[bytes, float]]:
    """
    Given a duck-typed Python input, usually an iterable of MIDI messages,
    normalize the input to a list of tuples of bytes which can be converted
    into a juce::MidiBuffer on the C++ side.
    """
    output = []
    for message in _input:
        if hasattr(message, "bytes") and hasattr(message, "time"):
            output.append((bytes(message.bytes()), message.time))
        elif (isinstance(message, tuple) or isinstance(message, list)) and len(message) == 2:
            message, time = message
            if isinstance(message, str):
                message = parse_midi_message_string(message)
            if isinstance(message, list):
                message = bytes([parse_midi_message_part(x) for x in message])
            elif not isinstance(message, bytes):
                message = bytes(message)
            output.append((message, time))
    return output
