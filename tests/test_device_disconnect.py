import numpy as np
import pytest
from pedalboard.io import AudioStream


def _first_usable_output_device() -> str | None:
    """Try opening AudioStream with a few common device names; return the name if successful, or None if all fail."""
    for name in ("Built-in Output", "default"):
        try:
            with AudioStream(output_device_name=name):
                return name
        except Exception:
            continue
    return None


def test_write_raises_if_device_missing():
    buf = np.zeros(512, np.float32)

    real_device = _first_usable_output_device()
    if real_device is None:
        pytest.skip("No output device available on this host")

    fake = "___FAKE_DEVICE___"

    with pytest.raises(RuntimeError, match="disconnected"):
        with AudioStream(output_device_name=real_device) as s:
            # Simulate "device being unplugged"
            s._output_device_name = fake
            s.write(buf, 44_100)
