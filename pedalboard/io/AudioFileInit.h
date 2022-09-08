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

// For pybind11-stubgen to properly parse the docstrings,
// we have to delcare all of the AudioFile subclasses first before using
// their types (i.e.: as return types from `__new__`).
// See:
// https://pybind11.readthedocs.io/en/latest/advanced/misc.html#avoiding-cpp-types-in-docstrings
inline py::class_<AudioFile, std::shared_ptr<AudioFile>>
declare_audio_file(py::module &m) {
  return py::class_<AudioFile, std::shared_ptr<AudioFile>>(
      m, "AudioFile",
      R"(A base class for readable and writeable audio files.

:class:`AudioFile` may be used just like a regular Python ``open``
function call, to open an audio file for reading (with the default ``"r"`` mode)
or for writing (with the ``"w"`` mode).

Unlike a typical ``open`` call:
 - :class:`AudioFile` objects can only be created in read (``"r"``) or write (``"w"``) mode.
   All audio files are binary (so a trailing ``b`` would be redundant) and appending to an
   existing audio file is not possible.
 - If opening an audio file in write mode (``"w"``), one additional argument is required:
   the sample rate of the file.
 - A file-like object can be provided to :class:`AudioFile`, allowing for reading and
   writing to in-memory streams or buffers. The provided file-like object must be seekable
   and must be opened in binary mode (i.e.: ``io.BinaryIO`` instead of ``io.StringIO``, 
   if using the `io` package).


Examples
--------

Opening an audio file on disk::

   with AudioFile("my_file.mp3") as f:
       first_ten_seconds = f.read(int(f.samplerate * 10))


Opening a file-like object::

   ogg_buffer: io.BytesIO = get_audio_buffer(...)
   with AudioFile(ogg_buffer) as f:
       first_ten_seconds = f.read(int(f.samplerate * 10))


Opening an audio file on disk, while resampling on-the-fly::

    with AudioFile("my_file.mp3").resampled_to(22_050) as f:
       first_ten_seconds = f.read(int(f.samplerate * 10))


Writing an audio file on disk::

   with AudioFile("white_noise.wav", "w", samplerate=44100, num_channels=2) as f:
       f.write(np.random.rand(2, 44100))


Writing encoded audio to a file-like object::

   wav_buffer = io.BytesIO()
   with AudioFile(wav_buffer, "w", samplerate=44100, num_channels=2) as f:
       f.write(np.random.rand(2, 44100))
   wav_buffer.getvalue()  # do something with the file-like object


Writing to an audio file while also specifying quality options for the codec::

   with AudioFile(
       "white_noise.mp3",
       "w",
       samplerate=44100,
       num_channels=2,
       quality=160,  # kilobits per second
   ) as f:
       f.write(np.random.rand(2, 44100))


Re-encoding a WAV file as an MP3 in four lines of Python::

   with AudioFile("input.wav") as i:
       with AudioFile("output.mp3", "w", i.samplerate, i.num_channels) as o:
           while i.tell() < i.frames:
               o.write(i.read(1024))

)");
}

inline void init_audio_file(
    py::class_<AudioFile, std::shared_ptr<AudioFile>> &pyAudioFile) {
  /**
   * Important note: any changes made to the function signatures here should
   * also be made to the constructor signatures of ReadableAudioFile and
   * WriteableAudioFile to keep a consistent interface!
   */

  pyAudioFile
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
          py::arg("cls"), py::arg("filename"), py::arg("mode") = "r",
          "Open an audio file for reading.")
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
          py::arg("cls"), py::arg("file_like"), py::arg("mode") = "r",
          "Open a file-like object for reading. The provided object must have "
          "``read``, ``seek``, ``tell``, and ``seekable`` methods, and must "
          "return binary data (i.e.: ``open(..., \"w\")`` or ``io.BinaryIO``, "
          "etc.).")
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
                  filename, *sampleRate, numChannels, bitDepth, quality);
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
                  format.value_or(""), std::move(stream), *sampleRate,
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