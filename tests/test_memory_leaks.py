#! /usr/bin/env python
#
# Copyright 2021 Spotify AB
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


import gc
import weakref
import pytest

from .test_external_plugins import load_test_plugin, AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT


def get_first_parameter_from(plugin):
    param_name, param = next(
        iter([(name, param) for name, param in plugin.parameters.items() if param.type is float])
    )
    value = getattr(plugin, param_name)
    return param_name, param, value


@pytest.mark.parametrize("plugin_path", AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_plugin_can_be_garbage_collected(plugin_path: str):
    # Load a VST3 or Audio Unit plugin from disk:
    plugin = load_test_plugin(plugin_path, disable_caching=True)

    _plugin_ref = weakref.ref(plugin)

    param_name, param, value = get_first_parameter_from(plugin)
    initial_value = float(value)

    # After the plugin is no longer in scope, it should be garbage collectable
    # However, we still have a parameter referring to this, so it won't be removed yet:
    del plugin
    gc.collect()
    assert _plugin_ref(), (
        f"Expected plugin to not have been garbage collected, as a parameter ({param_name},"
        f" {param}) still exists."
    )
    assert set(dir(initial_value)).issubset(set(dir(value)))
    assert value == initial_value  # value should not change

    # Once we delete that parameter, the plugin should be releasable
    del param
    gc.collect()

    # Do this dance because calling gc.get_referrers with None would return a huge amount:
    referrers = None
    if _plugin_ref() is not None:
        referrers = gc.get_referrers(_plugin_ref())

    assert _plugin_ref() is None, f"Expected plugin to have no referrers, but found: {referrers}"

    assert set(dir(initial_value)).issubset(set(dir(value)))
    assert value == initial_value  # value should still not change
