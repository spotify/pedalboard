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
from pedalboard import Pedalboard, Gain


@pytest.mark.parametrize("shape", [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_no_transforms(shape, sr=44100):
    _input = np.random.rand(*shape).astype(np.float32)

    output = Pedalboard([]).process(_input, sr)

    assert _input.shape == output.shape
    assert np.allclose(_input, output, rtol=0.0001)


def test_fail_on_invalid_plugin():
    with pytest.raises(TypeError):
        Pedalboard(["I want a reverb please"])


def test_fail_on_invalid_sample_rate():
    with pytest.raises(TypeError):
        Pedalboard([]).process([], "fourty four one hundred")


def test_fail_on_invalid_buffer_size():
    with pytest.raises(TypeError):
        Pedalboard([]).process([], 44100, "very big buffer please")


def test_repr():
    gain = Gain(-6)
    value = repr(Pedalboard([gain]))
    # Allow flexibility; all we care about is that these values exist in the repr.
    assert "Pedalboard" in value
    assert " 1 " in value
    assert repr(gain) in value

    gain2 = Gain(-6)
    value = repr(Pedalboard([gain, gain2]))
    # Allow flexibility; all we care about is that these values exist in the repr.
    assert "Pedalboard" in value
    assert " 2 " in value
    assert repr(gain) in value
    assert repr(gain2) in value


def test_is_list_like():
    gain = Gain(-6)

    assert len(Pedalboard([gain])) == 1
    assert len(Pedalboard([gain, Gain(-6)])) == 2

    pb = Pedalboard([gain])
    assert len(pb) == 1

    # Allow adding elements, like a list:
    pb.append(Gain())
    assert len(pb) == 2

    with pytest.raises(TypeError):
        pb.append("not a plugin")

    # Allow deleting elements, like a list:
    del pb[1]
    assert len(pb) == 1

    # Allow getting elements, like a list:
    assert pb[0] is gain

    # Allow setting elements, like a list:
    pb[0] = gain
    assert pb[0] is gain

    with pytest.raises(TypeError):
        pb[0] = "not a plugin"
