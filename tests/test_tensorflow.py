#! /usr/bin/env python
#
# Copyright 2021 Spotify AB
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

import numpy as np
import pytest

import pedalboard


def test_can_be_called_in_tensorflow_data_pipeline():
    try:
        import tensorflow as tf  # type: ignore
    except ImportError:
        pytest.skip("TensorFlow not installed.")

    sr = 48000
    plugins = pedalboard.Pedalboard([pedalboard.Gain(), pedalboard.Reverb()])

    noise = np.random.rand(sr).astype(np.float32)

    ds = tf.data.Dataset.from_tensor_slices([noise]).map(
        lambda audio: tf.numpy_function(plugins.process, [audio, sr], tf.float32)
    )

    model = tf.keras.models.Sequential(
        [tf.keras.layers.InputLayer(input_shape=(sr,)), tf.keras.layers.Dense(1)]
    )
    # Train a dummy model just to ensure nothing explodes:
    model.compile(loss="mse")
    model.fit(ds.map(lambda effected: (effected, 1)).batch(1), epochs=10)

    assert np.allclose(plugins(noise, sr), np.array([tensor.numpy() for tensor in ds]))
