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
pedalboard._pedalboard._load_plugin must try only the plugin classes it is given, in order.
"""

from typing import Any

import pytest

from pedalboard._pedalboard import _load_plugin

KWARGS: dict[str, Any] = dict(
    path_to_plugin_file="/nonexistent/plugin.vst3",
    parameter_values={},
    plugin_name=None,
    initialization_timeout=1.0,
)


def _fake_class(name, calls, result=None, error=None) -> Any:
    def construct(*, path_to_plugin_file, parameter_values, plugin_name, initialization_timeout):
        calls.append(name)
        if error is not None:
            raise error
        return result

    construct.__name__ = construct.__qualname__ = name
    return construct


def test_falls_through_import_error_to_next_class_in_order():
    calls = []
    sentinel = object()
    classes = [
        _fake_class("FirstFake", calls, error=ImportError("nope")),
        _fake_class("SecondFake", calls, result=sentinel),
        _fake_class("ThirdFake", calls, result=object()),
    ]
    assert _load_plugin(classes, **KWARGS) is sentinel
    assert calls == ["FirstFake", "SecondFake"]


def test_all_import_errors_raise_import_error_naming_only_given_classes():
    calls = []
    classes = [
        _fake_class("AlphaFake", calls, error=ImportError("alpha failed")),
        _fake_class("BetaFake", calls, error=ImportError("beta failed")),
    ]
    with pytest.raises(ImportError) as exc:
        _load_plugin(classes, **KWARGS)
    assert calls == ["AlphaFake", "BetaFake"]
    message = str(exc.value)
    assert "AlphaFake" in message
    assert "BetaFake" in message
    assert "VST3Plugin" not in message
    assert "AudioUnitPlugin" not in message


def test_non_import_error_propagates_immediately():
    calls = []
    classes = [
        _fake_class("BrokenFake", calls, error=ValueError("boom")),
        _fake_class("NeverFake", calls, result=object()),
    ]
    with pytest.raises(ValueError, match="boom"):
        _load_plugin(classes, **KWARGS)
    assert calls == ["BrokenFake"]


def test_empty_class_list_raises_import_error():
    with pytest.raises(ImportError):
        _load_plugin([], **KWARGS)
