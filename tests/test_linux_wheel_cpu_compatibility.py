import os
from pathlib import Path
import subprocess
import sys

import pytest


pytestmark = pytest.mark.skipif(
    sys.platform == "win32", reason="The wheel compatibility harness runs only on Linux"
)


HARNESS = Path(__file__).parents[1] / "scripts" / "test_linux_wheel_cpu_compatibility.py"
NON_AVX_CPU = "Nehalem"


def run_harness(
    tmp_path: Path,
    emulator_body: str,
    cpu_models: tuple[str, ...] = (NON_AVX_CPU,),
) -> tuple[subprocess.CompletedProcess, list[str]]:
    invocation_log = tmp_path / "qemu-invocations"
    emulator = tmp_path / "qemu-x86_64"
    emulator.write_text(
        "#!/bin/sh\n"
        'cpu_model="$2"\n'
        'for argument do code="$argument"; done\n'
        f'printf "%s|%s\\n" "$cpu_model" "$code" >> "{invocation_log}"\n'
        f"{emulator_body}\n"
    )
    emulator.chmod(0o755)

    env = os.environ.copy()
    env["PATH"] = f"{tmp_path}{os.pathsep}{env['PATH']}"
    result = subprocess.run(
        [sys.executable, HARNESS, *cpu_models],
        capture_output=True,
        env=env,
        text=True,
    )
    invocations = invocation_log.read_text().splitlines()
    return result, invocations


def test_non_avx_cpu_requires_wheel_import_to_terminate_with_sigill(tmp_path: Path) -> None:
    result, invocations = run_harness(
        tmp_path,
        'case "$code" in *pedalboard*) kill -ILL $$ ;; *) exit 0 ;; esac',
    )

    assert result.returncode == 0, result.stderr
    assert len(invocations) == 2
    assert "pedalboard" not in invocations[0]
    assert "pedalboard" in invocations[1]


def test_default_cpu_models_include_the_non_avx_boundary_check(tmp_path: Path) -> None:
    result, invocations = run_harness(
        tmp_path,
        'case "$cpu_model:$code" in Nehalem:*pedalboard*) kill -ILL $$ ;; *) exit 0 ;; esac',
        cpu_models=(),
    )

    assert result.returncode == 0, result.stderr
    assert any(invocation.startswith("IvyBridge|") for invocation in invocations)
    assert any(invocation.startswith("EPYC-Milan|") for invocation in invocations)
    non_avx_invocations = [
        invocation for invocation in invocations if invocation.startswith(f"{NON_AVX_CPU}|")
    ]
    assert len(non_avx_invocations) == 2
    assert "pedalboard" not in non_avx_invocations[0]
    assert "pedalboard" in non_avx_invocations[1]


def test_non_avx_cpu_rejects_a_broken_emulator_or_python_runtime(tmp_path: Path) -> None:
    result, invocations = run_harness(tmp_path, "kill -ILL $$")

    assert result.returncode != 0
    assert len(invocations) == 1
    assert "pedalboard" not in invocations[0]


def test_non_avx_cpu_rejects_a_non_sigill_wheel_failure(tmp_path: Path) -> None:
    result, invocations = run_harness(
        tmp_path,
        'case "$code" in *pedalboard*) exit 1 ;; *) exit 0 ;; esac',
    )

    assert result.returncode != 0
    assert len(invocations) == 2
    assert "pedalboard" not in invocations[0]
    assert "pedalboard" in invocations[1]
