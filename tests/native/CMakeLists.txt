cmake_minimum_required(VERSION 3.19)
# JUCE 6 uses APIs unavailable when targeting macOS 15 and newer.
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target")
endif()
project(external_plugin_buffer_regressions LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python COMPONENTS Interpreter Development.Embed REQUIRED)
execute_process(COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
  OUTPUT_VARIABLE PYBIND11_CMAKE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
find_package(pybind11 CONFIG REQUIRED PATHS "${PYBIND11_CMAKE_DIR}")
set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../JUCE" CACHE PATH "Pinned JUCE source directory")
if(NOT EXISTS "${JUCE_DIR}/modules/juce_core/juce_core.cpp")
  message(FATAL_ERROR "Initialize the JUCE submodule or pass -DJUCE_DIR=/path/to/JUCE")
endif()

add_executable(external_plugin_buffers external_plugin_buffers.cpp)
foreach(module core events data_structures graphics gui_basics gui_extra audio_basics audio_processors)
  target_sources(external_plugin_buffers PRIVATE "${JUCE_DIR}/modules/juce_${module}/juce_${module}.cpp")
endforeach()
target_include_directories(external_plugin_buffers PRIVATE "${JUCE_DIR}/modules" "${CMAKE_CURRENT_LIST_DIR}/../../pedalboard")
target_compile_definitions(external_plugin_buffers PRIVATE
  JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
  JUCE_MODULE_AVAILABLE_juce_gui_basics=1
  JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_VST=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0 JUCE_USE_XCURSOR=0)
target_link_libraries(external_plugin_buffers PRIVATE pybind11::embed)
if(APPLE)
  target_compile_options(external_plugin_buffers PRIVATE -x objective-c++)
  target_link_libraries(external_plugin_buffers PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreAudio"
    "-framework CoreMIDI" "-framework AudioToolbox" "-framework Accelerate"
    "-framework QuartzCore")
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
  find_package(X11 REQUIRED)
  target_link_libraries(external_plugin_buffers PRIVATE PkgConfig::FREETYPE X11::X11 pthread dl rt)
elseif(WIN32)
  target_link_libraries(external_plugin_buffers PRIVATE winmm ole32 oleaut32 uuid comdlg32 imm32 shlwapi version)
endif()
