cmake_minimum_required(VERSION 3.15...3.27)

# Find Python and pybind11 first, as we need the interpreter
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Set up ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# Extract version from pedalboard/version.py
execute_process(
    COMMAND ${CMAKE_COMMAND} -E echo "with open('pedalboard/version.py') as f: exec(f.read()); print(MAJOR, MINOR, PATCH, sep='.')"
    COMMAND ${Python_EXECUTABLE} -
    OUTPUT_VARIABLE PEDALBOARD_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(pedalboard 
    VERSION ${PEDALBOARD_VERSION}
    LANGUAGES CXX C
    DESCRIPTION "A Python library for adding effects to audio."
)

# Global flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Debug/Release configuration
option(DEBUG "Build with debug symbols" OFF)

if(DEBUG)
    add_compile_definitions(DEBUG=1 _DEBUG=1)
    add_compile_options(-O0 -g)
else()
    if(MSVC)
        add_compile_options(/Ox)
    else()
        add_compile_options(-O3)
    endif()
endif()

# Add flags based on compiler/platform
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall)
endif()

# Set up includes and library sources
set(ALL_INCLUDES
    "vendors/pybind11/include/"
    "JUCE/modules/"
    "JUCE/modules/juce_audio_processors/format_types/VST3_SDK/"
    "JUCE/modules/juce_audio_formats/codecs/"
    "vendors/"
    "vendors/libgsm/inc/"
    "vendors/lame/include/"
    "vendors/lame/libmp3lame/"
    "vendors/lame/"
)

# Add any paths containing subfolders that need to be included
file(GLOB GSM_INCLUDE_DIRS "vendors/libgsm/inc")
list(APPEND ALL_INCLUDES ${GSM_INCLUDE_DIRS})

# Platform-specific settings
if(APPLE)
    # macOS specific flags
    add_compile_definitions(MACOS=1 HAVE_VDSP=1)

    # MacOS Frameworks
    find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(COREAUDIOKIT_FRAMEWORK CoreAudioKit REQUIRED)
    find_library(COREMIDI_FRAMEWORK CoreMIDI REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
    find_library(WEBKIT_FRAMEWORK WebKit REQUIRED)

    set(MACOS_FRAMEWORKS
        ${ACCELERATE_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${COREAUDIOKIT_FRAMEWORK}
        ${COREMIDI_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${QUARTZ_FRAMEWORK}
        ${WEBKIT_FRAMEWORK}
    )

    add_compile_definitions(JUCE_PLUGINHOST_AU=1)
    
    # No threading code needed on macOS, vDSP does this for us
    add_compile_definitions(NO_THREADING)

    # File extension handling for Objective-C++
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".mm")
elseif(WIN32)
    # Windows specific flags
    add_compile_definitions(WINDOWS=1 USE_BUILTIN_FFT NO_THREADING)
    add_compile_definitions(JUCE_DLL_BUILD=1)
    
    # Windows-specific libraries
    list(APPEND PLATFORM_LIBRARIES
        kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 
        ole32 oleaut32 uuid odbc32 odbccp32
    )
elseif(UNIX AND NOT APPLE)
    # Linux specific flags
    add_compile_definitions(LINUX=1)
    
    # FFTW support for Linux
    add_compile_definitions(
        HAVE_FFTW3=1
        LACK_SINCOS=1 
        FFTW_DOUBLE_ONLY=1
        USE_PTHREADS
    )
    
    # Linux Dependencies
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    find_package(ALSA REQUIRED)
    
    include_directories(${FREETYPE_INCLUDE_DIRS})
    list(APPEND PLATFORM_LIBRARIES ${FREETYPE_LIBRARIES} ${ALSA_LIBRARIES})
    
    # Processor-specific optimizations (x86 vs ARM)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        add_compile_definitions(HAVE_NEON=1)
    else()
        add_compile_options(-march=native)
        add_compile_definitions(HAVE_AVX)
    endif()
    
    # Additional Linux flags
    list(APPEND ALL_INCLUDES "vendors/fftw3/api/" "vendors/fftw3/")
endif()

# JUCE-related flags
list(APPEND JUCE_COMPILE_DEFINITIONS
    JUCE_DISPLAY_SPLASH_SCREEN=1
    JUCE_USE_DARK_SPLASH_SCREEN=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_STANDALONE_APPLICATION=1
    JUCER_LINUX_MAKE_6D53C8B4=1
    JUCE_APP_VERSION=1.0.0
    JUCE_APP_VERSION_HEX=0x10000
    JucePlugin_Build_VST=0
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
    JucePlugin_Build_AUv3=0
    JucePlugin_Build_RTAS=0
    JucePlugin_Build_AAX=0
    JucePlugin_Build_Standalone=0
    JucePlugin_Build_Unity=0
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_FLAC=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

if(DEFINED ENV{CIBW_BUILD} AND "$ENV{CIBW_BUILD}" MATCHES "musllinux")
    # For Alpine/musllinux compatibility:
    add_compile_definitions(
        _NL_IDENTIFICATION_LANGUAGE=0x42
        _NL_IDENTIFICATION_TERRITORY=0x43
    )
endif()

# Rubber Band library flags
list(APPEND RUBBERBAND_COMPILE_DEFINITIONS
    USE_BQRESAMPLER=1
    _HAS_STD_BYTE=0
    NOMINMAX
    ALREADY_CONFIGURED
)

# Helper function to collect source files
function(collect_sources)
    # Collect all C++ source files
    file(GLOB_RECURSE CPP_SOURCES "pedalboard/*.cpp")
    
    # Handle platform-specific sources
    if(APPLE)
        file(GLOB_RECURSE OBJC_SOURCES "pedalboard/*.mm")
        # Exclude specific files that shouldn't be compiled directly
        list(FILTER OBJC_SOURCES EXCLUDE REGEX ".*juce_mac_PatchedWindowing\\.mm$")
        
        foreach(OBJC_SOURCE ${OBJC_SOURCES})
            get_filename_component(BASENAME ${OBJC_SOURCE} NAME_WE)
            list(FILTER CPP_SOURCES EXCLUDE REGEX ".*/${BASENAME}\\.cpp$")
        endforeach()
        list(APPEND CPP_SOURCES ${OBJC_SOURCES})
    endif()

    # We explicitly patch include_juce_gui_basics.{mm,cpp} with juce_patched_gui_basics.cpp,
    # so remove the latter from the list of C++ sources
    list(FILTER CPP_SOURCES EXCLUDE REGEX ".*juce_patched_gui_basics\\.cpp$")
    
    # Collect RubberBand sources
    file(GLOB RUBBERBAND_SOURCES "vendors/rubberband/single/*.cpp")
    
    # Collect general vendor sources
    file(GLOB VENDOR_SOURCES "vendors/*.c")
    
    # LAME/mpglib sources
    file(GLOB LAME_SOURCES "vendors/lame/libmp3lame/*.c" "vendors/lame/libmp3lame/vector/*.c" "vendors/lame/mpglib/*.c")
    
    # libgsm sources (excluding toast files)
    file(GLOB GSM_SOURCES "vendors/libgsm/src/*.c")
    list(FILTER GSM_SOURCES EXCLUDE REGEX ".*toast.*")
    
    # FFTW sources for Linux
    if(UNIX AND NOT APPLE AND NOT DEFINED CIBW_BUILD OR NOT CIBW_BUILD MATCHES "musllinux")
        file(GLOB_RECURSE FFTW_SOURCES "vendors/fftw3/*.c")
        
        # Exclude files depending on architecture
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
            list(FILTER FFTW_SOURCES EXCLUDE REGEX ".*(avx|/sse).*")
        else()
            list(FILTER FFTW_SOURCES EXCLUDE REGEX ".*neon.*")
        endif()
        
        # Exclude unused FFTW components
        list(FILTER FFTW_SOURCES EXCLUDE REGEX ".*(altivec|vsx|mpi|threads|tests|tools|/support|common/|libbench|sse2|avx2|avx512|kcvi|avx-128-fma|generic-simd).*")
    endif()
    
    # Combine all sources
    set(ALL_SOURCES 
        ${CPP_SOURCES} 
        ${RUBBERBAND_SOURCES} 
        ${VENDOR_SOURCES} 
        ${LAME_SOURCES} 
        ${GSM_SOURCES}
        ${FFTW_SOURCES}
        PARENT_SCOPE
    )
endfunction()

# Call the function to collect all source files
collect_sources()

# Create the Python module
pybind11_add_module(pedalboard_native ${ALL_SOURCES})

# Add include directories
target_include_directories(pedalboard_native PRIVATE ${ALL_INCLUDES})

if(APPLE)
    # Objective-C++ support for macOS, but only when compiling C++, not C:
    target_compile_options(pedalboard_native PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-xobjective-c++>)
endif()

# Add JUCE and other compile definitions
target_compile_definitions(pedalboard_native PRIVATE ${JUCE_COMPILE_DEFINITIONS})
target_compile_definitions(pedalboard_native PRIVATE ${RUBBERBAND_COMPILE_DEFINITIONS})

# LAME config
if(WIN32)
    target_compile_definitions(pedalboard_native PRIVATE 
        HAVE_MPGLIB 
        HAVE_XMMINTRIN_H
    )
    target_compile_options(pedalboard_native PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/vendors/lame_config.h"
    )
else()
    target_compile_definitions(pedalboard_native PRIVATE 
        HAVE_MPGLIB
    )
    target_compile_options(pedalboard_native PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/vendors/lame_config.h"
    )
endif()

# Link libraries
if(APPLE)
    target_link_libraries(pedalboard_native PRIVATE ${MACOS_FRAMEWORKS})
elseif(WIN32)
    target_link_libraries(pedalboard_native PRIVATE ${PLATFORM_LIBRARIES})
elseif(UNIX AND NOT APPLE)
    target_link_libraries(pedalboard_native PRIVATE ${PLATFORM_LIBRARIES})

    # Add missing FFTW-specific C flags for Linux
    target_compile_options(pedalboard_native PRIVATE
        $<$<COMPILE_LANGUAGE:C>:
        -DHAVE_UINTPTR_T
        -DPACKAGE="FFTW"
        -DVERSION="0"
        -DPACKAGE_VERSION="00000"
        -DFFTW_CC="clang"
        "-includestring.h"
        "-includestdint.h"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/vendors/fftw3/dft/codelet-dft.h"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/vendors/fftw3/rdft/codelet-rdft.h"
        "-includesys/time.h"
        -DHAVE_INTTYPES_H
        -DHAVE_STDINT_H
        -DHAVE_STDLIB_H
        -DHAVE_STRING_H
        -DHAVE_TIME_H
        -DHAVE_UNISTD_H
        -DHAVE_DECL_DRAND48
        -DHAVE_DECL_SRAND48
        -DHAVE_DECL_COSL
        -DHAVE_DECL_SINL
        -DHAVE_DECL_POSIX_MEMALIGN
        -DHAVE_DRAND48
        -DHAVE_SRAND48
        -DHAVE_POSIX_MEMALIGN
        -DHAVE_ISNAN
        -DHAVE_SNPRINTF
        -DHAVE_STRCHR
        -DHAVE_SYSCTL
        -DHAVE_GETTIMEOFDAY>
    )
endif()

# Add LTO if not debugging and not explicitly disabled
if(NOT DEBUG AND NOT DEFINED ENV{DISABLE_LTO})
    include(CheckIPOSupported)
    check_ipo_supported(RESULT HAVE_IPO)
    if(HAVE_IPO)
        set_property(TARGET pedalboard_native PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Install targets
install(TARGETS pedalboard_native DESTINATION .) 