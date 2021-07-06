import pytest
import numpy as np
from pedalboard import Pedalboard, Gain, Compressor, Convolution


@pytest.mark.parametrize('shape', [(44100,), (44100, 1), (44100, 2), (1, 4), (2, 4)])
def test_no_transforms(shape, sr=44100):
    _input = np.random.rand(*shape).astype(np.float32)

    output = Pedalboard([], sr).process(_input)

    assert _input.shape == output.shape
    assert np.allclose(_input, output, rtol=0.0001)


def test_fail_on_invalid_plugin():
    with pytest.raises(TypeError):
        Pedalboard(['I want a reverb please'], 44100)


def test_fail_on_invalid_sample_rate():
    with pytest.raises(TypeError):
        Pedalboard([], 'fourty four one hundred')
    with pytest.raises(TypeError):
        Pedalboard([]).process([], 'fourty four one hundred')


def test_fail_on_invalid_buffer_size():
    with pytest.raises(TypeError):
        Pedalboard([]).process([], 44100, 'very big buffer please')


def test_repr():
    sr = 44100
    gain = Gain(-6)
    value = repr(Pedalboard([gain], sr))
    # Allow flexibility; all we care about is that these values exist in the repr.
    assert "Pedalboard" in value
    assert str(sr) in value
    assert "plugins=" in value
    assert repr(gain) in value
    assert "sample_rate=" in value


def test_is_list_like():
    sr = 44100
    gain = Gain(-6)

    assert len(Pedalboard([gain], sr)) == 1
    assert len(Pedalboard([gain, Gain(-6)], sr)) == 2

    pb = Pedalboard([gain], sr)
    assert len(pb) == 1

    # Allow adding elements, like a list:
    pb.append(Gain())
    assert len(pb) == 2

    with pytest.raises(TypeError):
        pb.append("not a plugin")

    # Allow deleting elements, like a list:
    del pb[1]
    assert len(pb) == 1

    # Allow getting elements, like a list:
    assert pb[0] is gain

    # Allow setting elements, like a list:
    pb[0] = gain
    assert pb[0] is gain

    with pytest.raises(TypeError):
        pb[0] = "not a plugin"


def test_process_validates_sample_rate():
    sr = 44100
    full_scale_noise = np.random.rand(sr, 1).astype(np.float32)

    # Sample rate can be provided in the constructor:
    pb = Pedalboard([Gain(-6)], sr).process(full_scale_noise)

    # ...or in the `process` call:
    pb = Pedalboard([Gain(-6)]).process(full_scale_noise, sr)

    # But if not passed in either, an exception will be raised:
    pb = Pedalboard([Gain(-6)])
    with pytest.raises(ValueError):
        pb.process(full_scale_noise)


@pytest.mark.parametrize(
    'shape,buffer_size',
    sum(
        [
            [
                ((44100,), buffer_size),
                ((44100, 1), buffer_size),
                ((44100, 2), buffer_size),
                ((1, 4), buffer_size),
                ((2, 4), buffer_size),
            ]
            for buffer_size in (None, 128, 4096, 2 ** 16)
        ],
        [],
    ),
)
def test_noise_gain(shape, buffer_size, sr=44100):
    full_scale_noise = np.random.rand(*shape).astype(np.float32)

    # Use the Gain transform to scale down the noise by 6dB (0.5x)
    half_noise = Pedalboard([Gain(-6)], sr).process(full_scale_noise, buffer_size=buffer_size)
    assert full_scale_noise.shape == half_noise.shape
    assert np.allclose(full_scale_noise / 2.0, half_noise, rtol=0.01)


def test_throw_on_invalid_compressor_ratio(sr=44100):
    full_scale_noise = np.random.rand(sr, 1).astype(np.float32)

    # Should work:
    Pedalboard([Compressor(ratio=1.1)], sr).process(full_scale_noise)

    # Should fail:
    with pytest.raises(ValueError):
        Compressor(ratio=0.1)


def test_convolution_works(sr=44100, duration=10):
    full_scale_noise = np.random.rand(sr * duration).astype(np.float32)

    result = Pedalboard([Convolution('./tests/impulse_response.wav', 0.5)], sr).process(
        full_scale_noise
    )
    assert not np.allclose(full_scale_noise, result, rtol=0.1)


def test_throw_on_inaccessible_convolution_file():
    # Should work:
    Convolution('./tests/impulse_response.wav')

    # Should fail:
    with pytest.raises(RuntimeError):
        Convolution('./tests/missing_impulse_response.wav')
