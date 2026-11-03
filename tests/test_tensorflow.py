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

from .utils import import_framework


def test_can_be_called_in_tensorflow_data_pipeline():
    try:
        import tensorflow as tf  # type: ignore
    except ImportError:
        pytest.skip("TensorFlow not installed.")

    sr = 48000
    plugins = pedalboard.Pedalboard([pedalboard.Gain(), pedalboard.Reverb()])

    noise = np.random.default_rng(0).random(sr).astype(np.float32)

    def process_audio(audio):
        effected = tf.numpy_function(plugins.process, [audio, sr], tf.float32)
        # numpy_function cannot infer the shape of the samples returned by Python.
        return tf.ensure_shape(effected, audio.shape)

    ds = tf.data.Dataset.from_tensor_slices([noise]).map(process_audio)

    keras = import_framework("tensorflow.keras")
    model = keras.models.Sequential(
        [keras.layers.InputLayer(input_shape=(sr,)), keras.layers.Dense(1)]
    )
    # Train a dummy model just to ensure nothing explodes:
    model.compile(loss="mse")
    model.fit(ds.map(lambda effected: (effected, 1)).batch(1), epochs=10)

    assert np.allclose(plugins(noise, sr), np.array(list(ds.as_numpy_iterator())))
