#! /usr/bin/env python
#
# Copyright 2021-2026 Spotify AB
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
Decide whether a CI run should cut a release.

``pedalboard/version.py`` is the single source of truth for the version: bumping
``__version__`` there and merging to master is what publishes a release. This script is
what notices that bump, and is called by the ``detect-release`` job in
``.github/workflows/all.yml``.

On a pull request it doesn't release anything, but it does warn loudly when the version
changed, so nobody merges a release without meaning to.

The decision logic lives in pure functions so it can be tested without a git repository
or a CI environment; see ``tests/test_detect_release.py``.
"""

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Optional

# Matches the single line in pedalboard/version.py that defines the version. Kept in sync
# with the regex in pyproject.toml's [tool.scikit-build.metadata.version], which is how
# the build backend reads the same file.
VERSION_PATTERN = re.compile(r'^__version__ = "(?P<value>.+)"$', re.MULTILINE)

# Deliberately stricter than PEP 440: releases here are always MAJOR.MINOR.PATCH, with an
# optional suffix for the occasional pre-release or post-release (e.g. "1.2.3rc1").
VALID_VERSION_PATTERN = re.compile(r"\d+\.\d+\.\d+[a-z0-9.]*")


class InvalidVersionError(ValueError):
    """Raised when version.py is missing a version, or its version is unusable."""


@dataclass(frozen=True)
class Decision:
    """The outcome of deciding whether to release, and why."""

    should_release: bool
    version_changed: bool
    reason: str


def parse_version(source: str) -> Optional[str]:
    """Pull ``__version__`` out of the contents of a version.py, or None if absent."""
    match = VERSION_PATTERN.search(source)
    return match.group("value") if match else None


def is_valid_version(version: str) -> bool:
    """Return whether a version string is one we're willing to publish."""
    return VALID_VERSION_PATTERN.fullmatch(version) is not None


def require_version(source: str, where: str = "pedalboard/version.py") -> str:
    """Parse and validate a version, raising InvalidVersionError if unusable."""
    version = parse_version(source)
    if version is None:
        raise InvalidVersionError(f"Could not find __version__ in {where}.")
    if not is_valid_version(version):
        raise InvalidVersionError(f"{version!r} in {where} is not a valid version.")
    return version


def decide(
    current: str,
    previous: Optional[str],
    event_name: str,
    tag_exists: bool,
) -> Decision:
    """
    Decide whether this run should release, given the version now, the version before it,
    the CI event that triggered the run, and whether the tag already exists.

    ``previous`` is None when version.py didn't exist in the commit being compared
    against, which counts as a change (that's a brand new package).
    """
    changed = current != previous

    if not changed:
        return Decision(False, False, f"Version unchanged ({current}); not releasing.")

    if event_name != "push":
        # Pull requests report the change so it's visible, but never publish.
        return Decision(
            False,
            True,
            f"Version would change {previous or '<none>'} -> {current} once merged,"
            f" but this is a {event_name}, not a push to master.",
        )

    if tag_exists:
        return Decision(
            False,
            True,
            f"Tag v{current} already exists; not releasing again.",
        )

    return Decision(
        True,
        True,
        f"Version bumped {previous or '<none>'} -> {current}; this run will release.",
    )


def git_show(revision: str, path: str) -> Optional[str]:
    """Return a file's contents at a given revision, or None if it isn't there."""
    try:
        return subprocess.run(
            ["git", "show", f"{revision}:{path}"],
            capture_output=True,
            check=True,
            text=True,
        ).stdout
    except subprocess.CalledProcessError:
        return None


def tag_exists(version: str, remote: str = "origin") -> bool:
    """Return whether the release tag for a version already exists on the remote."""
    return (
        subprocess.run(
            ["git", "ls-remote", "--exit-code", "--tags", remote, f"refs/tags/v{version}"],
            capture_output=True,
        ).returncode
        == 0
    )


def write_github_output(**outputs: str) -> None:
    """Write step outputs for later jobs to read via needs.<job>.outputs.<name>."""
    path = os.environ.get("GITHUB_OUTPUT")
    if not path:
        return
    with open(path, "a") as f:
        for key, value in outputs.items():
            f.write(f"{key}={value}\n")


def write_step_summary(markdown: str) -> None:
    """Add a note to the run's summary page, so it's visible without reading logs."""
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not path:
        return
    with open(path, "a") as f:
        f.write(markdown + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--version-file",
        default="pedalboard/version.py",
        help="Path to the file holding __version__.",
    )
    parser.add_argument(
        "--compare-against",
        default="HEAD~1",
        help=(
            "Revision to compare the version against. On a push this is the previous"
            " commit; on a pull request it's the base branch, as HEAD is a merge commit."
        ),
    )
    parser.add_argument(
        "--event-name",
        default=os.environ.get("GITHUB_EVENT_NAME", ""),
        help="The CI event that triggered this run. Only 'push' can release.",
    )
    args = parser.parse_args()

    with open(args.version_file) as f:
        try:
            current = require_version(f.read(), args.version_file)
        except InvalidVersionError as e:
            print(f"::error file={args.version_file}::{e}")
            return 1

    previous_source = git_show(args.compare_against, args.version_file)
    previous = parse_version(previous_source) if previous_source is not None else None

    print(f"Version in {args.compare_against}: {previous or '<none>'}")
    print(f"Version in this commit:  {current}")

    decision = decide(
        current=current,
        previous=previous,
        event_name=args.event_name,
        # Only worth a network round-trip if we'd otherwise release.
        tag_exists=(
            tag_exists(current) if current != previous and args.event_name == "push" else False
        ),
    )
    print(decision.reason)

    if decision.version_changed and not decision.should_release and args.event_name != "push":
        # The whole point of the warning: make it obvious, on the PR itself, that merging
        # publishes to PyPI irreversibly.
        print(f"::warning file={args.version_file}::Merging this will publish v{current} to PyPI.")
        write_step_summary(
            f"## :rocket: This pull request releases v{current}\n\n"
            f"Merging it bumps `{previous or '<none>'}` to `{current}`, which publishes"
            f" **v{current}** to PyPI and creates the `v{current}` GitHub release.\n\n"
            "PyPI releases cannot be replaced, only yanked. If that isn't intended,"
            " drop the change to `pedalboard/version.py` before merging."
        )

    write_github_output(
        **{
            "should-release": "true" if decision.should_release else "false",
            # Distinct from should-release: the version can change without this run being
            # allowed to publish (a pull request, or a tag that already exists). The docs
            # workflow keys off this, so that editing the license header in version.py
            # doesn't republish docs for an unreleased master.
            "version-changed": "true" if decision.version_changed else "false",
            "version": current,
        }
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
