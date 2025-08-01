# In certain situations (including when using asyncio), Pedalboard's C++ code will
# implicitly and lazily import NumPy via its Pybind11 bindings. This lazy import
# can sometimes (!) cause a deadlock on the GIL if multiple threads are attempting
# it simultaneously. To work around this, we preload Numpy into the current Python
# process so that Pybind11's lazy import is effectively a no-op.
import numpy  # noqa
from pedalboard_native.io import *  # noqa: F403, F401 # type: ignore

# ➜ 新增這一行，讓 Ruff 確定 AudioStream 是誰
from pedalboard.io import AudioStream as _AudioStream

# ----- Monkey-patch AudioStream.write：裝置斷線時丟 RuntimeError -----
from ._device_helpers import is_device_alive  # noqa: E402

_original_write = AudioStream.write  # 保存原本的方法


def _patched_write(self, samples, sample_rate):
    if not is_device_alive(self._output_device_name):
        raise RuntimeError(f"Output device '{self._output_device_name}' disconnected.")
    return _original_write(self, samples, sample_rate)


AudioStream.write = _patched_write
# --------------------------------------------------------------------
