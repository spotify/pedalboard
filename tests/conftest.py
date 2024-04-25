import os


def pytest_collection_modifyitems(session, config, items):
    """
    Allow for parallel test execution on GitHub Actions without third-party plugins.
    """
    num_test_workers = int(os.getenv("NUM_TEST_WORKERS", "1"))
    test_worker_index = int(os.getenv("TEST_WORKER_INDEX", "1")) - 1
    if num_test_workers == 1:
        return
    print(f"\n\nRunning tests for worker {test_worker_index + 1} of {num_test_workers}.")
    print(
        f"This process will only run one out of every {num_test_workers:,} tests, offset by {test_worker_index}.\n"
    )
    for i, item in enumerate(list(items)):
        if (i + test_worker_index) % num_test_workers != 0:
            items.remove(item)
