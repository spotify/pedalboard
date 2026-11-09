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

import subprocess
import sys
import textwrap

import pytest

# Keep native crashes isolated so a regression cannot terminate the pytest run.
CHILD_SETUP = """
import sys

if sys.platform != "win32":
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

import numpy as np
from pedalboard_native._internal import copy_audio_stream_input_to_output  # type: ignore

num_frames = int(sys.argv[1])
"""


def run_routing_assertions(num_frames: int, assertions: str):
    result = subprocess.run(
        [
            sys.executable,
            "-c",
            CHILD_SETUP + textwrap.dedent(assertions),
            str(num_frames),
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 0, (
        f"Native audio routing subprocess exited with {result.returncode}\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )


@pytest.mark.parametrize("num_frames", [7, 512])
def test_zero_input_channels_silences_outputs(num_frames: int):
    run_routing_assertions(
        num_frames,
        """
        input_audio = np.empty((0, num_frames), dtype=np.float32)
        output_audio = np.full((2, num_frames), 0.75, dtype=np.float32)

        copy_audio_stream_input_to_output(input_audio, output_audio)

        np.testing.assert_array_equal(output_audio, np.zeros_like(output_audio))
        """,
    )


@pytest.mark.parametrize("num_frames", [7, 512])
def test_mono_input_is_replicated_to_both_outputs(num_frames: int):
    run_routing_assertions(
        num_frames,
        """
        input_audio = np.linspace(-0.75, 0.75, num_frames, dtype=np.float32)[None, :]
        original_input = input_audio.copy()
        output_audio = np.full((2, num_frames), 123.0, dtype=np.float32)

        copy_audio_stream_input_to_output(input_audio, output_audio)

        np.testing.assert_array_equal(output_audio[0], original_input[0])
        np.testing.assert_array_equal(output_audio[1], original_input[0])
        np.testing.assert_array_equal(input_audio, original_input)
        """,
    )


@pytest.mark.parametrize("num_frames", [7, 512])
def test_stereo_input_preserves_each_channel(num_frames: int):
    run_routing_assertions(
        num_frames,
        """
        left = np.linspace(-0.75, -0.25, num_frames, dtype=np.float32)
        right = np.linspace(0.25, 0.75, num_frames, dtype=np.float32)
        input_audio = np.stack([left, right])
        original_input = input_audio.copy()
        output_audio = np.full((2, num_frames), 123.0, dtype=np.float32)

        copy_audio_stream_input_to_output(input_audio, output_audio)

        np.testing.assert_array_equal(output_audio, original_input)
        np.testing.assert_array_equal(input_audio, original_input)
        """,
    )


@pytest.mark.parametrize("num_frames", [7, 512])
def test_zero_output_channels_leaves_input_unchanged(num_frames: int):
    run_routing_assertions(
        num_frames,
        """
        input_audio = np.linspace(-0.75, 0.75, num_frames, dtype=np.float32)[None, :]
        original_input = input_audio.copy()
        output_audio = np.empty((0, num_frames), dtype=np.float32)

        copy_audio_stream_input_to_output(input_audio, output_audio)

        assert output_audio.shape == (0, num_frames)
        np.testing.assert_array_equal(input_audio, original_input)
        """,
    )
