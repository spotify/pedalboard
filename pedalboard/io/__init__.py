# In certain situations (including when using asyncio), Pedalboard's C++ code will
# implicitly and lazily import NumPy via its Pybind11 bindings. This lazy import
# can sometimes (!) cause a deadlock on the GIL if multiple threads are attempting
# it simultaneously. To work around this, we preload Numpy into the current Python
# process so that Pybind11's lazy import is effectively a no-op.
import numpy  # noqa
from pedalboard_native.io import *  # noqa: F403, F401
