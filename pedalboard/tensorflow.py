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


"""Pedalboard's audio API, working with TensorFlow tensors.

    from pedalboard.io import AudioFile    ->  from pedalboard.tensorflow import AudioFile
    from pedalboard import Reverb          ->  from pedalboard.tensorflow import Reverb

This module provides Pedalboard's audio entry points: every plugin class (as a subclass
of the original), ``AudioFile``, ``process``, ``time_stretch`` and ``load_plugin``. Audio is
accepted as any CPU array supporting DLPack (torch, JAX, TensorFlow, NumPy, ...) and returned
as a ``tf.Tensor``. Everything else (e.g.
``AudioStream``, base classes and constants) is imported from ``pedalboard`` and
``pedalboard.io`` as usual.

Pedalboard's effects aren't differentiable: returned arrays have no gradient history.

Requires ``pip install pedalboard[tensorflow]``.
"""

from __future__ import annotations

import numpy as np
import tensorflow as tf

from pedalboard._array_interop import make_namespace


def _from_numpy(array: np.ndarray) -> tf.Tensor:
    return tf.experimental.dlpack.from_dlpack(array.__dlpack__())


globals().update(make_namespace(__name__, _from_numpy))
