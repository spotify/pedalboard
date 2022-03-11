/*
 * pedalboard
 * Copyright 2022 Spotify AB
 *
 * Licensed under the GNU Public License, Version 3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.gnu.org/licenses/gpl-3.0.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <mutex>
#include <optional>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "../JuceHeader.h"
#include "AudioFile.h"

#include "ReadableAudioFile.h"
#include "WriteableAudioFile.h"

namespace py = pybind11;

namespace Pedalboard {

inline void init_audio_file(py::module &m) {
  /**
   * Important note: any changes made to the function signatures here should
   * also be made to the constructor signatures of ReadableAudioFile and
   * WriteableAudioFile to keep a consistent interface!
   */
  py::class_<AudioFile, std::shared_ptr<AudioFile>>(
      m, "AudioFile", "A base class for readable and writeable audio files.")
      .def(py::init<>()) // Make this class effectively abstract; we can only
                         // instantiate subclasses via __new__.
      .def_static(
          "__new__",
          [](const py::object *, std::string filename, std::string mode) {
            if (mode == "r") {
              return std::make_shared<ReadableAudioFile>(filename);
            } else if (mode == "w") {
              throw py::type_error("Opening an audio file for writing requires "
                                   "samplerate and num_channels arguments.");
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") or write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("filename"), py::arg("mode") = "r")
      .def_static(
          "__new__",
          [](const py::object *, py::object filelike, std::string mode) {
            if (mode == "r") {
              if (!isReadableFileLike(filelike)) {
                throw py::type_error(
                    "Expected either a filename or a file-like object (with "
                    "read, seek, seekable, and tell methods), but received: " +
                    filelike.attr("__repr__")().cast<std::string>());
              }

              return std::make_shared<ReadableAudioFile>(
                  std::make_unique<PythonInputStream>(filelike));
            } else if (mode == "w") {
              throw py::type_error(
                  "Opening an audio file-like object for writing requires "
                  "samplerate and num_channels arguments.");
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") or write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("file_like"), py::arg("mode") = "r")
      .def_static(
          "__new__",
          [](const py::object *, std::string filename, std::string mode,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::variant<std::string, float>> quality) {
            if (mode == "r") {
              throw py::type_error(
                  "Opening an audio file for reading does not require "
                  "samplerate, num_channels, bit_depth, or quality arguments - "
                  "these parameters "
                  "will be read from the file.");
            } else if (mode == "w") {
              if (!sampleRate) {
                throw py::type_error(
                    "Opening an audio file for writing requires a samplerate "
                    "argument to be provided.");
              }

              return std::make_shared<WriteableAudioFile>(
                  filename, sampleRate.value(), numChannels, bitDepth, quality);
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") or write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("filename"), py::arg("mode") = "w",
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none())
      .def_static(
          "__new__",
          [](const py::object *, py::object filelike, std::string mode,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::variant<std::string, float>> quality,
             std::optional<std::string> format) {
            if (mode == "r") {
              throw py::type_error(
                  "Opening a file-like object for reading does not require "
                  "samplerate, num_channels, bit_depth, or quality arguments - "
                  "these parameters "
                  "will be read from the file-like object.");
            } else if (mode == "w") {
              if (!sampleRate) {
                throw py::type_error("Opening a file-like object for writing "
                                     "requires a samplerate "
                                     "argument to be provided.");
              }

              if (!isWriteableFileLike(filelike)) {
                throw py::type_error(
                    "Expected either a filename or a file-like object (with "
                    "write, seek, seekable, and tell methods), but received: " +
                    filelike.attr("__repr__")().cast<std::string>());
              }

              auto stream = std::make_unique<PythonOutputStream>(filelike);
              if (!format && !stream->getFilename()) {
                throw py::type_error(
                    "Unable to infer audio file format for writing. Expected "
                    "either a \".name\" property on the provided file-like "
                    "object (" +
                    filelike.attr("__repr__")().cast<std::string>() +
                    ") or an explicit file format passed as the \"format=\" "
                    "argument.");
              }

              return std::make_shared<WriteableAudioFile>(
                  format.value_or(""), std::move(stream), sampleRate.value(),
                  numChannels, bitDepth, quality);
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") or write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("file_like"), py::arg("mode") = "w",
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none(),
          py::arg("format") = py::none());
}
} // namespace Pedalboard