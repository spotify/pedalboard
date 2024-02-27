"""
This script is intended to postprocess type hint (.pyi) files produced by pybind11-stubgen.

A number of changes must be made to produce valid type hint files out-of-the-box.
Think of this as a more intelligent (or less intelligent) application of `git patch`.
"""

import argparse
import difflib
import io
import os
import re
from collections import defaultdict
from pathlib import Path

import black

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

OMIT_FILES = []

# Any lines containing any of these substrings will be omitted from the pedalboard_native stubs:
OMIT_LINES_CONTAINING = [
    "installed_plugins = [",
    "typing._GenericAlias",
    "typing._VariadicGenericAlias",
    "typing._SpecialForm",
    "__annotations__: dict",
    "_AVAILABLE_PLUGIN_CLASSES: list",
]

MULTILINE_REPLACEMENTS = [
    # Users don't want "Peter Sobot’s iPhone Microphone" to show up in their type hints:
    (r"input_device_names = \[[^]]*\]\n", "input_device_names: typing.List[str] = []\n"),
    (
        r"output_device_names = \[[^]]*\]\n",
        "output_device_names: typing.List[str] = []\n",
    ),
]

REPLACEMENTS = [
    # object is a superclass of `str`, which would make these declarations ambiguous:
    (
        r"file_like: object, mode: str = 'r'",
        r"file_like: typing.Union[typing.BinaryIO, memoryview], mode: str = 'r'",
    ),
    (
        r"file_like: object\) -> ReadableAudioFile:",
        "file_like: typing.Union[typing.BinaryIO, memoryview]) -> ReadableAudioFile:",
    ),
    ("file_like: object", "file_like: typing.BinaryIO"),
    # "r" is the default file open/reading mode:
    ("mode: str = 'r'", r'mode: Literal["r"] = "r"'),
    # ... but when using "w", "w" needs to be specified explicitly (no default):
    ("mode: str = 'w'", r'mode: Literal["w"]'),
    # ndarrays need to be corrected as well:
    (r"numpy\.ndarray\[(.*?)\]", r"numpy.ndarray[typing.Any, numpy.dtype[\1]]"),
    (
        r"import typing",
        "\n".join(
            [
                "import typing",
                "from typing_extensions import Literal",
                "from enum import Enum",
                "import threading",
            ]
        ),
    ),
    # None of our enums are properly detected by pybind11-stubgen. These are brittle hacks:
    (r"0, quality: Resample\.Quality", "0, quality: Quality"),
    (
        r"self, quality: Resample\.Quality = Quality",
        "self, quality: Resample.Quality = Resample.Quality",
    ),
    (
        r"pedalboard_native\.Resample\.Quality = Quality",
        "pedalboard_native.Resample.Quality = pedalboard_native.Resample.Quality",
    ),
    (
        r".*: pedalboard_native.ExternalPluginReloadType.*",
        "",
    ),
    # Enum values that should not be in __all__:
    (r'.*"ClearsAudioOnReset",.*', ""),
    (r'.*"PersistsAudioOnReset",.*', ""),
    (r'.*"Unknown",.*', ""),
    (r'.*"ExternalPluginReloadType",.*', ""),
    # Remove type hints in docstrings, added unnecessarily by pybind11-stubgen
    (r".*?:type:.*$", ""),
    # MyPy chokes on classes that contain both __new__ and __init__.
    # Remove all bare, arg-free inits:
    (r"def __init__\(self\) -> None: ...", ""),
    # Sphinx gets confused when inheriting twice from the same base class:
    (r"\(ExternalPlugin, Plugin\)", "(ExternalPlugin)"),
    # pybind11 has trouble when trying to include a type hint for a Python type from C++:
    (r"close_event: object = None", r"close_event: typing.Optional[threading.Event] = None"),
    # We allow passing an optional py::object to ExternalPlugin, but in truth,
    # that needs to be Dict[str, Union[str, float, int, bool]]:
    # (r": object = None", ": typing.Dict[str, typing.Union[str, float, int, bool]] = {}"),
    (
        "import typing",
        """
import typing

original_overload = typing.overload
__OVERLOADED_DOCSTRINGS = {}

def patch_overload(func):
    original_overload(func)
    if func.__doc__:
        __OVERLOADED_DOCSTRINGS[func.__qualname__] = func.__doc__
    else:
        func.__doc__ = __OVERLOADED_DOCSTRINGS.get(func.__qualname__)
    if func.__doc__:
        # Work around the fact that pybind11-stubgen generates
        # duplicate docstrings sometimes, once for each overload:
        docstring = func.__doc__
        if docstring[len(docstring) // 2:].strip() == docstring[:-len(docstring) // 2].strip():
            func.__doc__ = docstring[len(docstring) // 2:].strip()
    return func

typing.overload = patch_overload
    """,
    ),
]

REMOVE_INDENTED_BLOCKS_STARTING_WITH = [
    # TODO(psobot): Add a conditional to _AudioUnitPlugin
    # to ensure type stubs only show up on macOS:
    # "class _AudioUnitPlugin(Plugin):"
    "class ExternalPluginReloadType",
]

# .pyi files usually don't care about dependent types being present in order in the file;
# but if we want to use Sphinx, our Python files need to be both lexically and semantically
# valid as regular Python files. So...
INDENTED_BLOCKS_TO_MOVE_TO_END = ["class GSMFullRateCompressor"]

LINES_TO_IGNORE_FOR_MATCH = {"from __future__ import annotations"}


def stub_files_match(a: str, b: str) -> bool:
    a = "".join(
        [x for x in a.split("\n") if x.strip() and x.strip() not in LINES_TO_IGNORE_FOR_MATCH]
    )
    b = "".join(
        [x for x in b.split("\n") if x.strip() and x.strip() not in LINES_TO_IGNORE_FOR_MATCH]
    )
    return a == b


def main(args=None):
    parser = argparse.ArgumentParser(
        description="Post-process type hint files produced by pybind11-stubgen for Pedalboard."
    )
    parser.add_argument(
        "source_directory",
        default=os.path.join(REPO_ROOT, "pybind11-stubgen-output"),
    )
    parser.add_argument(
        "target_directory",
        default=os.path.join(REPO_ROOT, "pedalboard"),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=(
            "Return a non-zero exit code if files on disk don't match what this script would"
            " generate."
        ),
    )
    args = parser.parse_args(args)

    output_file_to_source_files = defaultdict(list)
    for source_path in Path(args.source_directory).rglob("*.pyi"):
        output_file_to_source_files[str(source_path)].append(str(source_path))

    for output_file_name, source_files in output_file_to_source_files.items():
        os.makedirs(os.path.dirname(output_file_name), exist_ok=True)

        print(f"Writing stub file {output_file_name}...")

        file_contents = io.StringIO()
        end_of_file_contents = io.StringIO()
        for source_file in source_files:
            module_name = output_file_name.replace("__init__.pyi", "").replace("/", ".").rstrip(".")
            with open(source_file) as f:
                source_file_contents = f.read()
                for find, replace in MULTILINE_REPLACEMENTS:
                    source_file_contents = re.sub(
                        find, replace, source_file_contents, flags=re.DOTALL
                    )
                lines = [x + "\n" for x in source_file_contents.split("\n")]

                in_excluded_indented_block = False
                in_moved_indented_block = False
                for line in lines:
                    if all(x not in line for x in OMIT_LINES_CONTAINING):
                        if any(line.startswith(x) for x in REMOVE_INDENTED_BLOCKS_STARTING_WITH):
                            in_excluded_indented_block = True
                            continue
                        elif any(line.startswith(x) for x in INDENTED_BLOCKS_TO_MOVE_TO_END):
                            in_excluded_indented_block = False
                            in_moved_indented_block = True
                        elif line.strip() and not line.startswith(" "):
                            in_excluded_indented_block = False
                            in_moved_indented_block = False

                        if in_excluded_indented_block:
                            continue

                        for _tuple in REPLACEMENTS:
                            if len(_tuple) == 2:
                                find, replace = _tuple
                                only_in_module = None
                            else:
                                find, replace, only_in_module = _tuple
                            if only_in_module and only_in_module != module_name:
                                continue
                            results = re.findall(find, line)
                            if results:
                                line = re.sub(find, replace, line)

                        if in_moved_indented_block:
                            end_of_file_contents.write(line)
                        else:
                            file_contents.write(line)
                print(f"\tRead {f.tell():,} bytes of stubs from {source_file}.")

        # Append end-of-file contents at the end:
        file_contents.write("\n")
        file_contents.write(end_of_file_contents.getvalue())

        # Run black:
        try:
            output = black.format_file_contents(
                file_contents.getvalue(),
                fast=False,
                mode=black.FileMode(is_pyi=True, line_length=100),
            )
        except black.report.NothingChanged:
            output = file_contents.getvalue()

        if args.check:
            with open(output_file_name, "r") as f:
                existing = f.read()
                if not stub_files_match(existing, output):
                    error = f"File that would be generated ({output_file_name}) "
                    error += "does not match existing file!\n"
                    error += f"Existing file had {len(existing):,} bytes, "
                    error += f"expected {len(output):,} bytes.\nDiff was:\n"
                    diff = difflib.context_diff(existing.split("\n"), output.split("\n"))
                    error += "\n".join([x.strip() for x in diff])
                    raise ValueError(error)
        else:
            with open(output_file_name, "w") as o:
                o.write(output)
                print(f"\tWrote {o.tell():,} bytes of stubs to {output_file_name}.")

    print("Done!")


if __name__ == "__main__":
    main()
