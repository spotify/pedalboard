Plugin Compatibility
====================

``pedalboard`` allows loading VST3® and Audio Unit plugins, which could contain *any* code.
Most plugins that have been tested work just fine with ``pedalboard``, but some plugins may
not work with ``pedalboard``; at worst, some may even crash the Python interpreter without
warning and with no ability to catch the error.

Most audio plugins are "well-behaved" and conform to a set of conventions for how audio
plugins are supposed to work, but many do not conform to the VST3® or Audio Unit
specifications. ``pedalboard`` attempts to detect some common programming errors in plugins
and can work around many issues, including automatically detecting plugins that don't
clear their internal state when asked. Even so, plugins can misbehave without ``pedalboard``
noticing.

If audio is being rendered incorrectly or if audio is "leaking" from one ``process()`` call
to the next in an undesired fashion, try:

1. Passing silence to the plugin in between calls to ``process()``, to ensure that any
   reverb tails or other internal state has time to fade to silence
2. Reloading the plugin every time audio is processed (with ``pedalboard.load_plugin``)
