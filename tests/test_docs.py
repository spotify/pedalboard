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

"""
Checks that the Sphinx docs include README.md slices via stable markers rather
than fixed line numbers, so README edits can't silently shift the docs.
"""

import re
from pathlib import Path

import pytest

from .utils import import_framework

REPO_ROOT = Path(__file__).resolve().parent.parent
README = REPO_ROOT / "README.md"
DOCS_SOURCE = REPO_ROOT / "docs" / "source"
ML_HEADING = "### Using Pedalboard with PyTorch, JAX or TensorFlow"


def _find_readme_includes():
    """Return (rst_name, {option: value}) for each mdinclude of README.md."""
    includes = []
    for rst in sorted(DOCS_SOURCE.glob("*.rst")):
        lines = rst.read_text().splitlines()
        for i, line in enumerate(lines):
            m = re.match(r"\s*\.\.\s+mdinclude::\s*(\S+)", line)
            if not m or Path(m.group(1)).name != "README.md":
                continue
            options = {}
            for opt_line in lines[i + 1 :]:
                om = re.match(r"\s+:([\w-]+):\s*(.*)$", opt_line)
                if not om:
                    break
                options[om.group(1)] = om.group(2).strip()
            includes.append((rst.name, options))
    return includes


INCLUDES = _find_readme_includes()
INCLUDE_IDS = [f"{name}#{i}" for i, (name, _) in enumerate(INCLUDES)]


def _slice(readme: str, options: dict) -> str:
    start, end = 0, len(readme)
    if "start-after" in options:
        start = readme.index(options["start-after"]) + len(options["start-after"])
    if "end-before" in options:
        end = readme.index(options["end-before"], start)
    return readme[start:end]


def test_docs_include_readme():
    assert INCLUDES, "expected at least one mdinclude of README.md in docs/source/*.rst"


@pytest.mark.parametrize("rst_name, options", INCLUDES, ids=INCLUDE_IDS)
def test_include_uses_no_fixed_line_numbers(rst_name, options):
    assert "start-line" not in options, f"{rst_name} uses :start-line:"
    assert "end-line" not in options, f"{rst_name} uses :end-line:"


@pytest.mark.parametrize("rst_name, options", INCLUDES, ids=INCLUDE_IDS)
def test_include_markers_exist_once_and_in_order(rst_name, options):
    readme = README.read_text()
    for key in ("start-after", "end-before"):
        if key in options:
            marker = options[key]
            assert marker, f"{rst_name} has an empty :{key}:"
            count = readme.count(marker)
            assert count == 1, f"{rst_name} :{key}: {marker!r} appears {count} times in README"
    if "start-after" in options and "end-before" in options:
        assert readme.index(options["start-after"]) < readme.index(options["end-before"])


def test_ml_frameworks_section_is_included_in_docs():
    readme = README.read_text()
    assert ML_HEADING in readme
    for _, options in INCLUDES:
        if any(k in options for k in ("start-line", "end-line")):
            continue  # line-number slices are rejected above
        try:
            if ML_HEADING in _slice(readme, options):
                return
        except ValueError:
            continue
    pytest.fail(f"{ML_HEADING!r} is not inside any marker-based README slice in the docs")


def test_rendered_docs_include_expected_readme_sections(tmp_path):
    """Build the real docs through the custom mdinclude directive and check the output."""
    import_framework("sphinx")
    import_framework("m2r2")
    from sphinx.cmd.build import build_main

    # The text builder renders page bodies only (no navigation sidebar listing every heading).
    out_dir = tmp_path / "text"
    status = build_main(
        ["-b", "text", "-q", "-d", str(tmp_path / "doctrees"), str(DOCS_SOURCE), str(out_dir)]
    )
    assert status == 0, "sphinx build failed"

    def rendered(name):
        return (out_dir / name).read_text(encoding="utf-8")

    assert "Using Pedalboard with PyTorch, JAX or TensorFlow" in rendered("examples.txt")
    index = rendered("index.txt")
    assert "Installation" in index
    # index.rst's toctree lists section titles, so check Quick start *body* content instead.
    assert "Make a Pedalboard object, containing multiple audio plugins" not in index
    citation = rendered("citation.txt")
    assert "Citing" in citation
    assert "zenodo.7817838" in citation
    assert "Copyright" in rendered("license.txt")
