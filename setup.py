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


import os
import platform
from subprocess import check_output
from pybind11.setup_helpers import Pybind11Extension
from pathlib import Path
from distutils.core import setup
from distutils.unixccompiler import UnixCCompiler

DEBUG = bool(int(os.environ.get('DEBUG', 0)))

JUCE_CPPFLAGS = [
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
    "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1",
    "-DJUCE_STRICT_REFCOUNTEDPOINTER=1",
    "-DJUCE_STANDALONE_APPLICATION=1",
    "-DJUCER_LINUX_MAKE_6D53C8B4=1",
    "-DJUCE_APP_VERSION=1.0.0",
    "-DJUCE_APP_VERSION_HEX=0x10000",
    '-Wall',
]

if platform.system() == "Darwin":
    JUCE_CPPFLAGS.append("-DMACOS=1")
elif platform.system() == "Linux":
    JUCE_CPPFLAGS.append("-DLINUX=1")
elif platform.system() == "Windows":
    JUCE_CPPFLAGS.append("-DWINDOWS=1")
else:
    raise NotImplementedError(
        "Not sure how to build JUCE on platform: {}!".format(platform.system())
    )


JUCE_CPPFLAGS_CONSOLEAPP = [
    "-DJucePlugin_Build_VST=0",
    "-DJucePlugin_Build_VST3=0",
    "-DJucePlugin_Build_AU=0",
    "-DJucePlugin_Build_AUv3=0",
    "-DJucePlugin_Build_RTAS=0",
    "-DJucePlugin_Build_AAX=0",
    "-DJucePlugin_Build_Standalone=0",
    "-DJucePlugin_Build_Unity=0",
    # "-DJUCE_PLUGINHOST_VST=1", # Include for VST2 support, not licensed by Steinberg
    "-DJUCE_PLUGINHOST_VST3=1",
    # "-DJUCE_PLUGINHOST_LADSPA=1", # Include for LADSPA plugin support, Linux only.
    "-DJUCE_DISABLE_JUCE_VERSION_PRINTING=1",
    "-DJUCE_WEB_BROWSER=0",
    "-DJUCE_USE_CURL=0",
    # "-DJUCE_USE_FREETYPE=0",
]

LINK_ARGS = []

JUCE_INCLUDES = ['JUCE/modules/', 'JUCE/modules/juce_audio_processors/format_types/VST3_SDK/']

if DEBUG:
    JUCE_CPPFLAGS += ["-DDEBUG=1", "-D_DEBUG=1"]
    JUCE_CPPFLAGS += ['-O0', '-g']
    if bool(int(os.environ.get('USE_ASAN', 0))):
        JUCE_CPPFLAGS += ['-fsanitize=address', '-fno-omit-frame-pointer']
        LINK_ARGS += ['-fsanitize=address']
        if platform.system() == "Linux":
            LINK_ARGS += ['-shared-libasan']
    elif bool(int(os.environ.get('USE_TSAN', 0))):
        JUCE_CPPFLAGS += ['-fsanitize=thread']
        LINK_ARGS += ['-fsanitize=thread']
    elif bool(int(os.environ.get('USE_MSAN', 0))):
        JUCE_CPPFLAGS += ['-fsanitize=memory', '-fsanitize-memory-track-origins']
        LINK_ARGS += ['-fsanitize=memory']
else:
    JUCE_CPPFLAGS += ['/Ox' if platform.system() == "Windows" else '-O3']


# Regardless of platform, allow our compiler to compile .mm files as Objective-C (required on MacOS)
UnixCCompiler.src_extensions.append(".mm")
UnixCCompiler.language_map[".mm"] = "objc++"

LIBS = []
if platform.system() == "Darwin":
    MACOS_FRAMEWORKS = [
        'Accelerate',
        'AppKit',
        'AudioToolbox',
        'Cocoa',
        'CoreAudio',
        'CoreAudioKit',
        'CoreMIDI',
        'Foundation',
        'IOKit',
        'QuartzCore',
        'WebKit',
    ]

    # On MacOS, we link against some Objective-C system libraries, so we search
    # for Objective-C++ files instead of C++ files.
    for f in MACOS_FRAMEWORKS:
        LINK_ARGS += ['-framework', f]
    JUCE_CPPFLAGS_CONSOLEAPP += ["-DJUCE_PLUGINHOST_AU=1"]
    JUCE_CPPFLAGS.append('-xobjective-c++')

    sources = list(Path("pedalboard").glob("**/*.cpp"))

    # Replace .cpp sources with matching .mm sources on macOS to force the
    # compiler to use Apple's Objective-C and Objective-C++ code.
    for objc_source in Path("pedalboard").glob("**/*.mm"):
        matching_cpp_source = next(
            iter(
                [
                    cpp_source
                    for cpp_source in sources
                    if os.path.splitext(objc_source.name)[0] == os.path.splitext(cpp_source.name)[0]
                ]
            ),
            None,
        )
        if matching_cpp_source:
            sources[sources.index(matching_cpp_source)] = objc_source
    PEDALBOARD_SOURCES = [str(p.resolve()) for p in sources]
elif platform.system() == "Linux":
    for package in ['freetype2']:
        flags = (
            check_output(['pkg-config', '--cflags-only-I', package])
            .decode('utf-8')
            .strip()
            .split(' ')
        )
        include_paths = [flag[2:] for flag in flags]
        JUCE_INCLUDES += include_paths
    LINK_ARGS += ['-lfreetype']

    PEDALBOARD_SOURCES = [str(p.resolve()) for p in (list(Path("pedalboard").glob("**/*.cpp")))]
elif platform.system() == "Windows":
    JUCE_CPPFLAGS += ['-DJUCE_DLL_BUILD=1']
    # https://forum.juce.com/t/statically-linked-exe-in-win-10-not-working/25574/3
    LIBS.extend(
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
    PEDALBOARD_SOURCES = [str(p.resolve()) for p in (list(Path("pedalboard").glob("**/*.cpp")))]
else:
    raise NotImplementedError(
        "Not sure how to build JUCE on platform: {}!".format(platform.system())
    )

pedalboard_cpp = Pybind11Extension(
    'pedalboard_native',
    sources=PEDALBOARD_SOURCES,
    include_dirs=JUCE_INCLUDES,
    extra_compile_args=JUCE_CPPFLAGS + JUCE_CPPFLAGS_CONSOLEAPP,
    extra_link_args=LINK_ARGS,
    libraries=LIBS,
    language="c++",
    cxx_std=17,
)


if DEBUG:
    # Why does Pybind11 always remove debugging symbols?
    pedalboard_cpp.extra_compile_args.remove('-g0')

# read the contents of the README file
this_directory = Path(__file__).parent
long_description = (this_directory / "README.md").read_text()

# current version
with open(os.path.join("pedalboard", "version.txt")) as f:
    version = f.read().strip()

setup(
    name='pedalboard',
    version=version,
    author='Peter Sobot',
    author_email='psobot@spotify.com',
    description='A Python library for adding effects to audio.',
    long_description=long_description,
    long_description_content_type='text/markdown',
    classifiers=[
        "Development Status :: 4 - Beta",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C++",
        "Programming Language :: Python",
        "Topic :: Multimedia :: Sound/Audio",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
    ],
    ext_modules=[pedalboard_cpp],
    install_requires=['numpy'],
    packages=['pedalboard'],
)
