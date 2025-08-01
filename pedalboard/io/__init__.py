# 預載 NumPy，避免 Pybind11 懶載入死鎖
import numpy  # noqa: F401

# 星號匯入仍保留
from pedalboard_native.io import *  # noqa: F403, F401

# 顯式匯入 AudioStream，給 Ruff 一個確定參照
from pedalboard_native.io import AudioStream as _AudioStream  # noqa: F401

# ------------------------------------------------------------------
# Monkey-patch AudioStream.write：裝置斷線時拋 RuntimeError
# ------------------------------------------------------------------

from ._device_helpers import is_device_alive  # noqa: E402

_original_write = _AudioStream.write  # 保存原方法，Ruff 不再抱怨 F405


def _patched_write(self, samples, sample_rate):
    if not is_device_alive(self._output_device_name):
        raise RuntimeError(f"Output device '{self._output_device_name}' disconnected.")
    return _original_write(self, samples, sample_rate)


_AudioStream.write = _patched_write  # 套用補丁
# ------------------------------------------------------------------
