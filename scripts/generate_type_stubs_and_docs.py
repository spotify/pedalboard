import os
import sys
import argparse
import inspect
import typing
import shutil
import importlib
from tempfile import TemporaryDirectory
from contextlib import contextmanager

from pybind11_stubgen import ClassStubsGenerator, StubsGenerator
from pybind11_stubgen import main as pybind11_stubgen_main
from .postprocess_type_hints import main as postprocess_type_hints_main
from mypy.stubtest import test_stubs, parse_options as mypy_parse_options
from sphinx.ext.apidoc import main as sphinx_apidoc_main
import sphinx.ext.autodoc.importer
from sphinx.cmd.build import main as sphinx_build_main


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
            return []

        def parse(self):
            self.doc_string = self.klass.__doc__ or ""
            self.doc_string = self.doc_string.split("Members:")[0]
            for name, (value_object, docstring) in getattr(self.klass, "__entries").items():
                self.enum_names.append(name)
                self.enum_values.append(value_object.value)
                self.enum_docstrings.append(docstring)

        def to_lines(self):  # type: () -> List[str]
            result = [
                "class {class_name}(Enum):{doc_string}".format(
                    class_name=self.klass.__name__,
                    doc_string="\n" + self.format_docstring(self.doc_string)
                    if self.doc_string
                    else "",
                ),
            ]
            for (name, value, docstring) in sorted(
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


def patch_sphinx(module_names_to_combine: typing.Set[str]):
    """
    Sphinx doesn't know how to read .pyi files, but we use .pyi files as our
    "source of truth" for the public API that we expose to IDEs and our documentation.
    This patch tells Sphinx how to read .pyi files, and goes one step further to
    combine both our "fake" module from our .pyi files and any real Python code
    in our regular modules.
    """
    old_import_module = sphinx.ext.autodoc.importer.import_module

    def patch_import_module(modname: str, *args, **kwargs) -> typing.Any:
        if modname in sys.modules:
            return sys.modules[modname]
        if modname in module_names_to_combine:
            try:
                pyi_module = import_stub(".", modname)
            except Exception as e:
                print(f"Failed to import stub module: {e}")
                raise
            # Remove the stub module from the module cache:
            temp_module_holding_area = {}
            for module_name in list(sys.modules.keys()):
                if module_name.split(".")[0] in module_names_to_combine:
                    temp_module_holding_area[module_name] = sys.modules[module_name]
                    del sys.modules[module_name]
            try:
                regular_module = importlib.import_module(modname, None)
            except Exception as e:
                print(f"Failed to import regular module: {e}")
                raise
            for x in dir(regular_module):
                if (
                    not hasattr(pyi_module, x)
                    and not inspect.ismodule(getattr(regular_module, x))
                    and not x.startswith("_")
                ):
                    setattr(pyi_module, x, getattr(regular_module, x))
                    getattr(pyi_module, "__all__").append(x)
            sys.modules.update(temp_module_holding_area)

            # The sort order of __all__ is used by Sphinx:
            getattr(pyi_module, "__all__").sort()

            return pyi_module
        return old_import_module(modname, *args, **kwargs)

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


def main():
    parser = argparse.ArgumentParser(
        description="Generate type stub files (.pyi) and Sphinx documentation for Pedalboard."
    )
    parser.add_argument(
        "--docs-output-dir", default="docs", help="Output directory for documentation HTML files."
    )
    parser.add_argument(
        "--docs-input-dir", default="docs/source", help="Input directory for Sphinx."
    )
    parser.add_argument(
        "--clean", action="store_true", help="If set, clear cache directories in between builds."
    )
    parser.add_argument(
        "--skip-regenerating-type-hints",
        action="store_true",
        help="If set, don't bother regenerating or reprocessing type hint files.",
    )
    args = parser.parse_args()

    patch_pybind11_stubgen()
    patch_sphinx(module_names_to_combine={"pedalboard", "pedalboard.io"})

    if not args.skip_regenerating_type_hints:
        with TemporaryDirectory() as tempdir, isolated_imports({"pedalboard", "pedalboard.io"}):
            # Generate .pyi stubs files from Pedalboard's native (Pybind11) source.
            # Documentation will be copied from the Pybind11 docstrings, and these stubs
            # files will be used as the "source of truth" for both IDE autocompletion
            # and Sphinx documentation.
            print("Generating type stubs from native code...")
            pybind11_stubgen_main(["-o", tempdir, "pedalboard_native", "--no-setup-py"])

            # Post-process the type hints generaetd by pybind11_stubgen; we can't patch
            # everything easily, so we do string manipulation after-the-fact and run ``black``.
            print("Postprocessing generated type hints...")
            postprocess_type_hints_main([tempdir, "pedalboard"])

            # Run mypy.stubtest to ensure that the results are correct and nothing got missed:
            print("Running `mypy.stubtest` to validate stubs match...")
            mypy_stubtest_args = mypy_parse_options(
                [
                    "pedalboard",
                    "--allowlist",
                    "stubtest.allowlist",
                    "--ignore-missing-stub",
                    "--ignore-unused-allowlist",
                ]
            )
            test_stubs(mypy_stubtest_args)

    if args.clean:
        for filename in os.listdir(args.docs_input_dir):
            if not filename.startswith(os.path.split(args.docs_output_dir)[-1]):
                print(f"Clearing docs output file ({filename})...")
            # shutil.rmtree(docs_output_dir, ignore_errors=True)

    # Why is this necessary? I don't know, but without it, things fail.
    print(f"Importing numpy to ensure a successful Pedalboard import...")
    import numpy

    print(f"Running Sphinx...")
    sphinx_build_main(["-b", "html", os.path.join("docs", "source"), os.path.join("docs")])

    print("Done!")


if __name__ == "__main__":
    main()
