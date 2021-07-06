import pytest
import numpy as np
from pedalboard import process, Gain, Compressor, Convolution


@pytest.mark.parametrize('shape', [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_no_transforms(shape, sr=44100):
    _input = np.random.rand(*shape).astype(np.float32)

    output = process(_input, sr, [])

    assert _input.shape == output.shape
    assert np.allclose(_input, output, rtol=0.0001)


@pytest.mark.parametrize('shape', [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_noise_gain(shape, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)

    # Use the Gain transform to scale down the noise by 6dB (0.5x)
    half_noise = process(full_scale_noise, sr, [Gain(-6)])

    assert full_scale_noise.shape == half_noise.shape
    assert np.allclose(full_scale_noise / 2.0, half_noise, rtol=0.01)


def test_throw_on_invalid_compressor_ratio(sr=44100):
    full_scale_noise = np.random.rand(sr, 1).astype(np.float32)

    # Should work:
    process(full_scale_noise, sr, [Compressor(ratio=1.1)])

    # Should fail:
    with pytest.raises(ValueError):
        Compressor(ratio=0.1)


def test_convolution_works(sr=44100, duration=10):
    full_scale_noise = np.random.rand(sr * duration).astype(np.float32)

    result = process(full_scale_noise, sr, [Convolution('./tests/impulse_response.wav', 0.5)])
    assert not np.allclose(full_scale_noise, result, rtol=0.1)


def test_throw_on_inaccessible_convolution_file():
    # Should work:
    Convolution('./tests/impulse_response.wav')

    # Should fail:
    with pytest.raises(RuntimeError):
        Convolution('./tests/missing_impulse_response.wav')
