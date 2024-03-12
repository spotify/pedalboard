#! /usr/bin/env python
#
# Copyright 2024 Spotify AB
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
import threading
from concurrent.futures import ThreadPoolExecutor
from functools import lru_cache
from io import BytesIO

import numpy as np
import pytest

from pedalboard import Resample
from pedalboard.io import AudioFile

try:
    from contextlib import nullcontext
except ImportError:
    from contextlib import suppress as nullcontext

# The number of iterations to run the test for with and
# without a lock around each call to AudioFile's methods:
MAX_ITERATIONS = {"lock": 10, "no lock": 100_000}


class RandomError(Exception):
    pass


@lru_cache(maxsize=None)
def big_buffer() -> np.ndarray:
    return np.random.rand(44_100 * 60 * 10)


def patch_bytes_io(io: BytesIO, should_error: threading.Event):
    old_read = io.read

    def read_and_sometimes_throw(*args, **kwargs):
        if should_error.is_set():
            raise RandomError()
        return old_read(*args, **kwargs)

    io.read = read_and_sometimes_throw

    old_write = io.write

    def write_and_sometimes_throw(*args, **kwargs):
        if should_error.is_set():
            raise RandomError()
        return old_write(*args, **kwargs)

    io.write = write_and_sometimes_throw
    return io


def one_minute_buffer(allow_memoryview: bool, should_error: threading.Event) -> BytesIO:
    buf = BytesIO()
    with AudioFile(buf, "w", 44100, 1, format="wav") as f:
        f.write(np.random.rand(44100 * 60))
    buf.seek(0)
    if not allow_memoryview:
        # Avoid triggering the fast-path for memoryview, which releases the GIL:
        buf.getbuffer = lambda: False
    return patch_bytes_io(buf, should_error)


@pytest.mark.parametrize("num_workers", [2, 4])
@pytest.mark.parametrize("allow_memoryview", [True, False])
@pytest.mark.parametrize("locking_scheme", ["lock", "no lock"])
@pytest.mark.parametrize("sample_rate", [44100, 22050])
@pytest.mark.parametrize("raise_exceptions", [False, True])
def test_simultaneous_reads_from_the_same_file(
    num_workers: int,
    allow_memoryview: bool,
    locking_scheme: str,
    sample_rate: float,
    raise_exceptions: bool,
):
    """
    Ensure that two concurrent readers of the same file don't cause
    the Python process to deadlock on the GIL or the AudioFile object's
    internal lock(s).

    Note: This is still nonsensical (you would never want this as the
    results are garbage) but it should at least not deadlock the
    process.

    raise_exceptions will randomly cause exceptions to be thrown by the
    underlying Python file-like object, which should be propagated to
    the caller regardless of the locking scheme used.
    """
    should_error = threading.Event()
    buf = one_minute_buffer(allow_memoryview, should_error)

    exception_occurred = threading.Event()
    num_iterations = MAX_ITERATIONS[locking_scheme]

    def do_work(af, lock):
        operations = [
            lambda: af.closed,
            lambda: af.samplerate,
            lambda: af.num_channels,
            lambda: af.frames,
            lambda: af.seek(0),
            lambda: af.tell(),
            lambda: af.seekable(),
            lambda: af.seek(22050),
            lambda: af.read(22050),
        ]
        # ResampledReadableAudioFile does not have this method:
        if hasattr(af, "read_raw"):
            operations.append(lambda: af.read_raw(22050))
        try:
            for _ in range(num_iterations):
                if exception_occurred.is_set():
                    break
                with lock or nullcontext():
                    try:
                        random.choice(operations)()
                    except RandomError:
                        pass
        except Exception as e:
            exception_occurred.set()
            raise e

    lock = threading.Lock() if locking_scheme == "lock" else None
    with ThreadPoolExecutor(num_workers) as e, AudioFile(buf).resampled_to(
        sample_rate, Resample.Quality.ZeroOrderHold
    ) as af:
        if raise_exceptions:
            should_error.set()
        futures = [e.submit(do_work, af, lock) for _ in range(num_workers)]

        if lock:
            for future in futures:
                future.result()
        else:
            # Without a lock, we should expect a RuntimeError:
            with pytest.raises(RuntimeError, match=r"thread"):
                for future in futures:
                    future.result()

        # Ensure that all threads are done:
        for future in futures:
            future.exception()
        should_error.clear()


@pytest.mark.parametrize("num_workers", [2, 4])
@pytest.mark.parametrize("locking_scheme", ["lock", "no lock"])
@pytest.mark.parametrize("raise_exceptions", [False, True])
def test_simultaneous_writes_to_the_same_file(
    num_workers: int,
    locking_scheme: str,
    raise_exceptions: bool,
):
    """
    Ensure that two concurrent writers to the same file don't cause
    the Python process to deadlock on the GIL or the AudioFile object's
    internal lock(s).

    Note: This is still nonsensical (you would never want this as the
    results are garbage) but it should at least not deadlock the
    process.

    raise_exceptions will randomly cause exceptions to be thrown by the
    underlying Python file-like object, which should be propagated to
    the caller regardless of the locking scheme used.
    """

    big_buffer()  # Pre-cache the buffer to avoid it being created in the threads

    should_error = threading.Event()
    exception_occurred = threading.Event()
    num_iterations = MAX_ITERATIONS[locking_scheme]

    def do_work(af, lock):
        operations = [
            lambda: af.tell(),
            lambda: af.flush(),
            lambda: af.closed,
            lambda: af.samplerate,
            lambda: af.num_channels,
            lambda: af.frames,
            lambda: af.write(big_buffer()),
        ]
        try:
            for _ in range(num_iterations):
                if exception_occurred.is_set():
                    break
                with lock or nullcontext():
                    try:
                        random.choice(operations)()
                    except RandomError:
                        pass
                    except RuntimeError as e:
                        if "Unable to write" in repr(e):
                            # TODO: We _should_ get a RandomError here, but
                            # the Python exception can be cleared by another
                            # thread before we have a chance to raise it.
                            # Properly fixing this would require a lot of
                            # refactoring (to hold the GIL indefinitely
                            # once an exception is thrown), so for now we just
                            # put up with the generic error message in this
                            # extremely rare case.
                            pass
                        else:
                            raise
        except Exception as e:
            exception_occurred.set()
            raise e

    lock = threading.Lock() if locking_scheme == "lock" else None
    buf = patch_bytes_io(BytesIO(), should_error)
    with ThreadPoolExecutor(num_workers) as e, AudioFile(buf, "w", 44100, 1, format="wav") as af:
        if raise_exceptions:
            should_error.set()
        futures = [e.submit(do_work, af, lock) for _ in range(num_workers)]

        if lock:
            for future in futures:
                future.result()
        else:
            # Without a lock, we should expect a RuntimeError eventually
            # once two or more threads try to write at the same time:
            with pytest.raises(RuntimeError, match=r"thread"):
                for future in futures:
                    future.result()

        for future in futures:
            future.exception()
        should_error.clear()


def test_bytesio_writes_can_happen_without_gil():
    """
    If we pass in a BytesIO object that is large enough to contain
    the entire output, we should be able to write to it without
    taking the GIL at all:
    """

    buf = BytesIO()
    # Resize the initial file to 1MB to avoid needing
    # to take the GIL to reallocate:
    #
    # Note that we can't just do buf.truncate(),
    # due to https://bugs.python.org/issue27261
    buf_size = 1024 * 1024
    buf.write(b"\x00" * buf_size)
    buf.seek(0)
    assert buf.tell() == 0
    assert len(buf.getbuffer()) == buf_size

    original_write = buf.write

    writes = []

    def write(data):
        writes.append(data)
        return original_write(data)

    buf.write = write

    random_audio = np.random.rand(44100)
    with AudioFile(buf, "w", 44100, 1, format="wav") as af:
        # Only consider calls to buf.write that happen after the file is already open:
        writes.clear()
        af.write(random_audio)
        buf.write = original_write
    assert buf.tell() < len(buf.getbuffer())
    buf.truncate()
    assert buf.tell() <= len(buf.getbuffer())
    assert not writes, (
        "Expected no GIL-locked writes to have happened after opening the "
        f"file, but .write was called {len(writes):,} times."
    )


def test_bytesio_resizes_as_necessary():
    """
    If we pass in a BytesIO object that is _not_ large enough to
    contain the entire output, we should be able to write to it
    without taking the GIL until we need to resize the buffer.

    At that point, the buffer should get resized to the nearest
    power of two:
    """

    buf = BytesIO()
    original_write = buf.write

    writes = []

    def write(data):
        writes.append(data)
        return original_write(data)

    buf.write = write
    random_audio = np.random.rand(44100)
    with AudioFile(buf, "w", 44100, 1, format="wav") as af:
        initial_bytes_written = buf.tell()
        writes.clear()
        af.write(random_audio)

    assert buf.tell() <= len(
        buf.getbuffer()
    ), "Buffer should have been truncated as it was resized."

    # Each write should double the size of the buffer (ignoring small writes at the end when closing the file)
    assert [len(x) for x in writes][:4] == [16384 - initial_bytes_written, 16384, 32768, 65536]
