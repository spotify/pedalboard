# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pedalboard is a Python library (with C++ internals) for audio processing: reading, writing, rendering, and adding effects. Built by Spotify's Audio Intelligence Lab using pybind11 bindings over the JUCE framework.

## Build Commands

```bash
# Clone with submodules (required for vendors/JUCE)
git clone --recurse-submodules --shallow-submodules git@github.com:spotify/pedalboard.git

# Install build deps and build
pip install pybind11 scikit-build-core
pip install --no-build-isolation -ve .

# Fast debug build with ccache (macOS)
rm -rf build && CC="ccache clang" CXX="ccache clang++" DEBUG=1 python3 -m pip install -e .

# Fast debug build with ccache (Linux, GCC)
rm -rf build && CC="ccache gcc" CXX="scripts/ccache_g++" DEBUG=1 python3 setup.py build -j8 develop
```

Build environment variables: `DEBUG=1`, `DISABLE_LTO`, `USE_MARCH_NATIVE`, `USE_ASAN`, `USE_TSAN`, `USE_MSAN`.

## Testing

```bash
# Run all tests
pytest tests/

# Run a single test file
pytest tests/test_python_interface.py -v

# Run a single test
pytest tests/test_python_interface.py::test_no_transforms

# Via tox (runs coverage)
tox
```

Test dependencies are in `test-requirements.txt`. CI parallelizes tests via `NUM_TEST_WORKERS` and `TEST_WORKER_INDEX` env vars (see `tests/conftest.py`).

## Linting and Formatting

```bash
# Python lint
ruff check pedalboard

# Python format check
ruff format --check pedalboard --diff

# Type checking
pyright pedalboard tests/test_*.py

# C++ formatting (clang-format 14, LLVM style)
# Applied to pedalboard/ C++ files, excluding vendors/
```

Ruff config is in `pyproject.toml`: line-length 100, target Python 3.10+.

## Architecture

**Python layer** (`pedalboard/`):
- `__init__.py` — re-exports from `pedalboard_native` (the compiled C++ module)
- `_pedalboard.py` — `Pedalboard` class (effect chain container) and `load_plugin()` for VST3/AU
- `io/` — `AudioFile` (read/write WAV, FLAC, MP3, OGG, AIFF) and `AudioStream` (live audio)

**C++ native module** (`pedalboard_native`):
- `pedalboard/python_bindings.cpp` — main pybind11 entry point
- `pedalboard/plugins/*.h` — individual effect plugin implementations (Compressor, Reverb, Delay, Chorus, etc.), each wrapping a JUCE processor via `JucePlugin<T>` template
- `pedalboard/plugin_templates/` — reusable plugin wrappers (Resample, ForceMono, etc.)
- `pedalboard/juce_overrides/` — patched JUCE module inclusions

**Type stubs** (`pedalboard_native/*.pyi`):
- Generated via `pybind11-stubgen`, post-processed by `scripts/postprocess_type_hints`
- Update process documented in CONTRIBUTING.md

**Vendor libraries** (`vendors/`): FFTW3, RubberBand, LAME, libgsm, libFLAC — compiled as part of the build.

**Build system**: scikit-build-core (primary, via `pyproject.toml` + `CMakeLists.txt`) with legacy `setup.py` still functional. All `.cpp` and `.mm` files under `pedalboard/` are compiled automatically.

## Platform Notes

- **macOS**: Links Accelerate, AudioToolbox, CoreAudio, and other frameworks. Supports Audio Units.
- **Linux**: Requires libfreetype-dev, xorg-dev, ALSA. FFTW3 used for FFT (vendored).
- **Windows**: MSVC required. No AU support.
- C++17 required. SIMD: AVX on x86, NEON on ARM.

## Key Design Patterns

- Plugin architecture: `Plugin` base → `JucePlugin<juce::dsp::SomeProcessor>` template
- Container composition: `Pedalboard` extends `Chain` for serial/parallel plugin graphs
- GIL is released during C++ audio processing for thread safety
- External plugin hosting: VST3 cross-platform, Audio Units macOS-only
