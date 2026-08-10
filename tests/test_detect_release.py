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

import importlib.util
import pathlib

import pytest

# Loaded by path rather than imported as a package: these tests also run from a temporary
# directory during post-wheel-build testing, where the repo root isn't on sys.path.
_SCRIPT = pathlib.Path(__file__).resolve().parent.parent / "scripts" / "detect_release.py"
_spec = importlib.util.spec_from_file_location("detect_release", _SCRIPT)
detect_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(detect_release)

VERSION_FILE = pathlib.Path(__file__).resolve().parent.parent / "pedalboard" / "version.py"


@pytest.mark.parametrize(
    "source,expected",
    [
        ('__version__ = "0.9.24"', "0.9.24"),
        ('# a comment\n__version__ = "1.2.3"\nMAJOR = 1\n', "1.2.3"),
        ('__version__ = "1.2.3rc1"', "1.2.3rc1"),
        # Must be a top-level assignment on its own line, not e.g. an attribute.
        ('foo.__version__ = "1.2.3"', None),
        ("__version__ = '1.2.3'", None),
        ("", None),
    ],
)
def test_parse_version(source, expected):
    assert detect_release.parse_version(source) == expected


def test_parse_version_matches_the_real_version_file():
    """The regex must keep working against the file it actually reads."""
    assert detect_release.parse_version(VERSION_FILE.read_text()) is not None


@pytest.mark.parametrize(
    "version,valid",
    [
        ("0.9.24", True),
        ("0.9.25", True),
        ("1.0.0", True),
        ("0.9.25rc1", True),
        ("0.10.0.post1", True),
        ("0.9", False),
        ("1.2.3-dirty", False),
        ("not-a-version", False),
        ("", False),
        # Must match the whole string, not just part of it:
        ("v1.2.3", False),
        ("1.2.3 && rm -rf /", False),
    ],
)
def test_is_valid_version(version, valid):
    assert detect_release.is_valid_version(version) is valid


def test_require_version_rejects_missing_and_invalid():
    with pytest.raises(detect_release.InvalidVersionError, match="Could not find"):
        detect_release.require_version("nothing here")
    with pytest.raises(detect_release.InvalidVersionError, match="not a valid version"):
        detect_release.require_version('__version__ = "banana"')
    assert detect_release.require_version('__version__ = "1.2.3"') == "1.2.3"


def test_bumping_the_version_on_master_releases():
    decision = detect_release.decide(
        current="0.9.25", previous="0.9.24", event_name="push", tag_exists=False
    )
    assert decision.should_release is True
    assert decision.version_changed is True
    assert "0.9.24 -> 0.9.25" in decision.reason


def test_unchanged_version_does_not_release():
    decision = detect_release.decide(
        current="0.9.24", previous="0.9.24", event_name="push", tag_exists=False
    )
    assert decision.should_release is False
    assert decision.version_changed is False


def test_pull_request_never_releases_but_reports_the_change():
    decision = detect_release.decide(
        current="0.9.25", previous="0.9.24", event_name="pull_request", tag_exists=False
    )
    assert decision.should_release is False
    # Still flagged, so the PR can warn that merging will publish.
    assert decision.version_changed is True


def test_existing_tag_does_not_release_again():
    decision = detect_release.decide(
        current="0.9.25", previous="0.9.24", event_name="push", tag_exists=True
    )
    assert decision.should_release is False
    assert "already exists" in decision.reason


def test_a_brand_new_version_file_counts_as_a_change():
    decision = detect_release.decide(
        current="0.9.25", previous=None, event_name="push", tag_exists=False
    )
    assert decision.should_release is True
    assert "<none> -> 0.9.25" in decision.reason


def test_a_downgrade_still_releases():
    """Deliberate: reverting a bad bump should be publishable without special-casing."""
    decision = detect_release.decide(
        current="0.9.23", previous="0.9.24", event_name="push", tag_exists=False
    )
    assert decision.should_release is True


@pytest.mark.parametrize("event_name", ["pull_request", "release", "schedule", ""])
def test_only_pushes_can_release(event_name):
    decision = detect_release.decide(
        current="0.9.25", previous="0.9.24", event_name=event_name, tag_exists=False
    )
    assert decision.should_release is False


# version.py is not just the version: it carries a copyright header whose year range gets
# bumped annually. Editing that must not look like a release to anything downstream.
LICENSE_HEADER = (
    "#! /usr/bin/env python\n#\n# Copyright 2021-{year} Spotify AB\n#\n"
    "# Licensed under the GNU Public License, Version 3.0 (the 'License');\n"
    "# limitations under the License.\n\n"
)


def _version_file(version: str, year: int) -> str:
    return (
        LICENSE_HEADER.format(year=year)
        + f'__version__ = "{version}"\n'
        + "MAJOR, MINOR, PATCH = (int(x) for x in __version__.split('.'))\n"
    )


def test_editing_only_the_license_header_is_not_a_version_change():
    before = _version_file("0.9.24", 2026)
    after = _version_file("0.9.24", 2027)
    assert before != after, "the fixture should differ, otherwise this proves nothing"

    decision = detect_release.decide(
        current=detect_release.parse_version(after),
        previous=detect_release.parse_version(before),
        event_name="push",
        tag_exists=False,
    )
    assert decision.version_changed is False
    assert decision.should_release is False


def test_bumping_the_version_and_the_year_together_is_a_version_change():
    decision = detect_release.decide(
        current=detect_release.parse_version(_version_file("0.9.25", 2027)),
        previous=detect_release.parse_version(_version_file("0.9.24", 2026)),
        event_name="push",
        tag_exists=False,
    )
    assert decision.version_changed is True
    assert decision.should_release is True


def test_version_changed_is_true_even_when_release_is_blocked():
    """
    version_changed drives the "merging this publishes" warning on pull requests, so it
    has to stay true in the cases where the version moved but we won't publish.
    """
    for event_name, tag_exists in [("pull_request", False), ("push", True)]:
        decision = detect_release.decide(
            current="0.9.25", previous="0.9.24", event_name=event_name, tag_exists=tag_exists
        )
        assert decision.version_changed is True
        assert decision.should_release is False
