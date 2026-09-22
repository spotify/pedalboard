#!/usr/bin/env python3
"""Smoke-test an installed Linux wheel on supported x86_64 CPUs."""

import argparse
import subprocess
import sys
import tempfile


SUPPORTED_CPU_MODELS = ("IvyBridge", "EPYC-Milan")
SMOKE_TEST_TIMEOUT_SECONDS = 60


def smoke_test_wheel(cpu_model: str, python: str = sys.executable) -> None:
    print(f"Testing installed wheel on {cpu_model}...")
    with tempfile.TemporaryDirectory() as working_directory:
        result = subprocess.run(
            [
                "qemu-x86_64",
                "-cpu",
                cpu_model,
                python,
                "-c",
                (
                    "import numpy as np; "
                    "from pedalboard import Gain, PitchShift; "
                    "output = Gain(gain_db=-6)(np.zeros((1, 1024), dtype=np.float32), 48000); "
                    "assert output.shape == (1, 1024); "
                    # Exercise the Rubber Band/FFTW path with nonzero audio.
                    "audio = np.sin(np.arange(8192) * (2 * np.pi * 440 / 48000)); "
                    "audio = audio.astype(np.float32).reshape(1, -1); "
                    "output = PitchShift(semitones=3)(audio, 48000); "
                    "assert output.shape == audio.shape; "
                    "assert np.isfinite(output).all(); "
                    "assert np.any(output)"
                ),
            ],
            capture_output=True,
            cwd=working_directory,
            text=True,
            timeout=SMOKE_TEST_TIMEOUT_SECONDS,
        )
    if result.returncode != 0:
        raise RuntimeError(
            f"pedalboard wheel failed its smoke test on {cpu_model} "
            f"(exit code {result.returncode})\n{result.stdout}{result.stderr}"
        )
    print(f"Installed wheel passed on {cpu_model}.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("cpu_models", nargs="*", default=SUPPORTED_CPU_MODELS)
    args = parser.parse_args()

    for cpu_model in args.cpu_models:
        smoke_test_wheel(cpu_model)


if __name__ == "__main__":
    main()
