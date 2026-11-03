"""Check framework APIs from the perspective of a typed Python consumer."""

import json
from pathlib import Path
import subprocess
import sys
import textwrap

import pytest

from .utils import import_framework


@pytest.fixture(params=["torch", "jax", "tensorflow"])
def framework(request):
    import_framework("pyright")
    import_framework(request.param)
    return request.param


def check_consumer(tmp_path, source):
    consumer = tmp_path / "consumer.py"
    consumer.write_text(textwrap.dedent(source))
    config = tmp_path / "pyrightconfig.json"
    config.write_text(
        json.dumps(
            {
                "include": [str(consumer)],
                "extraPaths": [str(Path(__file__).resolve().parent.parent)],
                "typeCheckingMode": "basic",
            }
        )
    )
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "pyright",
            "--project",
            str(config),
            "--pythonpath",
            sys.executable,
            "--outputjson",
        ],
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert result.returncode in (0, 1), result.stdout + result.stderr
    try:
        report = json.loads(result.stdout)
    except json.JSONDecodeError:
        pytest.fail(result.stdout + result.stderr)
    return report


def test_documented_framework_exports_have_usable_array_types(tmp_path, framework):
    array_import = {
        "torch": "from torch import Tensor as FrameworkArray",
        "jax": "from jax import Array as FrameworkArray",
        "tensorflow": "from tensorflow import Tensor as FrameworkArray",
    }[framework]
    report = check_consumer(
        tmp_path,
        f"""
        import numpy as np
        from typing_extensions import assert_type
        {array_import}
        from pedalboard.{framework} import (
            Gain, Reverb, Pedalboard, AudioFile, process, time_stretch, load_plugin,
        )

        def use_audio(audio: FrameworkArray) -> None:
            gain = Gain(gain_db=-6)
            reverb = Reverb(room_size=0.5)
            board = Pedalboard([gain, reverb])
            assert_type(gain(audio, 44100), FrameworkArray)
            assert_type(reverb.process(audio, 44100), FrameworkArray)
            assert_type(board(audio, 44100), FrameworkArray)
            assert_type(board.process(audio, 44100), FrameworkArray)
            assert_type(process(audio, 44100, [gain]), FrameworkArray)
            assert_type(time_stretch(audio, 44100, 1.5), FrameworkArray)
            numpy_audio = np.zeros((2, 128), dtype=np.float32)
            assert_type(gain(numpy_audio, 44100), FrameworkArray)
            with AudioFile("input.wav") as source:
                assert_type(source, AudioFile)
                assert_type(source.resampled_to(22050), AudioFile)
                assert_type(source.read(128), FrameworkArray)
                assert_type(source.read_raw(128), FrameworkArray)
                with source.resampled_to(22050) as resampled:
                    assert_type(resampled.read(128), FrameworkArray)
            with AudioFile("output.wav", "w", 44100, 2) as destination:
                destination.write(audio)
                destination.write(numpy_audio)
            assert_type(AudioFile.encode(audio, 44100, "wav", 2), bytes)
            loaded = load_plugin("effect.vst3")
            assert_type(loaded(audio, 44100), FrameworkArray)
        """,
    )
    assert report["summary"]["errorCount"] == 0, json.dumps(report, indent=2)


def test_misspelled_framework_export_is_rejected(tmp_path, framework):
    report = check_consumer(tmp_path, f"from pedalboard.{framework} import Gaain\n")
    assert any(
        diagnostic["severity"] == "error" and "Gaain" in diagnostic["message"]
        for diagnostic in report["generalDiagnostics"]
    ), json.dumps(report, indent=2)
