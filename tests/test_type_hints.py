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


from psutil import Process
import os
import sys
from glob import glob
import pytest
import subprocess
from subprocess import STDOUT
from typing import Dict

FIXTURE_DIR = os.path.join(os.path.dirname(__file__), "mypy_fixtures")
ALL_FIXTURES = set(glob(f"{FIXTURE_DIR}/*.py"))
FAILING_FIXTURES = set([f for f in ALL_FIXTURES if "fail" in f])
PASSING_FIXTURES = ALL_FIXTURES - FAILING_FIXTURES


def get_env_with_pythonpath() -> Dict[str, str]:
    """
    When launching Python in a subprocess, we need to pass along the current interpreter's PATH,
    which may not be sent via the environment variables by default (i.e.: in Tox)
    """
    env = os.environ.copy()
    env["PYTHONPATH"] = ":".join(sys.path)
    return env


@pytest.mark.skipif(
    os.environ.get("CIBW_BUILD") is not None,
    reason="Unable to get MyPy tests working while in cibuildwheel",
)
@pytest.mark.parametrize("filename", PASSING_FIXTURES)
def test_mypy_passes(filename):
    # Run this test in a subprocess, as MyPy forcibly exits, killing PyTest:
    try:
        subprocess.check_output(
            [Process(os.getpid()).exe(), "-m", "mypy", filename],
            env=get_env_with_pythonpath(),
            stderr=STDOUT,
        )
    except subprocess.CalledProcessError as e:
        raise AssertionError(
            f"Expected MyPy to pass, but failed with:\n\n{e.stdout.decode('utf-8')}"
        ) from e


@pytest.mark.skipif(
    os.environ.get("CIBW_BUILD") is not None,
    reason="Unable to get MyPy tests working while in cibuildwheel",
)
@pytest.mark.parametrize("filename", FAILING_FIXTURES)
def test_mypy_fails(filename):
    # Run this test in a subprocess, as MyPy forcibly exits, killing PyTest:
    with pytest.raises(subprocess.CalledProcessError):
        subprocess.check_output(
            [Process(os.getpid()).exe(), "-m", "mypy", filename],
            env=get_env_with_pythonpath(),
            stderr=STDOUT,
        )
