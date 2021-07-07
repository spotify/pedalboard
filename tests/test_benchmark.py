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


import time

import pytest
import numpy as np

import sox
import pedalboard


class timer(object):
    def __enter__(self):
        self.start_time = time.time()
        return self

    def __exit__(self, type, value, traceback):
        self.end_time = time.time()

    def __float__(self):
        return float(self.end_time - self.start_time)

    def __str__(self):
        return str(float(self))

    def __repr__(self):
        return str(float(self))


@pytest.mark.skip
def test_pysox_performance_difference():
    transformer = sox.Transformer()
    transformer.reverb()

    sr = 48000
    noise = np.random.rand(2, sr).astype(np.float32)

    pysox_measurements = []
    for _ in range(0, 10):
        with timer() as pysox_time_taken:
            transformer.build_array(input_array=noise, sample_rate_in=sr)
        pysox_measurements.append(float(pysox_time_taken))

    pedalboard_measurements = []
    for _ in range(0, 10):
        with timer() as pedalboard_time_taken:
            pedalboard.Reverb()(noise, sample_rate=sr)
        pedalboard_measurements.append(float(pedalboard_time_taken))

    average_pysox_time = np.mean(pysox_measurements)
    average_pedalboard_time = np.mean(pedalboard_measurements)

    # In local tests, Pedalboard is about 300x faster than PySoX.
    # This test ensures we're at least 100x faster to account for
    # variations across test run environments.
    assert average_pysox_time / average_pedalboard_time > 100
