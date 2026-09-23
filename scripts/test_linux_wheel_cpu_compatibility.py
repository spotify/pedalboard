#!/usr/bin/env python3
"""Smoke-test an installed Linux wheel across its x86_64 CPU compatibility boundary."""

import argparse
import signal
import subprocess
import sys
import tempfile


SUPPORTED_CPU_MODELS = ("IvyBridge", "EPYC-Milan")
UNSUPPORTED_CPU_MODELS = ("Nehalem",)
DEFAULT_CPU_MODELS = SUPPORTED_CPU_MODELS + UNSUPPORTED_CPU_MODELS
SMOKE_TEST_TIMEOUT_SECONDS = 60
RUNTIME_PROBE = "print('Python runtime started successfully')"
WHEEL_SMOKE_TEST = (
    "import numpy as np; "
    "from pedalboard import Gain; "
    "output = Gain(gain_db=-6)(np.zeros((1, 1024), dtype=np.float32), 48000); "
    "assert output.shape == (1, 1024)"
)


def run_on_cpu(
    cpu_model: str, python_code: str, python: str = sys.executable
) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory() as working_directory:
        return subprocess.run(
            [
                "qemu-x86_64",
                "-cpu",
                cpu_model,
                python,
                "-c",
                python_code,
            ],
            capture_output=True,
            cwd=working_directory,
            text=True,
            timeout=SMOKE_TEST_TIMEOUT_SECONDS,
        )


def format_failure(result: subprocess.CompletedProcess[str]) -> str:
    return f"exit code {result.returncode}\n{result.stdout}{result.stderr}"


def smoke_test_wheel(cpu_model: str, python: str = sys.executable) -> None:
    print(f"Testing installed wheel on {cpu_model}...")
    if cpu_model in UNSUPPORTED_CPU_MODELS:
        runtime_probe = run_on_cpu(cpu_model, RUNTIME_PROBE, python)
        if runtime_probe.returncode != 0:
            raise RuntimeError(
                f"Python runtime failed to start on {cpu_model} ({format_failure(runtime_probe)})"
            )

    result = run_on_cpu(cpu_model, WHEEL_SMOKE_TEST, python)
    if cpu_model in UNSUPPORTED_CPU_MODELS:
        expected_returncode = -signal.SIGILL
        if result.returncode != expected_returncode:
            raise RuntimeError(
                f"pedalboard wheel unexpectedly ran on non-AVX CPU {cpu_model}; "
                f"expected SIGILL ({expected_returncode}), got {format_failure(result)}"
            )
        print(f"Installed wheel correctly requires AVX on {cpu_model}.")
        return

    if result.returncode != 0:
        raise RuntimeError(
            f"pedalboard wheel failed its smoke test on {cpu_model} ({format_failure(result)})"
        )
    print(f"Installed wheel passed on {cpu_model}.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("cpu_models", nargs="*", default=DEFAULT_CPU_MODELS)
    args = parser.parse_args()

    for cpu_model in args.cpu_models:
        smoke_test_wheel(cpu_model)


if __name__ == "__main__":
    main()
