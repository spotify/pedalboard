#! /usr/bin/env python
#
# Copyright 2026 Spotify AB
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

"""
Torch-specific tests for pedalboard.torch. Behaviour shared with pedalboard.jax
and pedalboard.tensorflow is tested in test_ml_frameworks.py.
"""

import importlib

import numpy as np
import pytest

from .utils import import_framework, make_audio

torch = import_framework("torch")

# Imported after the check above, so that this file is skipped if torch isn't installed:
import pedalboard  # noqa: E402

pt = importlib.import_module("pedalboard.torch")

SAMPLE_RATE = 44100


def assert_tensor_matches(result, expected: np.ndarray):
    assert isinstance(result, torch.Tensor)
    assert result.device.type == "cpu"
    assert result.dtype == torch.from_numpy(expected).dtype
    assert tuple(result.shape) == expected.shape
    np.testing.assert_allclose(result.detach().numpy(), expected, rtol=1e-6, atol=1e-6)


def test_plugin_accepts_mono_1d_tensor():
    audio = make_audio(channels=1)[0]
    expected = pedalboard.Gain(gain_db=3)(audio, SAMPLE_RATE)
    result = pt.Gain(gain_db=3)(torch.from_numpy(audio.copy()), SAMPLE_RATE)
    assert_tensor_matches(result, expected)


def test_plugin_accepts_requires_grad_tensor_and_output_has_no_grad():
    audio = make_audio()
    expected = pedalboard.Gain(gain_db=3)(audio, SAMPLE_RATE)
    x = torch.from_numpy(audio.copy()).requires_grad_(True)
    result = pt.Gain(gain_db=3)(x, SAMPLE_RATE)
    assert_tensor_matches(result, expected)
    assert not result.requires_grad


def test_plugin_accepts_non_contiguous_view():
    audio = make_audio()
    x = torch.from_numpy(audio.copy())[:, ::2]
    assert not x.is_contiguous()
    expected = pedalboard.Gain(gain_db=3)(np.ascontiguousarray(audio[:, ::2]), SAMPLE_RATE)
    result = pt.Gain(gain_db=3)(x, SAMPLE_RATE)
    assert_tensor_matches(result, expected)


def _accelerator_device():
    if torch.cuda.is_available():
        return "cuda"
    mps = getattr(torch.backends, "mps", None)
    if mps is not None and mps.is_available():
        return "mps"
    return None


def test_plugin_rejects_non_cpu_tensor_with_value_error():
    device = _accelerator_device()
    if device is None:
        pytest.skip("No CUDA or MPS device available")
    x = torch.from_numpy(make_audio()).to(device)
    with pytest.raises(ValueError, match="CPU"):
        pt.Gain(gain_db=3)(x, SAMPLE_RATE)


def test_unsupported_dtype_error_does_not_mention_cpu():
    x = torch.from_numpy(make_audio()).to(torch.bfloat16)
    with pytest.raises((TypeError, ValueError)) as excinfo:
        pt.Gain(gain_db=3)(x, SAMPLE_RATE)
    message = str(excinfo.value)
    assert "CPU" not in message.upper()
    assert "dtype" in message.lower() or "bfloat16" in message.lower()
