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

std::optional<py::buffer> tryConvertingToBuffer(py::object bufferLike) {
  try {
    py::buffer b(bufferLike);
    return {b};
  } catch (std::exception &e) {
    // If not already a buffer, can we call getbuffer() on it and do we then get
    // a buffer?
    if (py::hasattr(bufferLike, "getbuffer")) {
      py::object getBufferResult = bufferLike.attr("getbuffer")();
      try {
        py::buffer b(getBufferResult);
        return {b};
      } catch (std::exception &e) {
      }
    }

    return {};
  }
}

/**
 * A juce::InputStream subclass that fetches its
 * data from a provided Python file-like object.
 */
class PythonInputStream : public juce::InputStream, public PythonFileLike {
public:
  PythonInputStream(py::object fileLike) : PythonFileLike(fileLike) {}
  virtual ~PythonInputStream() {}

  juce::int64 getTotalLength() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
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

  int read(void *buffer, int bytesToRead) noexcept override {
    // The buffer should never be null, and a negative size is probably a
    // sign that something is broken!
    jassert(buffer != nullptr && bytesToRead >= 0);
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;

    py::gil_scoped_acquire acquire;
    if (PythonException::isPending())
      return 0;

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

  bool isExhausted() noexcept override {
    // Read this up front to avoid releasing the object lock recursively:
    juce::int64 totalLength = getTotalLength();

    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return true;

    try {
      if (lastReadWasSmallerThanExpected) {
        return true;
      }

      return fileLike.attr("tell")().cast<juce::int64>() == totalLength;
    } catch (py::error_already_set e) {
      e.restore();
      return true;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return true;
    }
  }

  juce::int64 getPosition() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
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

  bool setPosition(juce::int64 pos) noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
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

/**
 * A juce::InputStream subclass that fetches its
 * data from a provided Python file-like object.
 */
class PythonMemoryViewInputStream : public PythonInputStream {
public:
  PythonMemoryViewInputStream(py::buffer bufferLike, py::object passedObject)
      : PythonInputStream(bufferLike) {
    info = bufferLike.request();
    totalLength = info.size * info.itemsize;
    repr = py::repr(passedObject).cast<std::string>();

    if (py::hasattr(passedObject, "tell")) {
      try {
        offset = std::min(
            std::max(0LL, passedObject.attr("tell")().cast<juce::int64>()),
            totalLength);
      } catch (py::error_already_set e) {
        e.restore();
      } catch (std::exception e) {
        // If we can't call tell(), that's fine; just assume an offset of 0.
      }
    }
  }

  std::string getRepresentation() override { return repr; }

  bool isSeekable() noexcept override { return true; }

  juce::int64 getTotalLength() noexcept override { return totalLength; }

  int read(void *buffer, int bytesToRead) noexcept override {
    if (offset + bytesToRead >= totalLength) {
      bytesToRead = totalLength - offset;
    }

    std::memcpy(buffer, ((const char *)info.ptr) + offset, bytesToRead);
    offset += bytesToRead;
    return bytesToRead;
  }

  bool isExhausted() noexcept override { return offset >= totalLength; }

  juce::int64 getPosition() noexcept override { return offset; }

  bool setPosition(juce::int64 pos) noexcept override {
    if (pos >= 0 && pos <= totalLength) {
      offset = pos;
      return true;
    }
    return false;
  }

private:
  juce::int64 totalLength = -1;
  juce::int64 offset = 0;
  py::buffer_info info;
  std::string repr;
};

}; // namespace Pedalboard