from pedalboard_native import *  # noqa: F403, F401
from .pedalboard import Pedalboard, AVAILABLE_PLUGIN_CLASSES, load_plugin  # noqa: F401

for klass in AVAILABLE_PLUGIN_CLASSES:
    vars()[klass.__name__] = klass
