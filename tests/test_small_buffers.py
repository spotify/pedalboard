import io

import numpy as np
import pytest

from pedalboard import Gain, Pedalboard, Reverb
from pedalboard.io import AudioFile


@pytest.mark.parametrize("transpose", [False, True])
def test_small_buffers(transpose: bool):
    """
    Ensure that if we're processing a stream of audio, we don't get stuck
    with a square buffer that confuses our automatic shape detection.
    """
    board = Pedalboard([Reverb(), Gain()])
    buf = io.BytesIO(AudioFile.encode(np.zeros((2, 17)), 44100, "wav", 2))
    num_output_frames = 0
    with AudioFile(buf) as f:
        while f.tell() < f.frames:
            # Read three frames at a time; but the last frame will
            # end up with only two samples in it.
            _input = f.read(3)
            if transpose:
                _input = _input.T
            output = board(_input, f.samplerate)
            num_output_frames += output.shape[0 if transpose else 1]
    assert num_output_frames == 17
