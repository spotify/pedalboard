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

bool isWriteableFileLike(py::object fileLike) {
  return py::hasattr(fileLike, "write") && py::hasattr(fileLike, "seek") &&
         py::hasattr(fileLike, "tell") && py::hasattr(fileLike, "seekable");
}

/**
 * A juce::OutputStream subclass that writes its
 * data to a provided Python file-like object.
 */
class PythonOutputStream : public juce::OutputStream, public PythonFileLike {
public:
  PythonOutputStream(py::object fileLike) : PythonFileLike(fileLike) {
    if (!isWriteableFileLike(fileLike)) {
      throw py::type_error("Expected a file-like object (with write, seek, "
                           "seekable, and tell methods).");
    }
  }

  virtual void flush() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return;

    try {
      if (py::hasattr(fileLike, "flush")) {
        fileLike.attr("flush")();
      }
    } catch (py::error_already_set e) {
      e.restore();
      return;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return;
    }
  }

  virtual juce::int64 getPosition() noexcept override {
    return PythonFileLike::getPosition();
  }

  virtual bool setPosition(juce::int64 pos) noexcept override {
    return PythonFileLike::setPosition(pos);
  }

  virtual bool write(const void *ptr, size_t numBytes) noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      py::object writeResponse =
          fileLike.attr("write")(py::bytes((const char *)ptr, numBytes));

      int bytesWritten;
      if (writeResponse.is_none()) {
        // Assume bytesWritten is numBytes if `write` returned None.
        // This shouldn't happen, but sometimes does if the file-like
        // object is not fully compliant with io.RawIOBase.
        bytesWritten = numBytes;
      } else {
        try {
          bytesWritten = writeResponse.cast<int>();
        } catch (const py::cast_error &e) {
          throw py::type_error(
              py::repr(fileLike.attr("write")).cast<std::string>() +
              " was expected to return an integer, but got " +
              py::repr(writeResponse).cast<std::string>());
        }
      }

      if (bytesWritten < numBytes) {
        return false;
      }
    } catch (py::error_already_set e) {
      e.restore();
      return false;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return false;
    }
    return true;
  }

  virtual bool writeRepeatedByte(juce::uint8 byte,
                                 size_t numTimesToRepeat) noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      const size_t maxEffectiveSize = std::min(numTimesToRepeat, (size_t)8192);
      std::vector<char> buffer(maxEffectiveSize, byte);

      for (size_t i = 0; i < numTimesToRepeat; i += buffer.size()) {
        const size_t chunkSize = std::min(numTimesToRepeat - i, buffer.size());

        py::object writeResponse = fileLike.attr("write")(
            py::bytes((const char *)buffer.data(), chunkSize));

        int bytesWritten;
        if (writeResponse.is_none()) {
          // Assume bytesWritten is numBytes if `write` returned None.
          // This shouldn't happen, but sometimes does if the file-like
          // object is not fully compliant with io.RawIOBase.
          bytesWritten = chunkSize;
        } else {
          try {
            bytesWritten = writeResponse.cast<int>();
          } catch (const py::cast_error &e) {
            throw py::type_error(
                py::repr(fileLike.attr("write")).cast<std::string>() +
                " was expected to return an integer, but got " +
                py::repr(writeResponse).cast<std::string>());
          }
        }

        if (bytesWritten != chunkSize) {
          return false;
        }
      }
    } catch (py::error_already_set e) {
      e.restore();
      return false;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return false;
    }

    return true;
  }
};
}; // namespace Pedalboard
