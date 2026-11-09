"""Temporary controlled test of the Windows MSVC runtime selected by Qt."""
import argparse
import ctypes
import faulthandler
import json
import ntpath
import os
import sys
from pathlib import Path


def runtime_paths(memory_maps):
    return sorted({
        mapping.path for mapping in memory_maps
        if ntpath.basename(mapping.path).lower().startswith(("msvcp", "vcruntime"))
        and mapping.path.lower().endswith(".dll")
    })


def system_runtime_path(windows_directory):
    return ntpath.join(windows_directory, "System32", "msvcp140.dll")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("qt_first", "system_runtime_first"))
    parser.add_argument("flac", type=Path)
    args = parser.parse_args()
    faulthandler.enable()
    if args.mode == "system_runtime_first":
        # Keep the handle alive. Change only runtime selection, preserving Qt-first imports.
        runtime = ctypes.WinDLL(system_runtime_path(os.environ["SystemRoot"]))
        print("Preloaded system runtime:", runtime._name, flush=True)
    from PyQt6.QtCore import QCoreApplication, pyqtSignal, QObject
    from pedalboard.io import AudioFile
    import pefile
    import psutil

    for path in runtime_paths(psutil.Process().memory_maps()):
        with pefile.PE(path) as image:
            info = image.VS_FIXEDFILEINFO[0]
            version = [info.FileVersionMS >> 16, info.FileVersionMS & 65535,
                       info.FileVersionLS >> 16, info.FileVersionLS & 65535]
        print(json.dumps({"runtime": path, "version": version}), flush=True)
    with pefile.PE(sys.modules["pedalboard_native"].__file__) as image:
        imports = [entry.name.decode() for library in image.DIRECTORY_ENTRY_IMPORT
                   for entry in library.imports if entry.name and entry.name.startswith(b"_Mtx_")]
        print(json.dumps({"native_mutex_imports": sorted(imports)}), flush=True)
    print("Opening audio", flush=True)
    with AudioFile(str(args.flac.resolve()), "r") as audio:
        samples = audio.read(44100)
        print(json.dumps({"shape": list(samples.shape), "samplerate": audio.samplerate}), flush=True)
    print("Audio closed", flush=True)


if __name__ == "__main__":
    main()
