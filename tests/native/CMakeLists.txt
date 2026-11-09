cmake_minimum_required(VERSION 3.19)
project(audio_stream_regressions LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Python COMPONENTS Interpreter Development.Embed REQUIRED)
execute_process(COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
                OUTPUT_VARIABLE PYBIND11_CMAKE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
find_package(pybind11 CONFIG REQUIRED PATHS "${PYBIND11_CMAKE_DIR}")
if(NOT DEFINED JUCE_DIR)
  set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../JUCE")
endif()
add_executable(audio_stream_harness audio_stream_harness.cpp
  "${JUCE_DIR}/modules/juce_core/juce_core.cpp"
  "${JUCE_DIR}/modules/juce_events/juce_events.cpp"
  "${JUCE_DIR}/modules/juce_audio_basics/juce_audio_basics.cpp"
  "${JUCE_DIR}/modules/juce_audio_devices/juce_audio_devices.cpp")
target_include_directories(audio_stream_harness PRIVATE "${JUCE_DIR}/modules")
target_compile_definitions(audio_stream_harness PRIVATE
  JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
  JUCE_MODULE_AVAILABLE_juce_audio_devices=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
  JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0
  JUCE_USE_XSHM=0 JUCE_USE_XCURSOR=0)
target_link_libraries(audio_stream_harness PRIVATE pybind11::embed)
if(APPLE)
  target_compile_options(audio_stream_harness PRIVATE -x objective-c++)
  target_link_libraries(audio_stream_harness PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreAudio"
    "-framework CoreMIDI" "-framework AudioToolbox" "-framework Accelerate")
elseif(UNIX)
  target_link_libraries(audio_stream_harness PRIVATE pthread dl rt)
elseif(WIN32)
  target_link_libraries(audio_stream_harness PRIVATE winmm ole32 uuid)
endif()
