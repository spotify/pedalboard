import os
import gc
import weakref
import pytest
import platform

import pedalboard
from .test_external_plugins import TEST_PLUGIN_BASE_PATH, AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT


@pytest.mark.parametrize('plugin_path', AVAILABLE_PLUGINS_IN_TEST_ENVIRONMENT)
def test_plugin_can_be_garbage_collected(plugin_path: str):
    # Load a VST3 plugin from disk:
    plugin = pedalboard.load_plugin(
        os.path.join(TEST_PLUGIN_BASE_PATH, platform.system(), plugin_path)
    )

    _plugin_ref = weakref.ref(plugin)
    param_name, param = next(
        iter([(name, param) for name, param in plugin.parameters.items() if param.type is float])
    )
    value = getattr(plugin, param_name)

    initial_value = float(value)

    # After the plugin is no longer in scope, it should be garbage collectable
    # However, we still have a parameter referring to this, so it won't be removed yet:
    del plugin
    gc.collect()
    assert _plugin_ref()
    assert set(dir(initial_value)).issubset(set(dir(value)))
    assert value == initial_value  # value should not change

    # Once we delete that parameter, the plugin should be releasable
    del param
    gc.collect()
    assert not _plugin_ref()
    assert set(dir(initial_value)).issubset(set(dir(value)))
    assert value == initial_value  # value should still not change
