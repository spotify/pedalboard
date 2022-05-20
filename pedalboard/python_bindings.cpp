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
#include "PluginContainer.h"
#include "process.h"

#include "plugin_templates/FixedBlockSize.h"
#include "plugin_templates/ForceMono.h"
#include "plugin_templates/PrimeWithSilence.h"
#include "plugin_templates/Resample.h"

#include "plugins/AddLatency.h"
#include "plugins/Bitcrush.h"
#include "plugins/Chain.h"
#include "plugins/Chorus.h"
#include "plugins/Compressor.h"
#include "plugins/Convolution.h"
#include "plugins/Delay.h"
#include "plugins/Distortion.h"
#include "plugins/GSMFullRateCompressor.h"
#include "plugins/Gain.h"
#include "plugins/HighpassFilter.h"
#include "plugins/IIRFilters.h"
#include "plugins/Invert.h"
#include "plugins/LadderFilter.h"
#include "plugins/Limiter.h"
#include "plugins/LowpassFilter.h"
#include "plugins/MP3Compressor.h"
#include "plugins/Mix.h"
#include "plugins/NoiseGate.h"
#include "plugins/Phaser.h"
#include "plugins/PitchShift.h"
#include "plugins/Reverb.h"

#include "io/AudioFileInit.h"
#include "io/AudioStream.h"
#include "io/ReadableAudioFile.h"
#include "io/WriteableAudioFile.h"

using namespace Pedalboard;

PYBIND11_MODULE(pedalboard_native, m) {
  m.def("process", process<float>,
        "Run a 32-bit floating point audio buffer through a list of Pedalboard "
        "plugins.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugins"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true);

  m.def("process", process<double>,
        "Run a 64-bit floating point audio buffer through a list of Pedalboard "
        "plugins. The buffer will be converted to 32-bit for processing.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugins"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true);

  m.def("process", processSingle<float>,
        "Run a 32-bit floating point audio buffer through a single Pedalboard "
        "plugin. (Note: if calling this multiple times with multiple plugins, "
        "consider passing a list of plugins instead.)",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugin"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true);

  m.def("process", processSingle<double>,
        "Run a 64-bit floating point audio buffer through a single Pedalboard "
        "plugin. (Note: if calling this multiple times with multiple plugins, "
        "consider passing a list of plugins instead.) The buffer will be "
        "converted to 32-bit for processing.",
        py::arg("input_array"), py::arg("sample_rate"), py::arg("plugin"),
        py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true);

  auto plugin =
      py::class_<Plugin, std::shared_ptr<Plugin>>(
          m, "Plugin",
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
              "reset", &Plugin::reset,
              "Clear any internal state kept by this plugin (e.g.: reverb "
              "tails). The values of plugin parameters will remain unchanged. "
              "For most plugins, this is a fast operation; for some, this will "
              "cause a full re-instantiation of the plugin.")
          .def(
              "process",
              [](std::shared_ptr<Plugin> self,
                 const py::array_t<float, py::array::c_style> inputArray,
                 double sampleRate, unsigned int bufferSize, bool reset) {
                return process(inputArray, sampleRate, {self}, bufferSize,
                               reset);
              },
              "Run a 32-bit floating point audio buffer through this plugin."
              "(Note: if calling this multiple times with multiple plugins, "
              "consider using pedalboard.process(...) instead.)",
              py::arg("input_array"), py::arg("sample_rate"),
              py::arg("buffer_size") = DEFAULT_BUFFER_SIZE,
              py::arg("reset") = true)

          .def(
              "process",
              [](std::shared_ptr<Plugin> self,
                 const py::array_t<double, py::array::c_style> inputArray,
                 double sampleRate, unsigned int bufferSize, bool reset) {
                const py::array_t<float, py::array::c_style> float32InputArray =
                    inputArray.attr("astype")("float32");
                return process(float32InputArray, sampleRate, {self},
                               bufferSize, reset);
              },
              "Run a 64-bit floating point audio buffer through this plugin."
              "(Note: if calling this multiple times with multiple plugins, "
              "consider using pedalboard.process(...) instead.) The buffer "
              "will be converted to 32-bit for processing.",
              py::arg("input_array"), py::arg("sample_rate"),
              py::arg("buffer_size") = DEFAULT_BUFFER_SIZE,
              py::arg("reset") = true);
  plugin.attr("__call__") = plugin.attr("process");

  init_plugin_container(m);

  // Publicly accessible plugins:
  init_bitcrush(m);
  init_chorus(m);
  init_compressor(m);
  init_convolution(m);
  init_delay(m);
  init_distortion(m);
  init_gain(m);

  // Init Resample before GSMFullRateCompressor, which uses Resample::Quality:
  init_resample(m);
  init_gsm_full_rate_compressor(m);

  init_highpass(m);
  init_iir_filters(m);
  init_invert(m);
  init_ladderfilter(m);
  init_limiter(m);
  init_lowpass(m);
  init_mp3_compressor(m);
  init_noisegate(m);
  init_phaser(m);
  init_pitch_shift(m);
  init_reverb(m);

  init_external_plugins(m);

  // Plugins that don't perform any audio effects, but that add other utilities:
  py::module utils = m.def_submodule("utils");
  init_mix(utils);
  init_chain(utils);

  // Internal plugins for testing, debugging, etc:
  py::module internal = m.def_submodule("_internal");
  init_add_latency(internal);
  init_prime_with_silence_test_plugin(internal);
  init_resample_with_latency(internal);
  init_fixed_size_block_test_plugin(internal);
  init_force_mono_test_plugin(internal);

  // I/O helpers and utilities:
  py::module io = m.def_submodule("io");
  init_audio_file(io);
  init_readable_audio_file(io);
  init_writeable_audio_file(io);
  init_audio_stream(io);
};
