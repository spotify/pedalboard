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

namespace Pedalboard {

namespace PythonException {
// Check if there's a Python exception pending in the interpreter.
inline bool isPending() {
  py::gil_scoped_acquire acquire;
  return PyErr_Occurred() != nullptr;
}

// If an exception is pending, raise it as a C++ exception to break the current
// control flow and result in an error being thrown in Python later.
inline void raise() {
  py::gil_scoped_acquire acquire;

  if (PyErr_Occurred()) {
    py::error_already_set existingError;
    throw existingError;
  }
}
}; // namespace PythonException

/**
 * A base class for file-like Python object wrappers.
 */
class PythonFileLike {
public:
  PythonFileLike(py::object fileLike) : fileLike(fileLike) {}

  std::string getRepresentation() {
    py::gil_scoped_acquire acquire;
    if (PythonException::isPending())
      return "<__repr__ failed>";
    return py::repr(fileLike).cast<std::string>();
  }

  std::optional<std::string> getFilename() noexcept {
    // Some Python file-like objects expose a ".name" property.
    // If this object has that property, return its value;
    // otherwise return an empty optional.
    py::gil_scoped_acquire acquire;

    if (!PythonException::isPending() && py::hasattr(fileLike, "name")) {
      return py::str(fileLike.attr("name")).cast<std::string>();
    } else {
      return {};
    }
  }

  bool isSeekable() noexcept {
    py::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        return fileLike.attr("seekable")().cast<bool>();
      } catch (py::error_already_set e) {
        e.restore();
      } catch (const py::builtin_exception &e) {
        e.set_error();
      }
    }

    return false;
  }

  juce::int64 getPosition() noexcept {
    py::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        return fileLike.attr("tell")().cast<juce::int64>();
      } catch (py::error_already_set e) {
        e.restore();
      } catch (const py::builtin_exception &e) {
        e.set_error();
      }
    }

    return -1;
  }

  bool setPosition(juce::int64 pos) noexcept {
    py::gil_scoped_acquire acquire;

    if (!PythonException::isPending()) {
      try {
        if (fileLike.attr("seekable")().cast<bool>()) {
          fileLike.attr("seek")(pos);
        }

        return fileLike.attr("tell")().cast<juce::int64>() == pos;
      } catch (py::error_already_set e) {
        e.restore();
      } catch (const py::builtin_exception &e) {
        e.set_error();
      }
    }

    return false;
  }

  py::object getFileLikeObject() { return fileLike; }

protected:
  py::object fileLike;
};
}; // namespace Pedalboard