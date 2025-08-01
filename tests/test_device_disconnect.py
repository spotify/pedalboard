import numpy as np
import pytest
from pedalboard.io import AudioStream


def test_write_raises_if_device_missing():
    buf = np.zeros(512, np.float32)
    fake = "___FAKE_DEVICE___"

    with pytest.raises(RuntimeError, match="disconnected"):
        with AudioStream() as s:  # 啟動預設裝置
            s._output_device_name = fake  # 假裝裝置「消失」
            s.write(buf, 44100)
