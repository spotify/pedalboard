import numpy as np
import pytest
from pedalboard.io import AudioStream


def test_write_should_raise():
    buf = np.zeros(512, np.float32)
    fake = "___FAKE_DEVICE___"  # 不存在的裝置名

    # 現階段程式不會拋 RuntimeError，因此這個測試會失敗
    with pytest.raises(RuntimeError):
        with AudioStream() as s:  # 不指定，讓它挑預設裝置
            s._output_device_name = fake  # 手動改成假裝置
            s.write(buf, 44100)
