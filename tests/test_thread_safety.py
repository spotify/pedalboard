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
from concurrent.futures import ThreadPoolExecutor

import numpy as np

import pedalboard


TESTABLE_PLUGINS = []
for plugin_class in pedalboard.Plugin.__subclasses__():
    try:
        plugin_class()
        TESTABLE_PLUGINS.append(plugin_class)
    except Exception:
        pass


@pytest.mark.parametrize("plugin_class", TESTABLE_PLUGINS)
def test_concurrent_processing_produces_identical_audio(plugin_class):
    num_concurrent_plugins = 10
    sr = 48000
    plugins = [plugin_class() for _ in range(num_concurrent_plugins)]

    noise = np.random.rand(1, sr * 10)
    expected_output = plugins[0].process(noise, sr)

    # Test first to ensure the plugin is deterministic (some might not be):
    if not np.allclose(expected_output, plugins[0].process(noise, sr)):
        return

    futures = []
    with ThreadPoolExecutor(max_workers=num_concurrent_plugins) as e:
        for plugin in plugins:
            futures.append(e.submit(plugin.process, noise, sample_rate=sr))

        # This will throw an exception if we exceed the timeout:
        processed = [future.result(timeout=2 * num_concurrent_plugins) for future in futures]

    for result in processed:
        np.testing.assert_allclose(expected_output, result)
