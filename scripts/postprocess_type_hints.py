"""
This script is intended to postprocess type hint (.pyi) files produced by pybind11-stubgen.

A number of changes must be made to produce valid type hint files out-of-the-box.
Think of this as a more intelligent (or less intelligent) application of `git patch`.
"""

import os
import io
import re
import difflib
from pathlib import Path
from collections import defaultdict
import black
import argparse

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

OMIT_FILES = [
    "pedalboard-stubs/__init__.pyi",
    "pedalboard-stubs/pedalboard/__init__.pyi",
    "pedalboard_native-stubs/_internal/__init__.pyi",
]

# Any lines containing any of these substrings will be omitted from the pedalboard_native stubs:
OMIT_LINES_CONTAINING = [
    "installed_plugins = [",
    "typing._GenericAlias",
    "typing._VariadicGenericAlias",
    "typing._SpecialForm",
    "__annotations__: dict",
    "_AVAILABLE_PLUGIN_CLASSES: list",
]

REPLACEMENTS = [
    # object is a superclass of `str`, which would make these declarations ambiguous:
    ("file_like: object", "file_like: typing.BinaryIO"),
    # "r" is the default file open/reading mode:
    ("mode: str = 'r'", r'mode: _Literal["r"] = "r"'),
    # ... but when using "w", "w" needs to be specified explicitly (no default):
    ("mode: str = 'w'", r'mode: _Literal["w"]'),
    # ndarrays need to be corrected as well:
    (r"numpy\.ndarray\[(.*?)\]", r"numpy.ndarray[typing.Any, numpy.dtype[\1]]"),
    # We return an ndarray with indeterminate type from read_raw,
    # which is represented by a py::handle object.
    # Type this as a numpy.ndarray with no type arguments:
    ("-> handle:", "-> numpy.ndarray:"),
    # None of our enums are properly detected by pybind11-stubgen:
    (r"Resample\.Quality = Quality\.", "Resample.Quality = Resample.Quality."),
    (r": pedalboard_native\.Resample\.Quality", ": Resample.Quality"),
    (r": pedalboard_native\.LadderFilter\.Mode", ": LadderFilter.Mode"),
    (r"import pedalboard_native(\.?.*)$", r"import pedalboard_native\1  # type: ignore"),
    (
        # For Python 3.6 compatibility:
        r"import typing",
        "\n".join(
            [
                "import sys",
                "import typing",
                "if sys.version_info < (3, 8):",
                "    import typing_extensions",
                "    _Literal = typing_extensions.Literal",
                "else:",
                "    _Literal = typing.Literal",
            ]
        ),
    ),
]

REMOVE_INDENTED_BLOCKS_STARTING_WITH = [
    # TODO(psobot): Add a conditional to _AudioUnitPlugin
    # to ensure type stubs only show up on macOS:
    "class _AudioUnitPlugin(Plugin):"
]

LINES_TO_IGNORE_FOR_MATCH = {"from __future__ import annotations"}


def stub_files_match(a: str, b: str) -> bool:
    a = "".join(
        [x for x in a.split("\n") if x.strip() and x.strip() not in LINES_TO_IGNORE_FOR_MATCH]
    )
    b = "".join(
        [x for x in b.split("\n") if x.strip() and x.strip() not in LINES_TO_IGNORE_FOR_MATCH]
    )
    return a == b


def main():
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
    args = parser.parse_args()

    output_file_to_source_files = defaultdict(list)
    for source_path in Path(args.source_directory).rglob("**/*.pyi"):
        source_path = str(source_path)
        if any(x in source_path for x in OMIT_FILES):
            print(f"Skipping possible source file '{source_path}'...")
            continue
        output_module_path = ["pedalboard"] + source_path.split("/")[2:-1]
        if not source_path.endswith("__init__.pyi"):
            raise NotImplementedError("Not sure how to create stubs not at the module level.")
        output_file_name = "/".join(output_module_path + ["__init__.pyi"])
        output_file_to_source_files[output_file_name].append(source_path)

    for output_file_name, source_files in output_file_to_source_files.items():
        os.makedirs(os.path.dirname(output_file_name), exist_ok=True)

        print(f"Writing stub file {output_file_name}...")

        file_contents = io.StringIO()
        for source_file in source_files:
            with open(source_file) as f:
                in_excluded_indented_block = False
                for line in f:
                    if all(x not in line for x in OMIT_LINES_CONTAINING):
                        if any(line.startswith(x) for x in REMOVE_INDENTED_BLOCKS_STARTING_WITH):
                            in_excluded_indented_block = True
                            continue

                        if line.strip() and not line.startswith(" "):
                            in_excluded_indented_block = False

                        if in_excluded_indented_block:
                            continue

                        for find, replace in REPLACEMENTS:
                            if re.findall(find, line):
                                print(f"\tReplacing '{find}' with '{replace}'...")
                                line = re.sub(find, replace, line)
                        file_contents.write(line)
                print(f"\tRead {f.tell():,} bytes of stubs from {source_file}.")

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
