import numpy as np

import tensorflow as tf

import pedalboard


def test_can_be_called_in_tensorflow_data_pipeline():
    sr = 48000
    plugins = pedalboard.Pedalboard([pedalboard.Gain(), pedalboard.Reverb()], sample_rate=sr)

    noise = np.random.rand(sr)
    ds = tf.data.Dataset.from_tensor_slices([noise]).map(
        lambda audio: tf.numpy_function(plugins.process, [audio], tf.float32)
    )

    model = tf.keras.models.Sequential(
        [tf.keras.layers.InputLayer(input_shape=(sr,)), tf.keras.layers.Dense(1)]
    )
    # Train a dummy model just to ensure nothing explodes:
    model.compile(loss='mse')
    model.fit(ds.map(lambda effected: (effected, 1)).batch(1), epochs=10)

    assert np.allclose(plugins.process(noise), np.array([tensor.numpy() for tensor in ds]))
