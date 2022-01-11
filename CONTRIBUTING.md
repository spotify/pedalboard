# How to Contribute

We'd love to get patches from you!

## Getting Started

To get your environment set up to build `pedalboard`, you'll need a working C++ compiler on your machine (`xcode-select --install` on macOS should do it). Try:

```shell
git clone --recurse-submodules --shallow-submodules git@github.com:spotify/pedalboard.git
cd pedalboard
pip3 install pybind11
pip3 install .
```

To compile a debug build of `pedalboard` that allows using a debugger (like gdb or lldb), use the following command to build the package locally and install a symbolic link for debugging:
```shell
python3 setup.py build develop
```

Then, you can `import pedalboard` from Python (or run the tests with `tox`) to test out your local changes.

To compile a debug build _faster_ by using [Ccache](https://ccache.dev/) (`brew install ccache` on macOS, similar elsewhere):
```shell
rm -rf build && CC="ccache clang" CXX="ccache clang++" DEBUG=1 python3 setup.py build -j8 develop
```

By default, [all `.cpp` files in the `pedalboard` directory (or subdirectories)](https://github.com/spotify/pedalboard/blob/master/setup.py#L129)
will be automatically compiled by `setup.py`.

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

We use `tox` for testing - running tests from end-to-end should be as simple as:

```
tox
```

## Style

Use [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html) for C++ code, and `black` with defaults for Python code.

## Issues

When creating an issue please try to ahere to the following format:

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
