"""Run the real AudioStream against deterministic, hardware-free JUCE devices.

Requires a C++ toolchain, CMake, pybind11 and a populated JUCE submodule.
PEDALBOARD_JUCE_DIR may point at another checkout of the pinned JUCE revision.
"""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


@pytest.fixture(scope="session")
def audio_stream_harness(tmp_path_factory):
    if sys.platform != "darwin":
        pytest.skip("Native AudioStream harness currently validated on macOS")
    root = Path(__file__).resolve().parents[1]
    juce = Path(os.environ.get("PEDALBOARD_JUCE_DIR", root / "JUCE"))
    if not (juce / "modules/juce_core/juce_core.cpp").exists():
        pytest.skip("Native AudioStream tests require the JUCE submodule")
    if shutil.which("cmake") is None or importlib.util.find_spec("pybind11") is None:
        pytest.skip("Native AudioStream tests require cmake and pybind11")
    build = tmp_path_factory.mktemp("audio-stream-native")
    configure = subprocess.run(
        [
            "cmake",
            "-S",
            str(root / "tests/native"),
            "-B",
            str(build),
            f"-DJUCE_DIR={juce}",
            f"-DPython_EXECUTABLE={sys.executable}",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        capture_output=True,
        text=True,
    )
    assert configure.returncode == 0, configure.stdout + configure.stderr
    compile_result = subprocess.run(
        ["cmake", "--build", str(build), "--parallel", "4"],
        capture_output=True,
        text=True,
    )
    assert compile_result.returncode == 0, compile_result.stdout + compile_result.stderr
    return build / "audio_stream_harness"


@pytest.mark.parametrize(
    "scenario",
    [
        "first_backend",
        "mono",
        "zero_input",
        "null_input",
        "null_output",
        "record_null_input",
        "play_null_output",
        "second_backend",
        "plugin_null_output",
        "plugin_zero_input",
        "record_fifo_wrap",
        "record_zero_inputs",
        "record_fewer_inputs",
        "play_fifo_wrap",
        "second_backend_input_only",
        "second_backend_output_only",
        "first_backend_name_variant",
        "second_backend_name_variant",
        "first_backend_empty_input",
        "first_backend_empty_output",
        "second_backend_empty_input",
        "second_backend_empty_output",
    ],
)
def test_stream_handles_device_scenarios(audio_stream_harness, scenario):
    result = subprocess.run(
        [str(audio_stream_harness), scenario],
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert result.returncode == 0, (
        f"AudioStream scenario {scenario!r} exited with {result.returncode}:\n"
        f"{result.stdout}{result.stderr}"
    )
