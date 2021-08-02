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

#include <chrono>
#include <thread>
#include <variant>

#include "JuceHeader.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "ExternalPlugin.h"
#include "JucePlugin.h"
#include "Plugin.h"
#include "process.h"

#include "plugins/Chorus.h"
#include "plugins/Compressor.h"
#include "plugins/Convolution.h"
#include "plugins/Distortion.h"
#include "plugins/Gain.h"
#include "plugins/HighpassFilter.h"
#include "plugins/LadderFilter.h"
#include "plugins/Limiter.h"
#include "plugins/LowpassFilter.h"
#include "plugins/NoiseGate.h"
#include "plugins/Phaser.h"
#include "plugins/Reverb.h"

using namespace Pedalboard;

static constexpr int DEFAULT_BUFFER_SIZE = 8192;

PYBIND11_MODULE(pedalboard_native, m) {
  m.def("process", process<float>,
        "Run a 32-bit floating point audio buffer through a list of Pedalboard "
        "plugins.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugins"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE);

  m.def("process", process<double>,
        "Run a 64-bit floating point audio buffer through a list of Pedalboard "
        "plugins. The buffer will be converted to 32-bit for processing.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugins"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE);

  m.def("process", processSingle<float>,
        "Run a 32-bit floating point audio buffer through a single Pedalboard "
        "plugin. (Note: if calling this multiple times with multiple plugins, "
        "consider passing a list of plugins instead.)",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugin"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE);

  m.def("process", processSingle<double>,
        "Run a 64-bit floating point audio buffer through a single Pedalboard "
        "plugin. (Note: if calling this multiple times with multiple plugins, "
        "consider passing a list of plugins instead.) The buffer will be "
        "converted to 32-bit for processing.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugin"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE);

  auto plugin =
      py::class_<Plugin>(m, "Plugin",
                         "A generic audio processing plugin. Base class of all "
                         "Pedalboard plugins.")
          .def(py::init([]() {
            throw py::type_error(
                "Plugin is an abstract base class - don't instantiate this "
                "directly, use its subclasses instead.");
            // This will never be hit, but is required to provide a non-void
            // type to return from this lambda or else the compiler can't do
            // type inference.
            return nullptr;
          }))
          .def(
              "process",
              [](Plugin *self,
                 const py::array_t<float, py::array::c_style> inputArray,
                 double sampleRate, unsigned int bufferSize) {
                return process(inputArray, sampleRate, {self}, bufferSize);
              },
              "Run a 32-bit floating point audio buffer through this plugin."
              "(Note: if calling this multiple times with multiple plugins, "
              "consider using pedalboard.process(...) instead.)",
              py::arg("input_array"), py::arg("sample_rate"),
              py::arg("buffer_size") = DEFAULT_BUFFER_SIZE)

          .def(
              "process",
              [](Plugin *self,
                 const py::array_t<double, py::array::c_style> inputArray,
                 double sampleRate, unsigned int bufferSize) {
                const py::array_t<float, py::array::c_style> float32InputArray =
                    inputArray.attr("astype")("float32");
                return process(float32InputArray, sampleRate, {self},
                               bufferSize);
              },
              "Run a 64-bit floating point audio buffer through this plugin."
              "(Note: if calling this multiple times with multiple plugins, "
              "consider using pedalboard.process(...) instead.) The buffer "
              "will be "
              "converted to 32-bit for processing.",
              py::arg("input_array"), py::arg("sample_rate"),
              py::arg("buffer_size") = DEFAULT_BUFFER_SIZE);
  plugin.attr("__call__") = plugin.attr("process");

  init_chorus(m);

  init_compressor(m);
  init_convolution(m);
  init_distortion(m);
  init_gain(m);
  init_highpass(m);
  init_ladderfilter(m);
  init_limiter(m);
  init_lowpass(m);
  init_noisegate(m);
  init_phaser(m);
  init_reverb(m);

  init_external_plugins(m);
};
