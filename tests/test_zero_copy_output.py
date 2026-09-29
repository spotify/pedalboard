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


import numpy as np
import pytest

from pedalboard import Gain


@pytest.mark.parametrize("sample_rate", [22050, 44100, 48000])
def test_mono_output_not_copied(sample_rate):
    """Mono output with no latency should reuse the buffer, not copy it."""
    signal = np.sin(
        2 * np.pi * 440 * np.arange(sample_rate) / sample_rate
    ).astype(np.float32)
    out = Gain(gain_db=0).process(signal, sample_rate)
    assert out.flags["C_CONTIGUOUS"]
    assert out.flags["WRITEABLE"]
    np.testing.assert_allclose(out, signal, atol=1e-7)


def test_mono_output_lifetime_independent():
    """Each mono output must own its data independently."""
    g = Gain(gain_db=0)
    results = []
    for freq in [440, 880, 1320]:
        signal = np.sin(
            2 * np.pi * freq * np.arange(44100) / 44100
        ).astype(np.float32)
        results.append((g.process(signal, 44100), signal))
    for out, expected in results:
        np.testing.assert_allclose(out, expected, atol=1e-7)
