# This file is not intended to be executed - it's intended to be used to ensure type checker
# compatibility. Type checkers should reject this file: `parameters` is read-only at runtime.

from pedalboard import load_plugin

plugin = load_plugin("some_plugin.vst3")
plugin.parameters["gain"] = plugin.parameters["gain"]
