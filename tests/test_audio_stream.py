#! /usr/bin/env python
#
# Copyright 2023 Spotify AB
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

import time

import numpy as np
import pytest

import pedalboard

# Very silly: even just creating an AudioStream object that reads from an `iPhone Microphone``
# will cause a locally-present iPhone to emit a sound. Running `pytest` on my laptop makes my
# phone ding.
INPUT_DEVICE_NAMES_TO_SKIP = {"iPhone Microphone"}
INPUT_DEVICE_NAMES = [
    n
    for n in pedalboard.io.AudioStream.input_device_names
    if not any(substr in n for substr in INPUT_DEVICE_NAMES_TO_SKIP)
]

ACCEPTABLE_ERRORS_ON_CI = {"No driver"}


# Note: this test may do nothing on CI, because we don't have mock audio devices available.
# This will run on Linux, macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.parametrize("input_device_name", INPUT_DEVICE_NAMES)
@pytest.mark.parametrize("output_device_name", pedalboard.io.AudioStream.output_device_names)
def test_create_stream(input_device_name: str, output_device_name: str):
    try:
        stream = pedalboard.io.AudioStream(
            input_device_name, output_device_name, allow_feedback=True
        )
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            raise pytest.skip(str(e))
        raise

    assert stream is not None
    assert input_device_name in repr(stream)
    assert output_device_name in repr(stream)
    assert not stream.running
    assert isinstance(stream.plugins, pedalboard.Chain)
    with stream:
        assert stream.running

        # Ensure that modifying a running stream does not crash:
        stream.plugins.append(pedalboard.Gain(gain_db=-120))
        for _ in range(0, 10):
            stream.plugins.append(pedalboard.Gain(gain_db=-120))
        for i in reversed(range(len(stream.plugins))):
            del stream.plugins[i]
        assert stream.running
    assert not stream.running


# Note: this test may do nothing on CI, because we don't have mock audio devices available.
# This will run on Linux, macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.skipif(
    (
        pedalboard.io.AudioStream.default_output_device_name == "Null Audio Device"
        or pedalboard.io.AudioStream.default_output_device_name is None
    ),
    reason="Tests do not work with a null audio device.",
)
def test_write_to_stream():
    try:
        stream = pedalboard.io.AudioStream(
            None, pedalboard.io.AudioStream.default_output_device_name
        )
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            raise pytest.skip(str(e))
        raise

    assert stream is not None
    assert not stream.running
    with stream:
        assert stream.running
        stream.write(np.zeros((2, 1024), dtype=np.float32), stream.sample_rate)
    assert not stream.running


# Note: this test may do nothing on CI, because we don't have mock audio devices available.
# This will run on Linux, macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.skipif(
    (
        pedalboard.io.AudioStream.default_output_device_name == "Null Audio Device"
        or pedalboard.io.AudioStream.default_output_device_name is None
    ),
    reason="Tests do not work with a null audio device.",
)
def test_write_to_stream_without_opening():
    try:
        stream = pedalboard.io.AudioStream(
            None, pedalboard.io.AudioStream.default_output_device_name
        )
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            raise pytest.skip(str(e))
        raise

    assert stream is not None
    assert not stream.running
    stream.write(np.zeros((2, 1024), dtype=np.float32), stream.sample_rate)
    assert not stream.running
    assert stream.buffered_input_sample_count is None


# Note: this test may do nothing on CI, because we don't have mock audio devices available.
# This will run on Linux, macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.skipif(
    (
        pedalboard.io.AudioStream.default_input_device_name == "Null Audio Device"
        or pedalboard.io.AudioStream.default_input_device_name is None
    ),
    reason="Tests do not work with a null audio device.",
)
def test_read_from_stream():
    try:
        stream = pedalboard.io.AudioStream(pedalboard.io.AudioStream.default_input_device_name)
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            raise pytest.skip(str(e))
        raise

    assert stream is not None
    assert not stream.running
    result = stream.read(1)
    assert result.shape == (stream.num_input_channels, 1)
    assert not stream.running
    assert stream.buffered_input_sample_count == 0


# Note: this test may do nothing on CI, because we don't have mock audio devices available.
# This will run on Linux, macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.skipif(
    (
        pedalboard.io.AudioStream.default_input_device_name == "Null Audio Device"
        or pedalboard.io.AudioStream.default_input_device_name is None
    ),
    reason="Tests do not work with a null audio device.",
)
def test_read_from_stream_measures_dropped_frames():
    try:
        stream = pedalboard.io.AudioStream(pedalboard.io.AudioStream.default_input_device_name)
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            raise pytest.skip(str(e))
        raise

    assert stream is not None
    with stream:
        if stream.sample_rate == 0:
            raise pytest.skip("Sample rate of default audio device is 0")
        assert stream.running
        assert stream.dropped_input_frame_count == 0
        time.sleep(5 * stream.buffer_size / stream.sample_rate)
        assert stream.buffered_input_sample_count > 0
        dropped_count = stream.dropped_input_frame_count
        assert dropped_count > 0
    # The input buffer was cleared on __exit__, so the buffer count should be zero:
    assert stream.buffered_input_sample_count == 0

    # ...but we should still know how many frames were dropped:
    assert stream.dropped_input_frame_count == dropped_count
