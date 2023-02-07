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
import pytest
import platform
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
# This will run on macOS and probably Windows as long as at least one audio device is available.
@pytest.mark.parametrize("input_device_name", INPUT_DEVICE_NAMES)
@pytest.mark.parametrize("output_device_name", pedalboard.io.AudioStream.output_device_names)
@pytest.mark.skipif(platform.system() == "Linux", reason="AudioStream not supported on Linux yet.")
def test_create_stream(input_device_name: str, output_device_name: str):
    try:
        stream = pedalboard.io.AudioStream(
            input_device_name, output_device_name, allow_feedback=True
        )
    except Exception as e:
        if any(substr in str(e) for substr in ACCEPTABLE_ERRORS_ON_CI):
            return
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
        for _ in range(0, 100):
            time.sleep(0.01)
            stream.plugins.append(pedalboard.Gain(gain_db=-120))
        for i in reversed(range(len(stream.plugins))):
            time.sleep(0.01)
            del stream.plugins[i]
        assert stream.running
    assert not stream.running


@pytest.mark.skipif(platform.system() != "Linux", reason="Test platform is not Linux.")
def test_create_stream_fails_on_linux():
    with pytest.raises(RuntimeError):
        pedalboard.io.AudioStream("input", "output")
