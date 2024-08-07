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
#include "TimeStretch.h"
#include "process.h"

#include "plugin_templates/FixedBlockSize.h"
#include "plugin_templates/ForceMono.h"
#include "plugin_templates/PrimeWithSilence.h"
#include "plugin_templates/Resample.h"

#include "plugins/AddLatency.h"
#include "plugins/Bitcrush.h"
#include "plugins/Chain.h"
#include "plugins/Chorus.h"
#include "plugins/Clipping.h"
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
#include "io/ResampledReadableAudioFile.h"
#include "io/StreamResampler.h"
#include "io/WriteableAudioFile.h"

using namespace Pedalboard;

PYBIND11_MODULE(pedalboard_native, m, py::mod_gil_not_used()) {
  m.doc() =
      ("This module provides classes and functions for generating and adding "
       "effects to audio. Most classes in this module are subclasses of "
       "``Plugin``, each of which allows applying effects to an audio buffer "
       "or stream.\n\nFor audio I/O classes (i.e.: reading and writing audio "
       "files), see ``pedalboard.io``.");

  auto plugin = py::class_<Plugin, std::shared_ptr<Plugin>>(
      m, "Plugin",
      "A generic audio processing plugin. Base class of all Pedalboard "
      "plugins.");

  m.def(
      "process",
      [](const py::array inputArray, double sampleRate,
         const std::vector<std::shared_ptr<Plugin>> plugins,
         unsigned int bufferSize, bool reset) {
        return process(inputArray, sampleRate, plugins, bufferSize, reset);
      },
      R"(
Run a 32-bit or 64-bit floating point audio buffer through a
list of Pedalboard plugins. If the provided buffer uses a 64-bit datatype,
it will be converted to 32-bit for processing.

The provided ``buffer_size`` argument will be used to control the size of
each chunk of audio provided into the plugins. Higher buffer sizes may speed up
processing at the expense of memory usage.

The ``reset`` flag determines if all of the plugins should be reset before
processing begins, clearing any state from previous calls to ``process``.
If calling ``process`` multiple times while processing the same audio file
or buffer, set ``reset`` to ``False``.

:meta private:
)",
      py::arg("input_array"), py::arg("sample_rate"), py::arg("plugins"),
      py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true);

  plugin
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
          "reset",
          [](std::shared_ptr<Plugin> self) {
            self->reset();
            // Only reset the last channel layout if the user explicitly calls
            // reset from the Python side:
            self->resetLastChannelLayout();
          },
          "Clear any internal state stored by this plugin (e.g.: reverb "
          "tails, delay lines, LFO state, etc). The values of plugin "
          "parameters will remain unchanged. ")
      .def(
          "process",
          [](std::shared_ptr<Plugin> self, const py::array inputArray,
             double sampleRate, unsigned int bufferSize, bool reset) {
            return process(inputArray, sampleRate, {self}, bufferSize, reset);
          },
          R"(
Run a 32-bit or 64-bit floating point audio buffer through this plugin.
(If calling this multiple times with multiple plugins, consider creating a
:class:`pedalboard.Pedalboard` object instead.)

The returned array may contain up to (but not more than) the same number of
samples as were provided. If fewer samples were returned than expected, the
plugin has likely buffered audio inside itself. To receive the remaining
audio, pass another audio buffer into ``process`` with ``reset`` set to
``True``.

If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
for processing.

The provided ``buffer_size`` argument will be used to control the size of
each chunk of audio provided to the plugin. Higher buffer sizes may speed up
processing at the expense of memory usage.

The ``reset`` flag determines if all of the plugins should be reset before
processing begins, clearing any state from previous calls to ``process``.
If calling ``process`` multiple times while processing the same audio file
or buffer, set ``reset`` to ``False``.

The layout of the provided ``input_array`` will be automatically detected,
assuming that the smaller dimension corresponds with the number of channels.
If the number of samples and the number of channels are the same, each
:py:class:`Plugin` object will use the last-detected channel layout until
:py:meth:`reset` is explicitly called (as of v0.9.9).

.. note::
    The :py:meth:`process` method can also be used via :py:meth:`__call__`;
    i.e.: just calling this object like a function (``my_plugin(...)``) will
    automatically invoke :py:meth:`process` with the same arguments.
)",
          py::arg("input_array"), py::arg("sample_rate"),
          py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true)
      .def(
          "__call__",
          [](std::shared_ptr<Plugin> self, const py::array inputArray,
             double sampleRate, unsigned int bufferSize, bool reset) {
            return process(inputArray, sampleRate, {self}, bufferSize, reset);
          },
          "Run an audio buffer through this plugin. Alias for "
          ":py:meth:`process`.",
          py::arg("input_array"), py::arg("sample_rate"),
          py::arg("buffer_size") = DEFAULT_BUFFER_SIZE, py::arg("reset") = true)
      .def_property_readonly(
          "is_effect",
          [](std::shared_ptr<Plugin> self) {
            return self->acceptsAudioInput();
          },
          "True iff this plugin is an audio effect and accepts audio "
          "as input.\n\n*Introduced in v0.7.4.*")
      .def_property_readonly(
          "is_instrument",
          [](std::shared_ptr<Plugin> self) {
            return !self->acceptsAudioInput();
          },
          "True iff this plugin is not an audio effect and accepts only "
          "MIDI input, not audio.\n\n*Introduced in v0.7.4.*");

  init_plugin_container(m);

  // Publicly accessible plugins:
  init_bitcrush(m);
  init_chorus(m);
  init_clipping(m);
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

  // Classes that don't perform any audio effects, but that add other utilities:
  py::module utils = m.def_submodule("utils");
  init_mix(utils);
  init_chain(utils);
  init_time_stretch(utils);

  // Internal plugins for testing, debugging, etc:
  py::module internal = m.def_submodule("_internal");
  init_add_latency(internal);
  init_prime_with_silence_test_plugin(internal);
  init_resample_with_latency(internal);
  init_fixed_size_block_test_plugin(internal);
  init_force_mono_test_plugin(internal);

  // I/O helpers and utilities:
  py::module io = m.def_submodule("io");
  io.doc() = "This module provides classes and functions for reading and "
             "writing audio files or streams.\n\n*Introduced in v0.5.1.*";

  auto pyAudioFile = declare_audio_file(io);
  auto pyReadableAudioFile = declare_readable_audio_file(io);
  auto pyResampledReadableAudioFile = declare_resampled_readable_audio_file(io);
  auto pyWriteableAudioFile = declare_writeable_audio_file(io);

  init_audio_file(pyAudioFile);
  init_readable_audio_file(io, pyReadableAudioFile);
  init_resampled_readable_audio_file(io, pyResampledReadableAudioFile);
  init_writeable_audio_file(io, pyWriteableAudioFile);

  init_stream_resampler(io);
  init_audio_stream(io);
};
