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

#include <cerrno>
#include <mutex>
#include <optional>

namespace nb = nanobind;

#include "../JuceHeader.h"

namespace Pedalboard {

namespace PythonException {
// Check if there's a Python exception pending in the interpreter.
inline bool isPending() {
  nb::gil_scoped_acquire acquire;
  return PyErr_Occurred() != nullptr;
}

// If an exception is pending, raise it as a C++ exception to break the current
// control flow and result in an error being thrown in Python later.
inline void raise() {
  nb::gil_scoped_acquire acquire;

  if (PyErr_Occurred()) {
    throw nb::python_error();
  }
}
}; // namespace PythonException

/**
 * @brief A helper class that clears the thread's errno when destructed.
 *
 * This is used to avoid failure when the following sequence of events occurs:
 *
 * 1. A Python file-like object is passed to Pedalboard, which could call any
 * other native code in its methods
 * 2. Pedalboard calls a Python file-like object's methods (e.g. read(), seek(),
 * tell(), seekable())
 * 3. The native code sets errno to a non-zero value, but does not clear it
 * 4. Pedalboard's codecs (Ogg Vorbis, etc) check errno and fail to decode
 * 5. The user is presented with a cryptic error message
 *
 * This makes it seem like we're ignoring errno, but we're not; the Python-level
 * code should throw an exception if the file-like object has an error, and we
 * handle errors at that level correctly.
 *
 * See also: https://en.wikipedia.org/wiki/Errno.h
 */
class ClearErrnoBeforeReturn {
public:
  ClearErrnoBeforeReturn() {}
  ~ClearErrnoBeforeReturn() { errno = 0; }
};

/**
 * A tiny helper class that downgrades a write lock
 * to a read lock for the given scope. If we can't
 * regain the write lock on the provided object,
 * the GIL will be released in a busy loop until
 * the write lock becomes available to us.
 */
class ScopedDowngradeToReadLockWithGIL {
public:
  ScopedDowngradeToReadLockWithGIL(juce::ReadWriteLock *lock) : lock(lock) {
    if (lock) {
      lock->enterRead();
      lock->exitWrite();
    }
  }

  ~ScopedDowngradeToReadLockWithGIL() {
    if (lock) {
      while (!lock->tryEnterWrite()) {
        // If we have the GIL, release it briefly to avoid deadlocks:
        if (PyGILState_Check() == true) {
          nb::gil_scoped_release release;
        }
      }

      lock->exitRead();
    }
  }

private:
  juce::ReadWriteLock *lock;
};

/** Cribbed from Juce 7: */
class ScopedTryWriteLock {
public:
  ScopedTryWriteLock(juce::ReadWriteLock &lockIn) noexcept
      : lock(lockIn), lockWasSuccessful(lock.tryEnterWrite()) {}

  ~ScopedTryWriteLock() noexcept {
    if (lockWasSuccessful)
      lock.exitWrite();
  }

  bool isLocked() const noexcept { return lockWasSuccessful; }

  bool retryLock() noexcept { return lockWasSuccessful = lock.tryEnterWrite(); }

private:
  juce::ReadWriteLock &lock;
  bool lockWasSuccessful;
};

/**
 * A base class for file-like Python object wrappers.
 *
 * Note that the objectLock passed in will be unlocked before
 * the GIL is reacquired to avoid deadlocks.
 */
class PythonFileLike {
public:
  PythonFileLike(nb::object fileLike) : fileLike(fileLike) {}
  virtual ~PythonFileLike() {}

  virtual std::string getRepresentation() {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;
    if (PythonException::isPending())
      return "<__repr__ failed>";
    return nb::cast<std::string>(nb::repr(fileLike));
  }

  std::optional<std::string> getFilename() noexcept {
    // Some Python file-like objects expose a ".name" property.
    // If this object has that property, return its value;
    // otherwise return an empty optional.
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (!PythonException::isPending() && nb::hasattr(fileLike, "name")) {
      return nb::cast<std::string>(nb::str(fileLike.attr("name")));
    } else {
      return {};
    }
  }

  virtual bool isSeekable() noexcept {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        return nb::cast<bool>(fileLike.attr("seekable")());
      } catch (nb::python_error &e) {
        e.restore();
      } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
      }
    }

    return false;
  }

  virtual juce::int64 getPosition() noexcept {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        return nb::cast<juce::int64>(fileLike.attr("tell")());
      } catch (nb::python_error &e) {
        e.restore();
      } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
      }
    }

    return -1;
  }

  virtual bool setPosition(juce::int64 pos) noexcept {
    ScopedDowngradeToReadLockWithGIL lock(objectLock);
    nb::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        fileLike.attr("seek")(pos);
        return nb::cast<juce::int64>(fileLike.attr("tell")()) == pos;
      } catch (nb::python_error &e) {
        e.restore();
      } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
      }
    }

    return false;
  }

  nb::object getFileLikeObject() { return fileLike; }

  void setObjectLock(juce::ReadWriteLock *lock) { objectLock = lock; }

protected:
  nb::object fileLike;
  juce::ReadWriteLock *objectLock = nullptr;
};
}; // namespace Pedalboard