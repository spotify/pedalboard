#! /usr/bin/env python
#
# Copyright 2021 Spotify AB
#
# Licensed under the GNU Public License, Version 3.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.gnu.org/licenses/gpl-3.0.html
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import os
import platform
from distutils.core import setup
from distutils.unixccompiler import UnixCCompiler
from pathlib import Path
from subprocess import check_output

from pybind11.setup_helpers import Pybind11Extension, build_ext

DEBUG = bool(int(os.environ.get("DEBUG", 0)))

# C or C++ flags:
BASE_CPP_FLAGS = [
    "-Wall",
]
ALL_INCLUDES = []
ALL_LINK_ARGS = []
ALL_CFLAGS = []
ALL_CPPFLAGS = []
ALL_LIBRARIES = []
ALL_SOURCE_PATHS = []

# Add JUCE-related flags:
ALL_CPPFLAGS.extend(
    [
        "-DJUCE_DISPLAY_SPLASH_SCREEN=1",
        "-DJUCE_USE_DARK_SPLASH_SCREEN=1",
        "-DJUCE_MODULE_AVAILABLE_juce_audio_basics=1",
        "-DJUCE_MODULE_AVAILABLE_juce_audio_formats=1",
        "-DJUCE_MODULE_AVAILABLE_juce_audio_processors=1",
        "-DJUCE_MODULE_AVAILABLE_juce_core=1",
        "-DJUCE_MODULE_AVAILABLE_juce_data_structures=1",
        "-DJUCE_MODULE_AVAILABLE_juce_dsp=1",
        "-DJUCE_MODULE_AVAILABLE_juce_events=1",
        "-DJUCE_MODULE_AVAILABLE_juce_graphics=1",
        "-DJUCE_MODULE_AVAILABLE_juce_gui_basics=1",
        "-DJUCE_MODULE_AVAILABLE_juce_gui_extra=1",
        "-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1",
        "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1",
        "-DJUCE_STRICT_REFCOUNTEDPOINTER=1",
        "-DJUCE_STANDALONE_APPLICATION=1",
        "-DJUCER_LINUX_MAKE_6D53C8B4=1",
        "-DJUCE_APP_VERSION=1.0.0",
        "-DJUCE_APP_VERSION_HEX=0x10000",
        # Consoleapp flags:
        "-DJucePlugin_Build_VST=0",
        "-DJucePlugin_Build_VST3=0",
        "-DJucePlugin_Build_AU=0",
        "-DJucePlugin_Build_AUv3=0",
        "-DJucePlugin_Build_RTAS=0",
        "-DJucePlugin_Build_AAX=0",
        "-DJucePlugin_Build_Standalone=0",
        "-DJucePlugin_Build_Unity=0",
        # "-DJUCE_PLUGINHOST_VST=1", # Include for VST2 support, not licensed by Steinberg
        # "-DJUCE_PLUGINHOST_VST3=1", # Disable the built-in VST3 support, as we include our own.
        # "-DJUCE_PLUGINHOST_LADSPA=1", # Include for LADSPA plugin support, Linux only.
        "-DJUCE_DISABLE_JUCE_VERSION_PRINTING=1",
        "-DJUCE_WEB_BROWSER=0",
        "-DJUCE_USE_CURL=0",
        "-DJUCE_USE_MP3AUDIOFORMAT=0",  # We've patched this out too
        "-DJUCE_USE_FLAC=0",  # We've patched this out
        # "-DJUCE_USE_FREETYPE=0",
        "-DJUCE_MODAL_LOOPS_PERMITTED=1",
    ]
)
ALL_INCLUDES.extend(
    [
        "vendors/pybind11/include/",
        "JUCE/modules/",
        "JUCE/modules/juce_audio_processors/format_types/VST3_SDK/",
    ]
)

if "musllinux" in os.getenv("CIBW_BUILD", ""):
    # For Alpine/musllinux compatibility:
    ALL_CPPFLAGS.extend(
        [
            "-D_NL_IDENTIFICATION_LANGUAGE=0x42",
            "-D_NL_IDENTIFICATION_TERRITORY=0x43",
        ]
    )

# Rubber Band library:
ALL_CPPFLAGS.extend(
    [
        "-DUSE_BQRESAMPLER=1",
        "-D_HAS_STD_BYTE=0",
        "-DNOMINMAX",
        "-DALREADY_CONFIGURED",
    ]
)


def ignore_files_matching(files, *matches):
    matches = set(matches)
    for match in matches:
        new_files = []
        for file in files:
            if match in str(file):
                # print(f"Skipping compilation of: {file}")
                pass
            else:
                new_files.append(file)
        files = new_files
    return files


# Platform-specific FFT speedup flags:
if platform.system() == "Windows" or "musllinux" in os.getenv("CIBW_BUILD", ""):
    ALL_CPPFLAGS.append("-DUSE_BUILTIN_FFT")
    ALL_CPPFLAGS.append("-DNO_THREADING")
elif platform.system() == "Darwin":
    # No need for any threading code on MacOS;
    # vDSP does all of this for us and these code paths are redundant.
    ALL_CPPFLAGS.append("-DNO_THREADING")
elif platform.system() == "Linux":
    # Use FFTW3 for FFTs on Linux, which should speed up Rubberband by 3-4x:
    ALL_CPPFLAGS.extend(
        [
            "-DHAVE_FFTW3=1",
            "-DLACK_SINCOS=1",
            "-DFFTW_DOUBLE_ONLY=1",
            "-DUSE_PTHREADS",
        ]
    )
    ALL_INCLUDES += ["vendors/fftw3/api/", "vendors/fftw3/"]
    fftw_paths = list(Path("vendors/fftw3/").glob("**/*.c"))
    fftw_paths = ignore_files_matching(
        fftw_paths,
        # Don't bother compiling in Altivec or VSX (PowerPC) support;
        # it's 2024, not 2004 (although RIP my G5 cheese grater)
        "altivec",
        "vsx",
        # We're not using FFTW in multi-threaded mode:
        "mpi",
        "threads",
        # No need for tests, tools, or support code:
        "tests",
        "tools",
        "/support",
        "common/",
        "libbench",
        # Ignore SSE, AVX2, AVX128, and AVX512 SIMD code;
        # For Rubber Band's usage, just AVX gives us the
        # largest speedup without bloating the binary
        "sse2",
        "avx2",
        "avx512",
        "kcvi",
        "avx-128-fma",
        "generic-simd",
    )

    # On ARM, ignore the X86-specific SIMD code:
    if "arm" in platform.processor() or "aarch64" in platform.processor():
        fftw_paths = ignore_files_matching(fftw_paths, "avx", "/sse")
        ALL_CFLAGS.append("-DHAVE_NEON=1")
    else:
        # And on x86, ignore the ARM-specific SIMD code (and KCVI; not GCC or Clang compatible).
        fftw_paths = ignore_files_matching(fftw_paths, "neon")
        ALL_CFLAGS.append("-march=native")
        # Enable SIMD instructions:
        ALL_CFLAGS.extend(
            [
                # "-DHAVE_SSE2",
                "-DHAVE_AVX",  # Testing shows this is all we need!
                # "-DHAVE_AVX_128_FMA", # AMD only
                # "-DHAVE_AVX2",
                # "-DHAVE_AVX512", # No measurable speed difference
                # "-DHAVE_GENERIC_SIMD128", # Crashes!
                # "-DHAVE_GENERIC_SIMD256", # Also crashes!
            ]
        )

    ALL_SOURCE_PATHS += fftw_paths

    ALL_CFLAGS.extend(
        [
            "-DHAVE_UINTPTR_T",
            '-DPACKAGE="FFTW"',
            '-DVERSION="0"',
            '-DPACKAGE_VERSION="00000"',
            '-DFFTW_CC="clang"',
            "-includestring.h",
            "-includestdint.h",
            "-includevendors/fftw3/dft/codelet-dft.h",
            "-includevendors/fftw3/rdft/codelet-rdft.h",
            "-DHAVE_INTTYPES_H",
            "-DHAVE_STDINT_H",
            "-DHAVE_STDLIB_H",
            "-DHAVE_STRING_H",
            "-DHAVE_TIME_H",
            "-DHAVE_UNISTD_H",
            "-DHAVE_DECL_DRAND48",
            "-DHAVE_DECL_SRAND48",
            "-DHAVE_DECL_COSL",
            "-DHAVE_DECL_SINL",
            "-DHAVE_DECL_POSIX_MEMALIGN",
            "-DHAVE_DRAND48",
            "-DHAVE_SRAND48",
            "-DHAVE_POSIX_MEMALIGN",
            "-DHAVE_ISNAN",
            "-DHAVE_SNPRINTF",
            "-DHAVE_STRCHR",
            "-DHAVE_SYSCTL",
        ]
    )
    if platform.system() == "Linux":
        ALL_CFLAGS.append("-DHAVE_GETTIMEOFDAY")

ALL_SOURCE_PATHS += list(Path("vendors/rubberband/single").glob("*.cpp"))

ALL_SOURCE_PATHS += list(Path("vendors").glob("*.c"))
ALL_INCLUDES += ["vendors/"]

# LAME/mpglib:
LAME_FLAGS = ["-DHAVE_MPGLIB"]
LAME_CONFIG_FILE = str(Path("vendors/lame_config.h").resolve())
if platform.system() == "Windows":
    LAME_FLAGS.append(f"/FI{LAME_CONFIG_FILE}")
    LAME_FLAGS.append("-DHAVE_XMMINTRIN_H")
else:
    LAME_FLAGS.append(f"-include{LAME_CONFIG_FILE}")
ALL_CFLAGS.extend(LAME_FLAGS)
ALL_SOURCE_PATHS += list(Path("vendors/lame/libmp3lame").glob("*.c"))
ALL_SOURCE_PATHS += list(Path("vendors/lame/libmp3lame/vector").glob("*.c"))
ALL_SOURCE_PATHS += list(Path("vendors/lame/mpglib").glob("*.c"))
ALL_INCLUDES += [
    "vendors/lame/include/",
    "vendors/lame/libmp3lame/",
    "vendors/lame/",
]

# libgsm
ALL_SOURCE_PATHS += [p for p in Path("vendors/libgsm/src").glob("*.c") if "toast" not in p.name]
ALL_INCLUDES += ["vendors/libgsm/inc"]

# like -ffast-math, but without the negative side effects on other code in the same process:
MOSTLY_FAST_MATH = [
    "-fassociative-math",
    "-fno-trapping-math",
    "-fno-signed-zeros",
    "-freciprocal-math",
    "-fno-math-errno",
]

# Add platform-specific flags:
if platform.system() == "Darwin":
    ALL_CPPFLAGS.append("-DMACOS=1")
    ALL_CPPFLAGS.append("-DHAVE_VDSP=1")
    ALL_CPPFLAGS.extend(MOSTLY_FAST_MATH)
    if not DEBUG and not os.getenv("DISABLE_LTO"):
        ALL_CPPFLAGS.append("-flto=thin")
        ALL_LINK_ARGS.append("-flto=thin")
    ALL_LINK_ARGS.append("-fvisibility=hidden")
    ALL_CFLAGS += ["-Wno-comment"]
    ALL_CFLAGS.extend(MOSTLY_FAST_MATH)
elif platform.system() == "Linux":
    ALL_CPPFLAGS.append("-DLINUX=1")
    ALL_CPPFLAGS.extend(MOSTLY_FAST_MATH)
    # We use GCC on Linux, which doesn't take a value for the -flto flag:
    if not DEBUG and not os.getenv("DISABLE_LTO"):
        ALL_CPPFLAGS.append("-flto")
        ALL_LINK_ARGS.append("-flto")
    ALL_LINK_ARGS.append("-fvisibility=hidden")
    ALL_CFLAGS += ["-Wno-comment"]
    ALL_CFLAGS.extend(MOSTLY_FAST_MATH)
elif platform.system() == "Windows":
    ALL_CPPFLAGS.append("-DWINDOWS=1")
else:
    raise NotImplementedError(
        "Not sure how to build JUCE on platform: {}!".format(platform.system())
    )

ALL_CPPFLAGS += [
    "-Rpass-analysis=loop-vectorize",
    "-Rpass=loop-vectorize",
    "-Rpass-missed=loop-vectorize",
    "-fdiagnostics-show-template-tree",
]
if DEBUG:
    ALL_CPPFLAGS += ["-DDEBUG=1", "-D_DEBUG=1"]
    ALL_CPPFLAGS += ["-O3", "-g"]
else:
    ALL_CPPFLAGS += ["/Ox" if platform.system() == "Windows" else "-O3"]

if bool(int(os.environ.get("USE_ASAN", 0))):
    ALL_CPPFLAGS += ["-fsanitize=address", "-fno-omit-frame-pointer"]
    ALL_LINK_ARGS += ["-fsanitize=address"]
    if platform.system() == "Linux":
        ALL_LINK_ARGS += ["-shared-libasan", "-latomic"]
elif bool(int(os.environ.get("USE_TSAN", 0))):
    ALL_CPPFLAGS += ["-fsanitize=thread"]
    ALL_LINK_ARGS += ["-fsanitize=thread"]
elif bool(int(os.environ.get("USE_MSAN", 0))):
    ALL_CPPFLAGS += ["-fsanitize=memory", "-fsanitize-memory-track-origins"]
    ALL_LINK_ARGS += ["-fsanitize=memory"]


# Regardless of platform, allow our compiler to compile .mm files as Objective-C (required on MacOS)
UnixCCompiler.src_extensions.append(".mm")
UnixCCompiler.language_map[".mm"] = "objc++"

# Add all Pedalboard C++ sources:
ALL_SOURCE_PATHS += list(Path("pedalboard").glob("**/*.cpp"))

if platform.system() == "Darwin":
    MACOS_FRAMEWORKS = [
        "Accelerate",
        "AppKit",
        "AudioToolbox",
        "Cocoa",
        "CoreAudio",
        "CoreAudioKit",
        "CoreMIDI",
        "Foundation",
        "IOKit",
        "QuartzCore",
        "WebKit",
    ]

    # On MacOS, we link against some Objective-C system libraries, so we search
    # for Objective-C++ files instead of C++ files.
    for f in MACOS_FRAMEWORKS:
        ALL_LINK_ARGS += ["-framework", f]
    ALL_CPPFLAGS.append("-DJUCE_PLUGINHOST_AU=1")
    ALL_CPPFLAGS.append("-xobjective-c++")

    # Replace .cpp sources with matching .mm sources on macOS to force the
    # compiler to use Apple's Objective-C and Objective-C++ code.
    for objc_source in Path("pedalboard").glob("**/*.mm"):
        matching_cpp_source = next(
            iter(
                [
                    cpp_source
                    for cpp_source in ALL_SOURCE_PATHS
                    if os.path.splitext(objc_source.name)[0] == os.path.splitext(cpp_source.name)[0]
                ]
            ),
            None,
        )
        if matching_cpp_source:
            ALL_SOURCE_PATHS[ALL_SOURCE_PATHS.index(matching_cpp_source)] = objc_source
        else:
            ALL_SOURCE_PATHS.append(objc_source)
    ALL_RESOLVED_SOURCE_PATHS = [str(p.resolve()) for p in ALL_SOURCE_PATHS]
elif platform.system() == "Linux":
    for package in ["freetype2"]:
        flags = (
            check_output(["pkg-config", "--cflags-only-I", package])
            .decode("utf-8")
            .strip()
            .split(" ")
        )
        include_paths = [flag[2:] for flag in flags]
        ALL_INCLUDES += include_paths
    ALL_LINK_ARGS += ["-lfreetype"]
    ALL_LINK_ARGS += ["-lasound"]

    ALL_RESOLVED_SOURCE_PATHS = [str(p.resolve()) for p in ALL_SOURCE_PATHS]
elif platform.system() == "Windows":
    ALL_CPPFLAGS += ["-DJUCE_DLL_BUILD=1"]
    # https://forum.juce.com/t/statically-linked-exe-in-win-10-not-working/25574/3
    ALL_LIBRARIES.extend(
        [
            "kernel32",
            "user32",
            "gdi32",
            "winspool",
            "comdlg32",
            "advapi32",
            "shell32",
            "ole32",
            "oleaut32",
            "uuid",
            "odbc32",
            "odbccp32",
        ]
    )
    ALL_RESOLVED_SOURCE_PATHS = [str(p.resolve()) for p in ALL_SOURCE_PATHS]
else:
    raise NotImplementedError(
        "Not sure how to build JUCE on platform: {}!".format(platform.system())
    )


def patch_compile(original_compile):
    """
    On GCC/Clang, we want to pass different arguments when compiling C files vs C++ files.
    """

    def new_compile(obj, src, ext, cc_args, extra_postargs, *args, **kwargs):
        _cc_args = cc_args

        if ext in (".cpp", ".cxx", ".cc", ".mm"):
            _cc_args = cc_args + ALL_CPPFLAGS
        elif ext in (".c",):
            # We're compiling C code, remove the -std= arg:
            extra_postargs = [arg for arg in extra_postargs if "std=" not in arg]
            _cc_args = cc_args + ALL_CFLAGS

        # Code in JUCE or vendors should not even know we're using Python:
        should_omit_python_header = any(x in src for x in ("JUCE", "/juce_overrides/", "/vendors/"))

        # Remove the Python header from most files; we only need it when compiling
        # This speeds up compile times on CI as most of the objects don't need Python
        # headers at all, and including -I/include/python3.x/Python.h prevents us from
        # re-using the same object file for different Python versions.
        if any("include/python3" in arg for arg in _cc_args) and should_omit_python_header:
            _cc_args = [arg for arg in _cc_args if "include/python3" not in arg]

        return original_compile(obj, src, ext, _cc_args, extra_postargs, *args, **kwargs)

    return new_compile


class BuildC_CxxExtensions(build_ext):
    """
    Add custom logic for injecting different arguments when compiling C vs C++ files.
    """

    def initialize_options(self):
        build_ext.initialize_options(self)
        # If on CI, avoid breaking ccache by using a consistent
        # output directory name regardless of Python version:
        if os.getenv("CI"):
            self.build_temp = "./build/temp"

    def build_extensions(self, *args, **kwargs):
        self.compiler._compile = patch_compile(self.compiler._compile)
        build_ext.build_extensions(self, *args, **kwargs)


if platform.system() == "Windows":
    # The MSVCCompiler extension doesn't support per-file command line arguments,
    # so let's merge all of the flags into one list here.
    BASE_CPP_FLAGS.extend(ALL_CPPFLAGS)
    BASE_CPP_FLAGS.extend(ALL_CFLAGS)


pedalboard_cpp = Pybind11Extension(
    "pedalboard_native",
    sources=ALL_RESOLVED_SOURCE_PATHS,
    include_dirs=ALL_INCLUDES,
    extra_compile_args=BASE_CPP_FLAGS,
    extra_link_args=ALL_LINK_ARGS,
    libraries=ALL_LIBRARIES,
    language="c++",
    cxx_std=17,
    include_pybind11=False,
)


if DEBUG:
    # Why does Pybind11 always remove debugging symbols?
    pedalboard_cpp.extra_compile_args.remove("-g0")

# read the contents of the README file
this_directory = Path(__file__).parent
long_description = (this_directory / "README.md").read_text()

# read the contents of the version.py
version = {}
version_file_contents = (this_directory / "pedalboard" / "version.py").read_text()
exec(version_file_contents, version)

logging.basicConfig(format="%(message)s")

setup(
    name="pedalboard",
    version=version["__version__"],
    author="Peter Sobot",
    author_email="psobot@spotify.com",
    description="A Python library for adding effects to audio.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    classifiers=[
        "Development Status :: 4 - Beta",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C++",
        "Programming Language :: Python",
        "Topic :: Multimedia :: Sound/Audio",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
    ],
    ext_modules=[pedalboard_cpp],
    install_requires=["numpy"],
    packages=["pedalboard", "pedalboard.io", "pedalboard_native"],
    package_data={
        "pedalboard": ["py.typed", "*.pyi", "**/*.pyi"],
        "pedalboard_native": ["py.typed", "*.pyi", "**/*.pyi"],
    },
    cmdclass={"build_ext": BuildC_CxxExtensions},
)
