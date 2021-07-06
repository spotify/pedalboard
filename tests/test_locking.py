import random
import pytest
from concurrent.futures import ThreadPoolExecutor

import numpy as np

import pedalboard


@pytest.mark.parametrize('num_concurrent_chains', [10, 24, 50])
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
        pedalboards.append(pedalboard.Pedalboard(list(plugins), sample_rate=sr))

    for _ in range(0, num_concurrent_chains // 2):
        # Shuffle the list of plugins so that the order in which
        # we pass plugins into the method is non-deterministic:
        random.shuffle(plugins)
        pedalboards.append(pedalboard.Pedalboard(list(plugins), sample_rate=sr))

    futures = []
    with ThreadPoolExecutor(max_workers=num_concurrent_chains) as e:
        noise = np.random.rand(1, sr)
        for pb in pedalboards:
            futures.append(e.submit(pb.process, np.copy(noise)))

        # This will throw an exception if we exceed the timeout:
        processed = [future.result(timeout=2 * num_concurrent_chains) for future in futures]

    # Ensure that all of the pedalboard instances returned the same results,
    # as the plugins were the same (although randomly shuffled instances):

    first_result = processed[0]
    for other_result in processed[1:]:
        assert np.allclose(first_result, other_result)
