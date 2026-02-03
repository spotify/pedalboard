/*
 * pedalboard
 * Copyright 2025 Spotify AB
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

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace Pedalboard {

/**
 * Utility function to convert various array-like objects to py::array.
 * Supports:
 * - NumPy arrays (pass-through)
 * - PyTorch tensors (via .numpy() method)
 * - JAX arrays (via .__array__() method)
 * - TensorFlow tensors (via .numpy() method)
 * - CuPy arrays (via .get() method for CPU copy)
 * - Any object with __array__ interface
 */
inline py::array ensureArrayLike(py::object input) {
  // If we were already given a numpy array, just return it
  if (py::isinstance<py::array>(input)) {
    return py::reinterpret_borrow<py::array>(input);
  }

  // Check if it's a PyTorch tensor (has a .numpy() method)
  if (py::hasattr(input, "numpy") && py::hasattr(input, "dtype") &&
      py::hasattr(input, "device")) {
    // Check if tensor is on CPU
    py::object device = input.attr("device");
    std::string device_type = py::str(device.attr("type")).cast<std::string>();

    if (device_type != "cpu") {
      // Move tensor to CPU first
      input = input.attr("cpu")();
    }

    // Call .numpy() to get the numpy array
    // This shares memory with the tensor when possible
    return input.attr("numpy")().cast<py::array>();
  }

  // Check if it's a TensorFlow tensor (has .numpy() method but no .device)
  if (py::hasattr(input, "numpy") && !py::hasattr(input, "device")) {
    try {
      return input.attr("numpy")().cast<py::array>();
    } catch (...) {
      // Fall through to next option
    }
  }

  // Check if it's a CuPy array (has .get() method)
  if (py::hasattr(input, "get") && py::hasattr(input, "dtype") &&
      py::hasattr(input, "ndim")) {
    try {
      // .get() copies from GPU to CPU and returns numpy array
      return input.attr("get")().cast<py::array>();
    } catch (...) {
      // Fall through to next option
    }
  }

  // Check if it implements the array protocol (__array__)
  if (py::hasattr(input, "__array__")) {
    try {
      return input.attr("__array__")().cast<py::array>();
    } catch (...) {
      // Fall through to error
    }
  }

  // Try to convert directly to array as a last resort
  // py::array::ensure() will attempt to convert the object to an array
  // or return an invalid array handle if conversion fails
  py::array result = py::array::ensure(input);

  if (!result) {
    throw py::type_error(
        "Expected an array-like object (numpy array, torch tensor, etc.), "
        "but received: " +
        py::repr(input).cast<std::string>());
  }

  return result;
}

/**
 * Template version that ensures the array has a specific dtype
 */
template <typename T>
inline py::array_t<T> ensureArrayLikeWithType(py::object input) {
  py::array arr = ensureArrayLike(input);

  // If the array already has the correct type, return it
  if (py::isinstance<py::array_t<T>>(arr)) {
    return py::reinterpret_borrow<py::array_t<T>>(arr);
  }

  // Otherwise, cast to the desired type
  // Note: this may create a copy
  return arr.cast<py::array_t<T>>();
}

} // namespace Pedalboard
