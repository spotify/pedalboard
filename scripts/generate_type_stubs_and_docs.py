import argparse
import difflib
import importlib
import inspect
import os
import pathlib
import re
import shutil
import subprocess
import sys
import traceback
import typing
from contextlib import contextmanager
from tempfile import TemporaryDirectory
from typing import Dict, List

import mypy.stubtest
import psutil
import sphinx.ext.autodoc.importer
from mypy.stubtest import parse_options as mypy_parse_options
from mypy.stubtest import test_stubs
from pybind11_stubgen import ClassStubsGenerator, StubsGenerator
from pybind11_stubgen import main as pybind11_stubgen_main
from sphinx.cmd.build import main as sphinx_build_main

from scripts.postprocess_type_hints import main as postprocess_type_hints_main

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
