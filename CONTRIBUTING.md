# How to Contribute

We'd love to get patches from you!

## Getting Started

### Prerequisites

To compile Pedalboard from scratch, the following packages will need to be installed:

- [Python 3.10](https://www.python.org/downloads/) or higher.
- CMake and a C++ compiler (Xcode Command Line Tools on macOS).
- On Linux:
  - FreeType, X11, ALSA, and libsndfile development packages. On Debian or Ubuntu:
    `pkg-config libsndfile1 libx11-dev libxrandr-dev libxinerama-dev libxrender-dev
    libxcomposite-dev libxcb-xinerama0-dev libxcursor-dev libfreetype6-dev libasound2-dev`.

### Building Pedalboard

```shell
git clone --recurse-submodules https://github.com/spotify/pedalboard.git
cd pedalboard
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -e .
python -m pip install -r test-requirements.txt
python -m pip install ruff==0.15.22
```

To compile a debug build of `pedalboard` that allows using a debugger (like gdb or lldb), use the following command to build the package locally and install a symbolic link for debugging:
```shell
python -m pip install -e . --config-settings=cmake.build-type=Debug
```

Then run `python -m pytest tests` to test your changes. Reinstall the editable
package after changing C++ code.

Linux x86_64 builds target the portable AVX baseline by default. To optimize a local
build for the current machine instead, set `USE_MARCH_NATIVE=1` while building.
The previous `USE_PORTABLE_SIMD` variable is no longer used; builds that set it remain
portable because AVX is now the default.

To speed up repeated native builds, install [ccache](https://ccache.dev/). CMake
detects it automatically. The native extension is built by scikit-build-core
and CMake, using the sources listed in `CMakeLists.txt`.

While `pedalboard` is mostly C++ code, it ships with `.pyi` files to allow for type hints in text editors and via MyPy. To update the type hint files, use the following commands:

```shell
# Use pybind11-stubgen to create intermediate stub files:
pybind11-stubgen -o stubs_output pedalboard pedalboard_native
# Post-process the stub files into more human-readable, usable ones:
python3 -m scripts.postprocess_type_hints stubs_output pedalboard --check
# Run mypy.stubtest to ensure the resulting stubs are valid
python3 -m mypy.stubtest pedalboard --allowlist stubtest.allowlist
# If all looks good, commit the resulting stubs to Git.
```

## Workflow

We follow the [GitHub Flow Workflow](https://guides.github.com/introduction/flow/):

1.  Fork the project 
1.  Check out the `master` branch 
1.  Create a feature branch
1.  Write code and tests for your change 
1.  From your branch, make a pull request against `https://github.com/spotify/pedalboard` 
1.  Work with repo maintainers to get your change reviewed 
1.  Wait for your change to be pulled into `https://github.com/spotify/pedalboard/master`
1.  Delete your feature branch

## Testing

Run the local checks before submitting a pull request:

```
python -m pytest tests
ruff check pedalboard
ruff format --check pedalboard --diff
pyright pedalboard tests/test_*.py
```

## Style

Use [`clang-format` 14](https://clang.llvm.org/docs/ClangFormat.html) for C++ code
and Ruff 0.15.22 for Python code, matching CI.

## Issues

When creating an issue please try to adhere to the following format:

    module-name: One line summary of the issue (less than 72 characters)

    ### Expected behaviour

    As concisely as possible, describe the expected behaviour.

    ### Actual behaviour

    As concisely as possible, describe the observed behaviour.

    ### Steps to reproduce the behaviour

    List all relevant steps to reproduce the observed behaviour.

## Pull Requests

Files should be exempt of trailing spaces.

We adhere to a specific format for commit messages. Please write your commit
messages along these guidelines. Please keep the line width no greater than 80
columns (You can use `fmt -n -p -w 80` to accomplish this).

    module-name: One line description of your change (less than 72 characters)

    Problem

    Explain the context and why you're making that change.  What is the problem
    you're trying to solve? In some cases there is not a problem and this can be
    thought of being the motivation for your change.

    Solution

    Describe the modifications you've done.

    Result

    What will change as a result of your pull request? Note that sometimes this
    section is unnecessary because it is self-explanatory based on the solution.

Some important notes regarding the summary line:

* Describe what was done; not the result 
* Use the active voice 
* Use the present tense 
* Capitalize properly 
* Do not end in a period — this is a title/subject 
* Prefix the subject with its scope

## Documentation

We also welcome improvements to the project documentation or to the existing
docs. Please file an [issue](https://github.com/spotify/pedalboard/issues/new).

## First Contributions

If you are a first time contributor to `pedalboard`,  familiarize yourself with the:
* [Code of Conduct](CODE_OF_CONDUCT.md)
* [GitHub Flow Workflow](https://guides.github.com/introduction/flow/)
<!-- * Issue and pull request style guides -->

When you're ready, navigate to [issues](https://github.com/spotify/pedalboard/issues/new). Some issues have been identified by community members as [good first issues](https://github.com/spotify/pedalboard/labels/good%20first%20issue). 

There is a lot to learn when making your first contribution. As you gain experience, you will be able to make contributions faster. You can submit an issue using the [question](https://github.com/spotify/pedalboard/labels/question) label if you encounter challenges.  

# License 

By contributing your code, you agree to license your contribution under the 
terms of the [LICENSE](https://github.com/spotify/pedalboard/blob/master/LICENSE).

# Code of Conduct

Read our [Code of Conduct](CODE_OF_CONDUCT.md) for the project.

# Troubleshooting

## Building the project

### `ModuleNotFoundError: No module named 'pybind11'`

Try updating your version of `pip`:
```shell
pip install --upgrade pip
```

### `Failed to establish a new connection: [Errno -2] Name or service not known'`
You may have networking issues. Check to make sure you do not have the `PIP_INDEX_URL` environment variable set (or that it points to a valid index).

### `fatal error: Python.h: No such file or directory`
Ensure you have the Python development packages installed.
You will need to find correct package for your operating system. (i.e.: `python-dev`, `python-devel`, etc.)

### `fatal error: lame/include/lame.h: No such file or directory`
Ensure that all Git submodules have been updated:
```shell
git submodule update --init
```

### `AttributeError: 'NoneType' object has no attribute 'group'`
- Ensure that you have Tox version 4 or greater installed
- _or_ set `ignore_basepython_conflict=true` in `tox.ini`
- _or_ install Tox using `pip` and not your system package manager
