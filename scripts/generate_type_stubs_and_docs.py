import argparse
import difflib
import importlib
import inspect
import io
import os
import pathlib
import re
import shutil
import subprocess
import sys
import traceback
import typing
from collections import defaultdict
from contextlib import contextmanager
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Dict, List

import black
import mypy.stubtest
import psutil
import sphinx.ext.autodoc.importer
from mypy.stubtest import parse_options as mypy_parse_options
from mypy.stubtest import test_stubs
from pybind11_stubgen import ClassStubsGenerator, StubsGenerator
from pybind11_stubgen import main as pybind11_stubgen_main
from sphinx.cmd.build import main as sphinx_build_main

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

"""
This script is intended to postprocess type hint (.pyi) files produced by pybind11-stubgen.

A number of changes must be made to produce valid type hint files out-of-the-box.
Think of this as a more intelligent (or less intelligent) application of `git patch`.
"""

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
    # Users don't want "Peter Sobot's iPhone Microphone" to show up in their type hints:
    (r"input_device_names = \[[^]]*\]\n", "input_device_names: typing.List[str] = []\n"),
    (
        r"output_device_names = \[[^]]*\]\n",
        "output_device_names: typing.List[str] = []\n",
    ),
    # Replace AudioFile.__new__ and __init__ with overloads for proper type narrowing.
    # Using original_overload (not typing.overload) because patch_overload confuses pyright.
    (
        r"(class AudioFile:[\s\S]*?)(\n    def __new__\([\s\S]*?-> typing\.Union\[ReadableAudioFile, WriteableAudioFile\]: \.\.\.)",
        r"\1\n"
        r"    # Overloads for type narrowing based on mode\n"
        r"    @original_overload\n"
        r"    @staticmethod\n"
        r"    def __new__(\n"
        r"        cls: typing.Type[AudioFile],\n"
        r"        filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview],\n"
        r"        mode: Literal[\"w\"],\n"
        r"        samplerate: float,\n"
        r"        num_channels: int = 1,\n"
        r"        bit_depth: int = 16,\n"
        r"        quality: typing.Optional[typing.Union[str, float]] = None,\n"
        r"        format: typing.Optional[str] = None,\n"
        r"    ) -> WriteableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    @staticmethod\n"
        r"    def __new__(\n"
        r"        cls: typing.Type[AudioFile],\n"
        r"        filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview],\n"
        r"        mode: Literal[\"r\"],\n"
        r"    ) -> ReadableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    @staticmethod\n"
        r"    def __new__(\n"
        r"        cls: typing.Type[AudioFile],\n"
        r"        filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview],\n"
        r"    ) -> ReadableAudioFile: ...\n"
        r"    def __init__(self, *args: typing.Any, **kwargs: typing.Any) -> None: ...",
    ),
    # Add mode overloads to ReadableAudioFile for calls routed through AudioFile
    (
        r"(class ReadableAudioFile\(AudioFile\):[\s\S]*?)\n    def __new__\(\n        cls,\n        filename_or_file_like:[^)]+\) -> ReadableAudioFile: \.\.\.\n    def __init__\(\n        self,\n        filename_or_file_like:[^)]+\) -> None: \.\.\.",
        r"\1\n"
        r"    @original_overload\n"
        r"    def __new__(cls, filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview]) -> ReadableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    def __new__(cls, filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview], mode: Literal[\"r\"]) -> ReadableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    def __init__(self, filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview]) -> None: ...\n"
        r"    @original_overload\n"
        r"    def __init__(self, filename_or_file_like: typing.Union[str, typing.BinaryIO, memoryview], mode: Literal[\"r\"]) -> None: ...",
    ),
    # Add mode overloads to WriteableAudioFile for calls routed through AudioFile
    (
        r"(class WriteableAudioFile\(AudioFile\):[\s\S]*?def __exit__[^.]+\.\.\.)\n    def __new__\(\n        cls,\n        filename_or_file_like:[^)]+\n        samplerate:[^)]+\) -> WriteableAudioFile: \.\.\.\n    def __init__\(\n        self,\n        filename_or_file_like:[^)]+\n        samplerate:[^)]+\) -> None: \.\.\.",
        r"\1\n"
        r"    @original_overload\n"
        r"    def __new__(cls, filename_or_file_like: typing.Union[str, typing.BinaryIO], samplerate: float, num_channels: int = 1, bit_depth: int = 16, quality: typing.Optional[typing.Union[str, float]] = None, format: typing.Optional[str] = None) -> WriteableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    def __new__(cls, filename_or_file_like: typing.Union[str, typing.BinaryIO], mode: Literal[\"w\"], samplerate: float, num_channels: int = 1, bit_depth: int = 16, quality: typing.Optional[typing.Union[str, float]] = None, format: typing.Optional[str] = None) -> WriteableAudioFile: ...\n"
        r"    @original_overload\n"
        r"    def __init__(self, filename_or_file_like: typing.Union[str, typing.BinaryIO], samplerate: float, num_channels: int = 1, bit_depth: int = 16, quality: typing.Optional[typing.Union[str, float]] = None, format: typing.Optional[str] = None) -> None: ...\n"
        r"    @original_overload\n"
        r"    def __init__(self, filename_or_file_like: typing.Union[str, typing.BinaryIO], mode: Literal[\"w\"], samplerate: float, num_channels: int = 1, bit_depth: int = 16, quality: typing.Optional[typing.Union[str, float]] = None, format: typing.Optional[str] = None) -> None: ...",
    ),
    # Remove _reload_type property blocks (internal implementation detail using ExternalPluginReloadType)
    # Match @property followed by def _reload_type and its docstring (using [\s\S] for multiline)
    (r'    @property\n    def _reload_type\(self\)[^\n]*\n        """[\s\S]*?"""\n', ""),
    (r'    @_reload_type\.setter\n    def _reload_type\(self[^\n]*\n        """[\s\S]*?"""\n', ""),
    # MyPy chokes on classes that contain both __new__ and __init__.
    # Remove all bare, arg-free inits (and their @typing.overload decorators):
    (r"    @typing\.overload\n    def __init__\(self\) -> None: \.\.\.\n", ""),
    # After removing one overload from a pair in Chain/Mix classes, a single @typing.overload
    # remains which is invalid. Remove it ONLY for the plugins: parameter pattern (from utils)
    (
        r"    @typing\.overload\n(    def __init__\(self, plugins:)",
        r"\1",
    ),
    # Fix VST3Plugin __call__ overload order to match ExternalPlugin (midi_messages first)
    # The raw stubs have input_array first, but parent has midi_messages first.
    # Raw stubs have single-line signatures before black formatting.
    (
        r'(class VST3Plugin\(ExternalPlugin, Plugin\):[\s\S]*?"""\n)'
        r'(    @typing\.overload\n    def __call__\(self, input_array: object[^\n]*\n        """[\s\S]*?"""\n)'
        r"(    @typing\.overload\n    def __call__\(self, midi_messages[^\n]*\.\.\.)",
        r"\1\3\n\2",
    ),
    # Fix VST3Plugin process overload order similarly
    (
        r'(    def load_preset\(self, preset_file_path: str\)[^\n]*\n        """[\s\S]*?"""\n)'
        r'(    @typing\.overload\n    def process\(self, input_array: object[^\n]*\n        """[\s\S]*?"""\n)'
        r"(    @typing\.overload\n    def process\(self, midi_messages[^\n]*\.\.\.)",
        r"\1\3\n\2",
    ),
    # Fix AudioUnitPlugin __call__ and process overload order similarly
    (
        r'(class AudioUnitPlugin\(ExternalPlugin, Plugin\):[\s\S]*?"""\n)'
        r'(    @typing\.overload\n    def __call__\(self, input_array: object[^\n]*\n        """[\s\S]*?"""\n)'
        r"(    @typing\.overload\n    def __call__\(self, midi_messages[^\n]*\.\.\.)",
        r"\1\3\n\2",
    ),
    (
        r'(class AudioUnitPlugin[\s\S]*?def load_preset\(self, preset_file_path: str\)[^\n]*\n        """[\s\S]*?"""\n)'
        r'(    @typing\.overload\n    def process\(self, input_array: object[^\n]*\n        """[\s\S]*?"""\n)'
        r"(    @typing\.overload\n    def process\(self, midi_messages[^\n]*\.\.\.)",
        r"\1\3\n\2",
    ),
]

REPLACEMENTS = [
    # Replace generic 'object' with a proper ArrayLike type hint for audio data:
    (r"input_array: object", r"input_array: ArrayLike"),
    (r"input: object", r"input: ArrayLike"),
    (r"samples: object", r"samples: ArrayLike"),
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
                "from typing import Optional",
                "from typing_extensions import Literal",
                "from enum import Enum",
                "import threading",
                "",
                "# Array-like type that includes numpy arrays, torch tensors, etc.",
                "# At runtime, we accept any array-like object (numpy arrays, torch tensors,",
                "# tensorflow tensors, jax arrays, or anything with __array__ method).",
                "# For type checking, we use numpy.typing.ArrayLike which covers most cases.",
                "if typing.TYPE_CHECKING:",
                "    import numpy",
                "    import numpy.typing",
                "    ArrayLike = numpy.typing.ArrayLike",
                "else:",
                "    ArrayLike = typing.Any",
            ]
        ),
    ),
    (
        r"default_input_device_name = [\"'].*?[\"']",
        "default_input_device_name: Optional[str] = None",
    ),
    (
        r"default_output_device_name = [\"'].*?[\"']",
        "default_output_device_name: Optional[str] = None",
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
        # Remove any lines referencing ExternalPluginReloadType (internal implementation detail)
        r".*ExternalPluginReloadType.*",
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
    # Remove all bare, arg-free inits (moved to MULTILINE_REPLACEMENTS):
    (r"def __init__\(self\) -> None: \.\.\.", ""),
    # Sphinx gets confused when inheriting twice from the same base class:
    (r"\(ExternalPlugin, Plugin\)", "(ExternalPlugin)"),
    # Python <3.9 doesn't like bare lists in type hints:
    (r"-> list\[", "-> typing.List["),
    (r": list\[", ": typing.List["),
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


def postprocess_type_hints_main(args=None):
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


MAX_DIFF_LINE_LENGTH = 150


SPHINX_REPLACEMENTS = {
    # Pretend that the `pedalboard_native` package just does not exist:
    r"pedalboard_native\.": r"pedalboard.",
    # Remove any "self: <foo>," lines:
    (
        r'<em class="sig-param"><span class="n"><span class="pre">self'
        r'</span></span><span class="p"><span class="pre">:.*?</em>, '
    ): r"",
    # Remove any "(self: <foo>)" lines too:
    (
        r'<em class="sig-param"><span class="n"><span class="pre">self</span>'
        r'</span><span class="p"><span class="pre">:.*?</em><span class="sig-paren">\)</span>'
    ): r'<span class="sig-paren">)</span>',
    # Sometimes Sphinx fully-qualifies names, other times it doesn't. Here are some special cases:
    r'<span class="pre">pedalboard\.Plugin</span>': r'<span class="pre">Plugin</span>',
}


def patch_mypy_stubtest():
    def patched_verify_metaclass(*args, **kwargs):
        # Just ignore metaclass mismatches entirely:
        return []

    mypy.stubtest._verify_metaclass = patched_verify_metaclass


def patch_pybind11_stubgen():
    """
    Patch ``pybind11_stubgen`` to generate more ergonomic code for Enum-like classes.
    This generates a subclass of :class:``Enum`` for each Pybind11-generated Enum,
    which is not strictly correct, but produces much nicer documentation and allows
    for a much more Pythonic API.
    """

    original_class_stubs_generator_new = ClassStubsGenerator.__new__

    class EnumClassStubsGenerator(StubsGenerator):
        def __init__(self, klass):
            self.klass = klass
            assert inspect.isclass(klass)
            assert klass.__name__.isidentifier()
            assert hasattr(klass, "__entries")

            self.doc_string = None
            self.enum_names = []
            self.enum_values = []
            self.enum_docstrings = []

        def get_involved_modules_names(self):
            return set()

        def parse(self):
            self.doc_string = self.klass.__doc__ or ""
            self.doc_string = self.doc_string.split("Members:")[0]
            for name, (value_object, docstring) in getattr(self.klass, "__entries").items():
                self.enum_names.append(name)
                self.enum_values.append(value_object.value)
                self.enum_docstrings.append(docstring)

        def to_lines(self):
            result = [
                "class {class_name}(Enum):{doc_string}".format(
                    class_name=self.klass.__name__,
                    doc_string="\n" + self.format_docstring(self.doc_string)
                    if self.doc_string
                    else "",
                ),
            ]
            for name, value, docstring in sorted(
                list(zip(self.enum_names, self.enum_values, self.enum_docstrings)),
                key=lambda x: x[1],
            ):
                result.append(f"    {name} = {value}  # fmt: skip")
                result.append(f"{self.format_docstring(docstring)}")
            if not self.enum_names:
                result.append(self.indent("pass"))
            return result

    def patched_class_stubs_generator_new(cls, klass, *args, **kwargs):
        if hasattr(klass, "__entries"):
            return EnumClassStubsGenerator(klass, *args, **kwargs)
        else:
            return original_class_stubs_generator_new(cls)

    ClassStubsGenerator.__new__ = patched_class_stubs_generator_new


def import_stub(stubs_path: str, module_name: str) -> typing.Any:
    """
    Import a stub file (.pyi) as a regular Python module.
    Note that two modules of the same name cannot (usually) be imported,
    so additional care may need to be taken after using this method to
    change ``sys.modules`` to avoid clobbering existing modules.
    """
    sys.path_hooks.insert(
        0,
        importlib.machinery.FileFinder.path_hook((importlib.machinery.SourceFileLoader, [".pyi"])),
    )
    sys.path.insert(0, stubs_path)

    try:
        return importlib.import_module(module_name)
    finally:
        sys.path.pop(0)
        sys.path_hooks.pop(0)


def patch_sphinx_to_read_pyi():
    """
    Sphinx doesn't know how to read .pyi files, but we use .pyi files as our
    "source of truth" for the public API that we expose to IDEs and our documentation.
    This patch tells Sphinx how to read .pyi files, using them to replace their .py
    counterparts.
    """
    old_import_module = sphinx.ext.autodoc.importer.import_module

    def patch_import_module(modname: str, *args, **kwargs) -> typing.Any:
        if modname in sys.modules:
            return sys.modules[modname]
        try:
            return import_stub(".", modname)
        except ImportError:
            return old_import_module(modname, *args, **kwargs)
        except Exception as e:
            print(f"Failed to import stub module: {e}")
            traceback.print_exc()
            raise

    sphinx.ext.autodoc.importer.import_module = patch_import_module


@contextmanager
def isolated_imports(only: typing.Set[str] = {}):
    """
    When used as a context manager, this function scopes all imports
    that happen within it as local to the scope.

    Put another way: if you import something inside a
    ``with isolated_imports()`` block, it won't be imported after
    the block is done.
    """
    before = list(sys.modules.keys())
    yield
    for module_name in list(sys.modules.keys()):
        if module_name not in before and module_name in only:
            del sys.modules[module_name]


def remove_non_public_files(output_dir: str):
    try:
        shutil.rmtree(os.path.join(output_dir, ".doctrees"))
    except Exception:
        pass
    try:
        os.unlink(os.path.join(output_dir, ".buildinfo"))
    except Exception:
        pass


def trim_diff_line(x: str) -> str:
    x = x.strip()
    if len(x) > MAX_DIFF_LINE_LENGTH:
        suffix = f" [plus {len(x) - MAX_DIFF_LINE_LENGTH:,} more characters]"
        return x[: MAX_DIFF_LINE_LENGTH - len(suffix)] + suffix
    else:
        return x


def glob_matches(filename: str, globs: List[str]) -> bool:
    for glob in globs:
        if glob.startswith("*") and filename.lower().endswith(glob[1:].lower()):
            return True
        if glob in filename:
            return True
    return False


def postprocess_sphinx_output(directory: str, renames: Dict[str, str]):
    """
    I've spent 7 hours of my time this weekend fighting with Sphinx.
    Rather than find the "correct" way to fix this, I'm just going to
    overwrite the HTML output with good old find-and-replace.
    """
    for html_path in pathlib.Path(directory).rglob("*.html"):
        html_contents = html_path.read_text()
        for find, replace in renames.items():
            results = re.findall(find, html_contents)
            if results:
                html_contents = re.sub(find, replace, html_contents)
        with open(html_path, "w") as f:
            f.write(html_contents)


def main():
    parser = argparse.ArgumentParser(
        description="Generate type stub files (.pyi) and Sphinx documentation for Pedalboard."
    )
    parser.add_argument(
        "--docs-output-dir", default="docs", help="Output directory for documentation HTML files."
    )
    parser.add_argument(
        "--docs-input-dir",
        default=os.path.join("docs", "source"),
        help="Input directory for Sphinx.",
    )
    parser.add_argument(
        "--skip-regenerating-type-hints",
        action="store_true",
        help="If set, don't bother regenerating or reprocessing type hint files.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=(
            "If set, compare the existing files with those that would have been generated if this"
            " script were re-run."
        ),
    )
    parser.add_argument(
        "--skip-comparing",
        nargs="*",
        default=["*.js", "*.css"],
        help=(
            "If set and if --check is passed, the provided filenames (including '*' globs) will be"
            " ignored when comparing expected file contents against actual file contents."
        ),
    )
    args = parser.parse_args()

    patch_mypy_stubtest()
    patch_pybind11_stubgen()
    patch_sphinx_to_read_pyi()

    if not args.skip_regenerating_type_hints:
        with isolated_imports(
            {
                "pedalboard",
                "pedalboard.io",
                "pedalboard_native",
                "pedalboard_native.io",
                "pedalboard_native.utils",
            }
        ):
            # print("Generating type stubs from pure-Python code...")
            # subprocess.check_call(["stubgen", "-o", tempdir, "pedalboard/_pedalboard.py"])

            # Generate .pyi stubs files from Pedalboard's native (Pybind11) source.
            # Documentation will be copied from the Pybind11 docstrings, and these stubs
            # files will be used as the "source of truth" for both IDE autocompletion
            # and Sphinx documentation.
            print("Generating type stubs from native code...")
            pybind11_stubgen_main(["-o", "pedalboard_native", "pedalboard_native", "--no-setup-py"])

            # Move type hints out of pedalboard_native/something_else/*:
            native_dir = pathlib.Path("pedalboard_native")
            native_subdir = [f for f in native_dir.glob("*") if "stubs" in f.name][0]
            shutil.copytree(native_subdir, native_dir, dirs_exist_ok=True)
            shutil.rmtree(native_subdir)

            # Post-process the type hints generaetd by pybind11_stubgen; we can't patch
            # everything easily, so we do string manipulation after-the-fact and run ``black``.
            print("Postprocessing generated type hints...")
            postprocess_type_hints_main(
                ["pedalboard_native", "pedalboard_native"] + (["--check"] if args.check else [])
            )

            # Run mypy.stubtest to ensure that the results are correct and nothing got missed:
            if sys.version_info > (3, 6):
                print("Running `mypy.stubtest` to validate stubs match...")
                test_stubs(
                    mypy_parse_options(
                        [
                            "pedalboard",
                            "--allowlist",
                            "stubtest.allowlist",
                            "--ignore-missing-stub",
                            "--ignore-unused-allowlist",
                        ]
                    )
                )
        # Re-run this same script in a fresh interpreter, but with skip_regenerating_type_hints
        # enabled:
        subprocess.check_call(
            [psutil.Process(os.getpid()).exe()] + sys.argv + ["--skip-regenerating-type-hints"]
        )
        return

    # Why is this necessary? I don't know, but without it, things fail.
    print("Importing numpy to ensure a successful Pedalboard stub import...")
    import numpy  # noqa

    print("Importing .pyi files for our native modules...")
    for modname in ["pedalboard_native", "pedalboard_native.io", "pedalboard_native.utils"]:
        import_stub(".", modname)

    print("Running Sphinx...")
    if args.check:
        missing_files = []
        mismatched_files = []
        with TemporaryDirectory() as tempdir:
            sphinx_build_main(["-b", "html", args.docs_input_dir, tempdir, "-v", "-v", "-v"])
            postprocess_sphinx_output(tempdir, SPHINX_REPLACEMENTS)
            remove_non_public_files(tempdir)

            for dirpath, _dirnames, filenames in os.walk(tempdir):
                prefix = dirpath.replace(tempdir, "").lstrip(os.path.sep)
                for filename in filenames:
                    if glob_matches(filename, args.skip_comparing):
                        print(f"Skipping comparison of file: {filename}")
                        continue
                    expected_path = os.path.join(tempdir, prefix, filename)
                    actual_path = os.path.join(args.docs_output_dir, prefix, filename)
                    if not os.path.isfile(actual_path):
                        missing_files.append(os.path.join(prefix, filename))
                    else:
                        with open(expected_path, "rb") as e, open(actual_path, "rb") as a:
                            if e.read() != a.read():
                                mismatched_files.append(os.path.join(prefix, filename))
            if missing_files or mismatched_files:
                error_lines = []
                if missing_files:
                    error_lines.append(
                        f"{len(missing_files):,} file(s) were expected in {args.docs_output_dir},"
                        " but not found:"
                    )
                    for missing_file in missing_files:
                        error_lines.append(f"\t{missing_file}")
                if mismatched_files:
                    error_lines.append(
                        f"{len(mismatched_files):,} file(s) in {args.docs_output_dir} did not match"
                        " expected values:"
                    )
                    for mismatched_file in mismatched_files:
                        expected_path = os.path.join(tempdir, mismatched_file)
                        actual_path = os.path.join(args.docs_output_dir, mismatched_file)
                        try:
                            with open(expected_path) as e, open(actual_path) as a:
                                diff = difflib.context_diff(
                                    e.readlines(),
                                    a.readlines(),
                                    os.path.join("expected", mismatched_file),
                                    os.path.join("actual", mismatched_file),
                                )
                            error_lines.append("\n".join([trim_diff_line(x) for x in diff]))
                        except UnicodeDecodeError:
                            error_lines.append(
                                f"Binary file {mismatched_file} does not match expected contents."
                            )
                raise ValueError("\n".join(error_lines))
        print("Done! Generated type stubs and documentation are valid.")
    else:
        sphinx_build_main(["-b", "html", args.docs_input_dir, args.docs_output_dir])
        postprocess_sphinx_output(args.docs_output_dir, SPHINX_REPLACEMENTS)
        remove_non_public_files(args.docs_output_dir)
        print(f"Done! Commit the contents of `{args.docs_output_dir}` to Git.")


if __name__ == "__main__":
    main()
