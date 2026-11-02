"""Regression tests for TensorFlow's eager tensor and memory-layout boundaries."""

import importlib
import os
from pathlib import Path
import subprocess
import sys
import textwrap

import numpy as np
import pytest

from .utils import import_framework, make_audio

tf = import_framework("tensorflow")
ptf = importlib.import_module("pedalboard.tensorflow")
pedalboard = importlib.import_module("pedalboard")
audio_io = importlib.import_module("pedalboard.io")

SAMPLE_RATE = 44100


@pytest.fixture
def legacy_eager_audio(monkeypatch):
    audio = make_audio(frames=128)
    with tf.device("/CPU:0"):
        tensor = tf.constant(audio)
    # Older TensorFlow eager tensors expose .numpy(), but no Python DLPack protocol.
    for name in ("__dlpack__", "__dlpack_device__"):
        for cls in type(tensor).__mro__:
            if name in vars(cls):
                monkeypatch.delattr(cls, name)
    assert not hasattr(tensor, "__dlpack__")
    return tensor, audio


def test_plugin_accepts_legacy_tensorflow_eager_tensor(legacy_eager_audio):
    tensor, audio = legacy_eager_audio
    expected = pedalboard.Gain(gain_db=-6)(audio, SAMPLE_RATE)
    result = ptf.Gain(gain_db=-6)(tensor, SAMPLE_RATE)
    assert isinstance(result, tf.Tensor)
    np.testing.assert_allclose(result.numpy(), expected, rtol=1e-6, atol=1e-6)


def test_audiofile_write_accepts_legacy_tensorflow_eager_tensor(legacy_eager_audio, tmp_path):
    tensor, audio = legacy_eager_audio
    path = str(tmp_path / "legacy.wav")
    with ptf.AudioFile(path, "w", SAMPLE_RATE, audio.shape[0]) as f:
        f.write(tensor)
    with audio_io.AudioFile(path) as f:
        np.testing.assert_allclose(f.read(f.frames), audio, atol=1e-4)


def test_unaligned_numpy_output_can_be_used_by_tensorflow_concat():
    # TensorFlow may abort the process when an imported buffer lacks its required
    # alignment. Keep the kernel execution in a subprocess so pytest survives.
    script = textwrap.dedent(
        """
        import os
        if os.name != "nt":
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        import numpy as np
        import tensorflow as tf
        from pedalboard.tensorflow import _from_numpy

        storage = np.empty(2 * 128 + 16, dtype=np.float32)
        offset = (1 - storage.ctypes.data // storage.itemsize) % 16
        audio = storage[offset:offset + 2 * 128].reshape(2, 128)
        assert audio.ctypes.data % 64 == 4
        audio[:] = np.arange(audio.size, dtype=np.float32).reshape(audio.shape) / 512
        tensor = _from_numpy(audio)
        assert isinstance(tensor, tf.Tensor)
        result = tf.concat([tensor, tensor], axis=1).numpy()
        np.testing.assert_array_equal(result, np.concatenate([audio, audio], axis=1))
        """
    )
    result = subprocess.run(
        [sys.executable, "-c", script],
        cwd=Path(__file__).resolve().parent.parent,
        env={**os.environ, "TF_NUM_INTRAOP_THREADS": "1", "TF_NUM_INTEROP_THREADS": "1"},
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert result.returncode == 0, result.stdout + result.stderr
