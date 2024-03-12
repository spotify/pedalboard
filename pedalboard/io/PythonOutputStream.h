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

// If we're going to preallocate data, don't bother doing so in chunks smaller
// than this size:
static constexpr juce::int64 MIN_PREALLOCATION_SIZE = 16384;

/**
 * A juce::OutputStream subclass that writes its
 * data to a provided Python BytesIO-like object.
 * When used with a BytesIO object, this class enables
 * writing to the underlying memory without taking the GIL
 * for every write.
 *
 * This class is designed to be used in conjunction with
 * ScopedPreallocation to ensure that the underlying BytesIO
 * object is properly seeked and truncated once the scope
 * (i.e.: a write(..) call) exits.
 */
class PythonMemoryViewOutputStream : public PythonOutputStream {
public:
  /**
   * A scoped object that ensures that the underlying BytesIO object
   * is properly seeked and truncated once the scope (i.e.: a write(..) call)
   * exits.
   *
   * If the PythonMemoryViewOutputStream is called without a
   * ScopedPreallocation object, the default PythonOutputStream behaviour
   * will be used (i.e.: the GIL will be taken for all writes). This should
   * ideally only happen when the writer is being closed.
   */
  class ScopedPreallocation {
  public:
    ScopedPreallocation(PythonOutputStream *stream)
        : stream(dynamic_cast<PythonMemoryViewOutputStream *>(stream)) {
      if (this->stream) {
        this->stream->enterPreallocationScope();
      }
    }

    ~ScopedPreallocation() {
      if (stream && !PythonException::isPending()) {
        stream->exitPreallocationScope();
      }
    }

  private:
    PythonMemoryViewOutputStream *stream;
  };

  PythonMemoryViewOutputStream(py::object bytesIO)
      : PythonOutputStream(bytesIO) {

    try {
      writePointerPosition = fileLike.attr("tell")().cast<juce::int64>();
    } catch (py::error_already_set e) {
      e.restore();
      throw;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      throw;
    }
  }

  void enterPreallocationScope() {
    std::optional<py::buffer> memoryview = tryConvertingToBuffer(fileLike);
    if (PythonException::isPending()) {
      return;
    }

    // Get a reference to the currently-held BytesIO buffer:
    outputStreamLock.enterWrite();
    info = memoryview->request(/* writeable= */ true);
    totalLength = info->size * info->itemsize;
    isInPreallocationScope = true;
  }

  void exitPreallocationScope() {
    isInPreallocationScope = false;

    if (expanded) {
      // Only truncate the stream if we expanded it past its initial size:
      truncate();
    }

    info = {};
    outputStreamLock.exitWrite();

    try {
      fileLike.attr("seek")(writePointerPosition);
    } catch (py::error_already_set e) {
      e.restore();
    } catch (const py::builtin_exception &e) {
      e.set_error();
    }
  }

  virtual bool isSeekable() noexcept override {
    juce::ScopedReadLock lock(outputStreamLock);
    if (!isInPreallocationScope) {
      return PythonOutputStream::isSeekable();
      return true;
    }
  }

  /**
   * Ensure that the underlying BytesIO object contains
   * enough space such that a write of `size` bytes would
   * succeed without forcing a reallocation.
   */
  bool preallocate(size_t writeSize) {
    if (!isInPreallocationScope) {
      jassertfalse;
      return false;
    }

    if (writePointerPosition + writeSize <= totalLength) {
      // No reallocation necessary:
      return true;
    }

    juce::int64 desiredSize = writePointerPosition + writeSize;
    // Round desiredSize up to the next power of two to avoid
    // repeated reallocations:
    desiredSize =
        juce::nextPowerOfTwo(std::max(MIN_PREALLOCATION_SIZE, desiredSize));
    juce::int64 extraBytesRequired = desiredSize - totalLength;

    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    py::gil_scoped_acquire acquire;

    if (PythonException::isPending()) {
      return false;
    }

    if (extraBytesRequired) {
      // Write uninitialized bytes of the requested size to preallocate:
      // Note that after this write() call, the BytesIO write pointer will
      // point to the end of the buffer, but the `writePointerPosition` will
      // not move.

      expanded = true;

      // Call the destructor on the buffer_info so we can call write
      // on the BytesIO wrapper to expand its size:
      info = {};

      try {
        fileLike.attr("seek")(totalLength);
        juce::int64 endOfBuffer = fileLike.attr("tell")().cast<juce::int64>();
        printf("About to write %d bytes at position %d\n", extraBytesRequired,
               endOfBuffer);
        fileLike.attr("write")(py::bytes(nullptr, extraBytesRequired));
        juce::int64 endOfBufferAfterWrite =
            fileLike.attr("tell")().cast<juce::int64>();
        if (endOfBufferAfterWrite != desiredSize) {
          // PyErr_SetString gets used here it's the easiest way to throw a
          // Python exception without actually throwing a C++ exception which
          // would escape this function scope, which we can't do.
          PyErr_SetString(PyExc_RuntimeError,
                          ("Failed to preallocate enough space; wrote " +
                           std::to_string(extraBytesRequired) +
                           " bytes at position " + std::to_string(endOfBuffer) +
                           " bytes, but the write pointer is now at " +
                           std::to_string(endOfBufferAfterWrite) + " bytes.")
                              .c_str());
          return false;
        }
      } catch (py::error_already_set e) {
        e.restore();
        return false;
      } catch (const py::builtin_exception &e) {
        e.set_error();
        return false;
      }

      std::optional<py::buffer> memoryview = tryConvertingToBuffer(fileLike);
      if (PythonException::isPending()) {
        return false;
      }
      info = memoryview->request(/* writeable= */ true);
      totalLength = info->size * info->itemsize;
    }
    return true;
  }

  void truncate() {
    // Must be called with the GIL held:
    jassert(PyGILState_Check());
    jassert(!isInPreallocationScope);

    if (PythonException::isPending()) {
      return;
    }

    // Destroy the memoryview, which would prevent truncation:
    info = {};

    if (PythonException::isPending()) {
      return;
    }

    try {
      fileLike.attr("truncate")(writePointerPosition);
      if (PythonException::isPending()) {
        return;
      }
      expanded = false;
    } catch (py::error_already_set e) {
      e.restore();
      return;
    } catch (const py::builtin_exception &e) {
      e.set_error();
      return;
    }
  }

  virtual void flush() noexcept override {
    // Flushing should do nothing, as the bytes are already in the BytesIO.
    juce::ScopedReadLock lock(outputStreamLock);

    if (!isInPreallocationScope) {
      PythonOutputStream::flush();
    }
  }

  virtual juce::int64 getPosition() noexcept override {
    juce::ScopedReadLock lock(outputStreamLock);
    if (!isInPreallocationScope) {
      return PythonOutputStream::getPosition();
    }

    return writePointerPosition;
  }

  virtual bool setPosition(juce::int64 pos) noexcept override {
    juce::ScopedReadLock readLock(outputStreamLock);
    if (!isInPreallocationScope) {
      if (!PythonOutputStream::setPosition(pos)) {
        return false;
      }
    }

    juce::ScopedWriteLock writeLock(outputStreamLock);

    writePointerPosition = pos;
    return true;
  }

  virtual bool write(const void *ptr, size_t numBytes) noexcept override {
    juce::ScopedWriteLock lock(outputStreamLock);
    if (!isInPreallocationScope) {
      if (PythonOutputStream::write(ptr, numBytes)) {
        writePointerPosition += numBytes;
        return true;
      }
      return false;
    }

    // Copy numBytes from *ptr into the memoryview.
    // If the amount of preallocated space (totalLength -
    // writePointerPosition) is not enough, grab the GIL and reallocate enough
    // bytes, then try again.
    if (preallocate(numBytes)) {
      std::memcpy(((char *)info->ptr) + writePointerPosition, ptr, numBytes);
      writePointerPosition += numBytes;
      return true;
    }

    return false;
  }

  virtual bool writeRepeatedByte(juce::uint8 byte,
                                 size_t numTimesToRepeat) noexcept override {
    juce::ScopedWriteLock lock(outputStreamLock);
    if (!isInPreallocationScope) {
      if (PythonOutputStream::writeRepeatedByte(byte, numTimesToRepeat)) {
        writePointerPosition += numTimesToRepeat;
        return true;
      }
      return false;
    }

    if (preallocate(numTimesToRepeat)) {
      std::memset(((char *)info->ptr) + writePointerPosition, byte,
                  numTimesToRepeat);
      writePointerPosition += numTimesToRepeat;
      return true;
    }

    return false;
  }

private:
  juce::ReadWriteLock outputStreamLock;
  std::optional<py::buffer_info> info;
  bool expanded = false;
  bool isInPreallocationScope = false;
  juce::int64 totalLength = -1;
  juce::int64 writePointerPosition = -1;
};

}; // namespace Pedalboard
