import numpy  # noqa: F401
from pedalboard_native.io import *  # noqa: F403, F401

from pedalboard_native.io import AudioStream as _AudioStream  # Explicitly alias for Ruff (linter)

from ._device_helpers import is_device_alive  # noqa: E402

# ----- monkey-patch -----
_original_write = _AudioStream.write  # noqa: F401


def _patched_write(self, samples, sample_rate):
    if not is_device_alive(self._output_device_name):
        raise RuntimeError(f"Output device '{self._output_device_name}' disconnected.")
    return _original_write(self, samples, sample_rate)


_AudioStream.write = _patched_write
# ------------------------
