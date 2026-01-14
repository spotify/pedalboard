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

bool isWriteableFileLike(nb::object fileLike) {
  return nb::hasattr(fileLike, "write") && nb::hasattr(fileLike, "seek") &&
         nb::hasattr(fileLike, "tell") && nb::hasattr(fileLike, "seekable");
}

/**
 * A juce::OutputStream subclass that writes its
 * data to a provided Python file-like object.
 */
class PythonOutputStream : public juce::OutputStream, public PythonFileLike {
public:
  PythonOutputStream(nb::object fileLike) : PythonFileLike(fileLike) {
    if (!isWriteableFileLike(fileLike)) {
      throw nb::type_error("Expected a file-like object (with write, seek, "
                           "seekable, and tell methods).");
    }
  }

  virtual void flush() noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return;

    try {
      if (nb::hasattr(fileLike, "flush")) {
        fileLike.attr("flush")();
      }
    } catch (nb::python_error &e) {
      e.restore();
      return;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
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
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      nb::object writeResponse =
          fileLike.attr("write")(nb::bytes((const char *)ptr, numBytes));

      int bytesWritten;
      if (writeResponse.is_none()) {
        // Assume bytesWritten is numBytes if `write` returned None.
        // This shouldn't happen, but sometimes does if the file-like
        // object is not fully compliant with io.RawIOBase.
        bytesWritten = numBytes;
      } else {
        try {
          bytesWritten = nb::cast<int>(writeResponse);
        } catch (const nb::cast_error &e) {
          throw nb::type_error(
              (nb::cast<std::string>(nb::repr(fileLike.attr("write"))) +
              " was expected to return an integer, but got " +
              nb::cast<std::string>(nb::repr(writeResponse))).c_str());
        }
      }

      if (bytesWritten < (int)numBytes) {
        return false;
      }
    } catch (nb::python_error &e) {
      e.restore();
      return false;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
      return false;
    }
    return true;
  }

  virtual bool writeRepeatedByte(juce::uint8 byte,
                                 size_t numTimesToRepeat) noexcept override {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (PythonException::isPending())
      return false;

    try {
      const size_t maxEffectiveSize = std::min(numTimesToRepeat, (size_t)8192);
      std::vector<char> buffer(maxEffectiveSize, byte);

      for (size_t i = 0; i < numTimesToRepeat; i += buffer.size()) {
        const size_t chunkSize = std::min(numTimesToRepeat - i, buffer.size());

        nb::object writeResponse = fileLike.attr("write")(
            nb::bytes((const char *)buffer.data(), chunkSize));

        int bytesWritten;
        if (writeResponse.is_none()) {
          // Assume bytesWritten is numBytes if `write` returned None.
          // This shouldn't happen, but sometimes does if the file-like
          // object is not fully compliant with io.RawIOBase.
          bytesWritten = chunkSize;
        } else {
          try {
            bytesWritten = nb::cast<int>(writeResponse);
          } catch (const nb::cast_error &e) {
            throw nb::type_error(
                (nb::cast<std::string>(nb::repr(fileLike.attr("write"))) +
                " was expected to return an integer, but got " +
                nb::cast<std::string>(nb::repr(writeResponse))).c_str());
          }
        }

        if (bytesWritten != (int)chunkSize) {
          return false;
        }
      }
    } catch (nb::python_error &e) {
      e.restore();
      return false;
    } catch (const std::exception &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
      return false;
    }

    return true;
  }
};
}; // namespace Pedalboard
