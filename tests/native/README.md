# ExternalPlugin buffer regression tests

These tests instantiate the production `ExternalPlugin` host through a native
subclass using its protected instance-injection constructor and a small native
instrument. Its stereo auxiliary output is mandatory: attempts to disable it or
change either output layout are rejected. Before touching audio samples, the
instrument verifies that every enabled output channel has writable storage. Main
and auxiliary outputs contain different known values.

The single-bus cases are positive controls. Multi-bus cases independently cover
warm-up and MIDI rendering. Rendering checks exact main-output shape and samples,
multiple processing blocks, and a partial final block. Rendering disables warm-up
so that a warm-up failure cannot hide a separate rendering failure. Each case
runs in its own process to isolate native failures.

Only Python MIDI normalization is replaced with an identity function for already
normalized `(bytes, time)` messages. The production C++ MIDI parser, bus
negotiation, warm-up, and rendering code execute unchanged. These tests do not
cover VST3 scanning, controller initialization, or plugin-format buffer mapping.
The injected host has no file-based factory: the harness manages its instance,
uses `reset=false`, and does not exercise reset/re-instantiation.

Build with the repository's pinned JUCE source, a C++17 compiler, CMake, and a
Python environment containing `pybind11`, `numpy`, and `pytest`. Run pytest with
the same Python version and ABI used to configure the executable; the wrapper
passes its module search paths to the embedded interpreter:

```sh
cmake -S tests/native -B /tmp/pedalboard-native-tests \
  -DPython_EXECUTABLE=/path/to/python \
  -DJUCE_DIR=/path/to/pinned/JUCE
cmake --build /tmp/pedalboard-native-tests --parallel 4
PEDALBOARD_NATIVE_TEST_BINARY=/tmp/pedalboard-native-tests/external_plugin_buffers \
  /path/to/python -m pytest tests/test_external_plugin_buffers.py -q
```

On Windows, adjust the executable path for the selected configuration and `.exe`
extension. The wrapper skips explicitly when `PEDALBOARD_NATIVE_TEST_BINARY` is
unset; it never compiles during test collection or execution. A configured path
that does not exist fails the tests.

The executable also accepts an individual case name. When running it directly,
make the build environment's installed Python packages visible to the embedded
interpreter (using the pytest wrapper above handles this automatically):

```sh
export PYTHONPATH="$(/path/to/python -c 'import sysconfig; print(sysconfig.get_path("purelib"))')${PYTHONPATH:+:$PYTHONPATH}"
/tmp/pedalboard-native-tests/external_plugin_buffers multi_bus_warm_up
/tmp/pedalboard-native-tests/external_plugin_buffers multi_bus_render_partial
```

## Captured failure (2026-11-09)

Verified on macOS arm64 with the pinned JUCE commit
`ddaa09110392a4419fecbb6d3022bede89b7e841` and Python 3.12.13.
The current allocation code yields **3 passed, 3 failed**: all single-bus
controls pass; multi-bus warm-up, full-block rendering, and partial-block
rendering fail with:

```text
Enabled output buses require 4 writable channels, but host supplied 2
```

Temporarily providing storage for all enabled channels made all six cases pass.
That experimental correction was removed, the binary was rebuilt, and the same
three failures were verified again. The branch intentionally retains the RED
regressions; the only host change is the protected instance-injection constructor.
The full Python suite was not run for this native test-only investigation.
