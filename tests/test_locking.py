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


import random
import pytest
from concurrent.futures import ThreadPoolExecutor

import numpy as np

import pedalboard


@pytest.mark.parametrize("num_concurrent_chains", [2, 10, 20])
def test_multiple_threads_using_same_plugin_instances(num_concurrent_chains: int):
    """
    Instantiate a large number of stateful plugins, then run audio through them
    in randomly chosen orders, ensuring that the results are the same each time.
    """
    sr = 48000
    plugins = sum(
        [[pedalboard.Reverb()] for _ in range(100)],
        [],
    )

    pedalboards = []
    for _ in range(0, num_concurrent_chains // 2):
        # Reverse the list of plugins so that the order in which
        # we take locks (if we didn't have logic for it) would be
        # the pathologically worst-case.
        plugins.reverse()
        pedalboards.append(pedalboard.Pedalboard(list(plugins)))

    for _ in range(0, num_concurrent_chains // 2):
        # Shuffle the list of plugins so that the order in which
        # we pass plugins into the method is non-deterministic:
        random.shuffle(plugins)
        pedalboards.append(pedalboard.Pedalboard(list(plugins)))

    futures = []
    with ThreadPoolExecutor(max_workers=num_concurrent_chains) as e:
        noise = np.random.rand(1, sr)
        for pb in pedalboards:
            futures.append(e.submit(pb.process, np.copy(noise), sample_rate=sr))

        # This will throw an exception if we exceed the timeout:
        processed = [future.result(timeout=2 * num_concurrent_chains) for future in futures]

    # Ensure that all of the pedalboard instances returned the same results,
    # as the plugins were the same (although randomly shuffled instances):

    first_result = processed[0]
    for other_result in processed[1:]:
        assert np.allclose(first_result, other_result)
