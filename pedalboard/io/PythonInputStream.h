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

namespace nb = nanobind;

#include "../JuceHeader.h"
#include "PythonFileLike.h"

namespace Pedalboard {

bool isReadableFileLike(nb::object fileLike) {
  return nb::hasattr(fileLike, "read") && nb::hasattr(fileLike, "seek") &&
         nb::hasattr(fileLike, "tell") && nb::hasattr(fileLike, "seekable");
}

struct BufferInfo {
  void *ptr;
  size_t size;
  size_t itemsize;
};

std::optional<BufferInfo> tryConvertingToBuffer(nb::object bufferLike) {
  try {
    // Try to get buffer interface from the object
    Py_buffer view;
    if (PyObject_GetBuffer(bufferLike.ptr(), &view, PyBUF_SIMPLE) == 0) {
      BufferInfo info;
      info.ptr = view.buf;
      info.size = view.len / (view.itemsize > 0 ? view.itemsize : 1);
      info.itemsize = view.itemsize > 0 ? view.itemsize : 1;
      PyBuffer_Release(&view);
      return {info};
    }
    PyErr_Clear();

    // If not already a buffer, can we call getbuffer() on it?
    if (nb::hasattr(bufferLike, "getbuffer")) {
      nb::object getBufferResult = bufferLike.attr("getbuffer")();
      if (PyObject_GetBuffer(getBufferResult.ptr(), &view, PyBUF_SIMPLE) == 0) {
        BufferInfo info;
        info.ptr = view.buf;
        info.size = view.len / (view.itemsize > 0 ? view.itemsize : 1);
        info.itemsize = view.itemsize > 0 ? view.itemsize : 1;
        PyBuffer_Release(&view);
        return {info};
      }
      PyErr_Clear();
    }

    return {};
  } catch (std::exception &e) {
    return {};
  }
}

/**
 * A juce::InputStream subclass that fetches its
 * data from a provided Python file-like object.
 */
class PythonInputStream : public juce::InputStream, public PythonFileLike {
public:
  PythonInputStream(nb::object fileLike) : PythonFileLike(fileLike) {}
  virtual ~PythonInputStream() {}

  juce::int64 getTotalLength() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return -1;

    // TODO: Try reading a couple of Python properties that may contain the
    // total length: urllib3.response.HTTPResponse provides `length_remaining`,
    // for instance

    try {
      if (!nb::cast<bool>(fileLike.attr("seekable")())) {
        return -1;
      }

      if (totalLength == -1) {
        juce::int64 pos = nb::cast<juce::int64>(fileLike.attr("tell")());
        fileLike.attr("seek")(0, 2);
        totalLength = nb::cast<juce::int64>(fileLike.attr("tell")());
        fileLike.attr("seek")(pos, 0);
      }
    } catch (nb::python_error &e) {
      e.restore();
      return -1;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
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

    nb::gil_scoped_acquire acquire;
    if (PythonException::isPending())
      return 0;

    try {
      auto readResult = fileLike.attr("read")(bytesToRead);

      if (!nb::isinstance<nb::bytes>(readResult)) {
        std::string message =
            "File-like object passed to AudioFile was expected to return "
            "bytes from its read(...) method, but "
            "returned " +
            nb::cast<std::string>(nb::str(readResult.type().attr("__name__"))) +
            ".";

        if (nb::hasattr(fileLike, "mode") &&
            nb::cast<std::string>(nb::str(fileLike.attr("mode"))) == "r") {
          message += " (Try opening the stream in \"rb\" mode instead of "
                     "\"r\" mode if possible.)";
        }

        throw nb::type_error(message.c_str());
        return 0;
      }

      nb::bytes bytesObject = nb::cast<nb::bytes>(readResult);
      const char *pythonBuffer = bytesObject.c_str();
      size_t pythonLength = bytesObject.size();

      if (!buffer && pythonLength > 0) {
        throw std::runtime_error("Internal error: bytes pointer is null, but a "
                               "non-zero number of bytes were returned!");
      }

      if (buffer && pythonLength) {
        std::memcpy(buffer, pythonBuffer, pythonLength);
      }

      lastReadWasSmallerThanExpected = bytesToRead > (int)pythonLength;
      return pythonLength;
    } catch (nb::python_error &e) {
      e.restore();
      return 0;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
      return 0;
    }
  }

  bool isExhausted() noexcept override {
    // Read this up front to avoid releasing the object lock recursively:
    juce::int64 totalLen = getTotalLength();

    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return true;

    try {
      if (lastReadWasSmallerThanExpected) {
        return true;
      }

      return nb::cast<juce::int64>(fileLike.attr("tell")()) == totalLen;
    } catch (nb::python_error &e) {
      e.restore();
      return true;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
      return true;
    }
  }

  juce::int64 getPosition() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return -1;

    try {
      return nb::cast<juce::int64>(fileLike.attr("tell")());
    } catch (nb::python_error &e) {
      e.restore();
      return -1;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
      return -1;
    }
  }

  bool setPosition(juce::int64 pos) noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    ClearErrnoBeforeReturn clearErrnoBeforeReturn;
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      if (nb::cast<bool>(fileLike.attr("seekable")())) {
        fileLike.attr("seek")(pos);
        lastReadWasSmallerThanExpected = false;
      }

      return nb::cast<juce::int64>(fileLike.attr("tell")()) == pos;
    } catch (nb::python_error &e) {
      e.restore();
      return false;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
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
  PythonMemoryViewInputStream(nb::object bufferLike, nb::object passedObject)
      : PythonInputStream(bufferLike) {
    Py_buffer view;
    if (PyObject_GetBuffer(bufferLike.ptr(), &view, PyBUF_SIMPLE) != 0) {
      PyErr_Clear();
      throw std::runtime_error("Failed to get buffer from object");
    }
    bufferPtr = view.buf;
    totalLength = view.len;
    PyBuffer_Release(&view);
    
    repr = nb::cast<std::string>(nb::repr(passedObject));

    if (nb::hasattr(passedObject, "tell")) {
      try {
        offset = std::min(
            std::max((juce::int64)0, nb::cast<juce::int64>(passedObject.attr("tell")())),
            totalLength);
      } catch (nb::python_error &e) {
        e.restore();
      } catch (std::exception &e) {
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

    std::memcpy(buffer, ((const char *)bufferPtr) + offset, bytesToRead);
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
  void *bufferPtr = nullptr;
  std::string repr;
};

}; // namespace Pedalboard