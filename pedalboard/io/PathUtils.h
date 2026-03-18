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

#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace Pedalboard {

/**
 * Check if a Python object is path-like (str or has __fspath__ method).
 *
 * NOTE: This function requires the GIL to be held by the caller.
 */
inline bool isPathLike(py::object obj) {
  return py::isinstance<py::str>(obj) || py::hasattr(obj, "__fspath__");
}

/**
 * Convert a Python path-like object (str, bytes, or os.PathLike) to a
 * std::string, without requiring std::filesystem::path.
 *
 * NOTE: This function requires the GIL to be held by the caller.
 */
inline std::string pathToString(py::object path) {
  // If it's already a string, just return it
  if (py::isinstance<py::str>(path)) {
    return path.cast<std::string>();
  }

  // Try calling os.fspath() to handle PathLike objects
  try {
    py::object os = py::module_::import("os");
    py::object fspath = os.attr("fspath");
    py::object result = fspath(path);
    return result.cast<std::string>();
  } catch (py::error_already_set &e) {
    throw py::type_error(
        "expected str, bytes, or os.PathLike object, not " +
        std::string(py::str(path.get_type().attr("__name__"))));
  }
}

} // namespace Pedalboard
