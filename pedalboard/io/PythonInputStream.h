/*
 * pedalboard
 * Copyright 2021 Spotify AB
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

namespace py = pybind11;

#include "../JuceHeader.h"
#include "PythonFileLike.h"

namespace Pedalboard {

bool isReadableFileLike(py::object fileLike) {
  return py::hasattr(fileLike, "read") && py::hasattr(fileLike, "seek") &&
         py::hasattr(fileLike, "tell") && py::hasattr(fileLike, "seekable");
}

/**
 * A juce::InputStream subclass that fetches its
 * data from a provided Python file-like object.
 */
class PythonInputStream : public juce::InputStream, public PythonFileLike {
public:
  PythonInputStream(py::object fileLike) : PythonFileLike(fileLike) {
    if (!isReadableFileLike(fileLike)) {
      throw py::type_error("Expected a file-like object (with read, seek, "
                           "seekable, and tell methods).");
    }
  }

  bool isSeekable() noexcept {
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      return fileLike.attr("seekable")().cast<bool>();
    } catch (py::error_already_set e) {
      e.restore();
      return false;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return false;
    }
  }

  juce::int64 getTotalLength() noexcept {
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return -1;

    // TODO: Try reading a couple of Python properties that may contain the
    // total length: urllib3.response.HTTPResponse provides `length_remaining`,
    // for instance

    try {
      if (!fileLike.attr("seekable")().cast<bool>()) {
        return -1;
      }

      if (totalLength == -1) {
        juce::int64 pos = fileLike.attr("tell")().cast<juce::int64>();
        fileLike.attr("seek")(0, 2);
        totalLength = fileLike.attr("tell")().cast<juce::int64>();
        fileLike.attr("seek")(pos, 0);
      }
    } catch (py::error_already_set e) {
      e.restore();
      return -1;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return -1;
    }

    return totalLength;
  }

  int read(void *buffer, int bytesToRead) noexcept {
    // The buffer should never be null, and a negative size is probably a
    // sign that something is broken!
    jassert(buffer != nullptr && bytesToRead >= 0);

    if (PythonException::isPending())
      return 0;

    py::gil_scoped_acquire acquire;
    try {
      auto readResult = fileLike.attr("read")(bytesToRead);

      if (!py::isinstance<py::bytes>(readResult)) {
        std::string message =
            "File-like object passed to AudioFile was expected to return "
            "bytes from its read(...) method, but "
            "returned " +
            py::str(readResult.get_type().attr("__name__"))
                .cast<std::string>() +
            ".";

        if (py::hasattr(fileLike, "mode") &&
            py::str(fileLike.attr("mode")).cast<std::string>() == "r") {
          message += " (Try opening the stream in \"rb\" mode instead of "
                     "\"r\" mode if possible.)";
        }

        throw py::type_error(message);
        return 0;
      }

      py::bytes bytesObject = readResult.cast<py::bytes>();
      char *pythonBuffer = nullptr;
      py::ssize_t pythonLength = 0;

      if (PYBIND11_BYTES_AS_STRING_AND_SIZE(bytesObject.ptr(), &pythonBuffer,
                                            &pythonLength)) {
        throw py::buffer_error(
            "Internal error: failed to read bytes from bytes object!");
      }

      if (!buffer && pythonLength > 0) {
        throw py::buffer_error("Internal error: bytes pointer is null, but a "
                               "non-zero number of bytes were returned!");
      }

      if (buffer && pythonLength) {
        std::memcpy(buffer, pythonBuffer, pythonLength);
      }

      lastReadWasSmallerThanExpected = bytesToRead > pythonLength;
      return pythonLength;
    } catch (py::error_already_set e) {
      e.restore();
      return 0;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return 0;
    }
  }

  bool isExhausted() noexcept {
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return true;

    try {
      if (lastReadWasSmallerThanExpected) {
        return true;
      }

      return fileLike.attr("tell")().cast<juce::int64>() == getTotalLength();
    } catch (py::error_already_set e) {
      e.restore();
      return true;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return true;
    }
  }

  juce::int64 getPosition() noexcept {
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return -1;

    try {
      return fileLike.attr("tell")().cast<juce::int64>();
    } catch (py::error_already_set e) {
      e.restore();
      return -1;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return -1;
    }
  }

  bool setPosition(juce::int64 pos) noexcept {
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      if (fileLike.attr("seekable")().cast<bool>()) {
        fileLike.attr("seek")(pos);
        lastReadWasSmallerThanExpected = false;
      }

      return fileLike.attr("tell")().cast<juce::int64>() == pos;
    } catch (py::error_already_set e) {
      e.restore();
      return false;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return false;
    }
  }

private:
  juce::int64 totalLength = -1;
  bool lastReadWasSmallerThanExpected = false;
};
}; // namespace Pedalboard