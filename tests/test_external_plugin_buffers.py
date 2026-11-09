"""Run the native ExternalPlugin buffer contract tests in isolated processes."""

import os
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.fixture(scope="module")
def native_buffer_test_binary():
    configured = os.environ.get("PEDALBOARD_NATIVE_TEST_BINARY")
    if not configured:
        pytest.skip("Set PEDALBOARD_NATIVE_TEST_BINARY; see tests/native/README.md")
    executable = Path(configured).expanduser().resolve()
    assert executable.is_file(), f"Native test binary does not exist: {executable}"
    return executable


@pytest.mark.parametrize(
    "case",
    [
        "single_bus_warm_up",
        "multi_bus_warm_up",
        "single_bus_render_full",
        "single_bus_render_partial",
        "multi_bus_render_full",
        "multi_bus_render_partial",
    ],
)
def test_external_plugin_buffers(native_buffer_test_binary, case):
    environment = os.environ.copy()
    python_paths = [str(Path(path or os.getcwd()).resolve()) for path in sys.path]
    if environment.get("PYTHONPATH"):
        python_paths.append(environment["PYTHONPATH"])
    environment["PYTHONPATH"] = os.pathsep.join(python_paths)
    result = subprocess.run(
        [str(native_buffer_test_binary), case],
        env=environment,
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 0, (
        f"Native case {case} exited with {result.returncode}\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    assert f"PASS {case}" in result.stdout
