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

#include "BufferUtils.h"
#include "JuceHeader.h"
#include "Plugin.h"

using namespace pybind11::literals;

namespace Pedalboard {
/**
 * A C++ class that wraps a Python object with three methods:
 *  - prepare(sample_rate: float, num_channels: int, maximum_block_size: int)
 *  - process(np.ndarray[np.float32]) -> np.ndarray[np.float32]
 *  - reset()
 */
class PythonPlugin : public Plugin {
public:
  PythonPlugin(py::object pythonPluginLike)
      : pythonPluginLike(pythonPluginLike) {
    if (!py::hasattr(pythonPluginLike, "process") &&
        !py::hasattr(pythonPluginLike, "__call__")) {
      throw py::type_error(
          "Expected Python plugin-like object to be either a callable (i.e.: a "
          "function or lambda) or to be an object with a process method (and "
          "optional prepare and reset methods).");
    }
  }

  virtual ~PythonPlugin(){};

  void prepare(const juce::dsp::ProcessSpec &spec) override {
    if (lastSpec.sampleRate != spec.sampleRate ||
        lastSpec.maximumBlockSize < spec.maximumBlockSize ||
        spec.numChannels != lastSpec.numChannels) {

      py::gil_scoped_acquire gil;
      try {
        if (py::hasattr(pythonPluginLike, "prepare")) {
          pythonPluginLike.attr("prepare")("sample_rate"_a = spec.sampleRate,
                                           "num_channels"_a = spec.numChannels,
                                           "maximum_block_size"_a =
                                               spec.maximumBlockSize);
        }
      } catch (py::error_already_set &e) {
        py::raise_from(e, PyExc_RuntimeError,
                       ("PythonPlugin failed to call \"prepare\" method on " +
                        py::repr(pythonPluginLike).cast<std::string>())
                           .c_str());
        throw py::error_already_set();
      }

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override {
    auto outputBlock = context.getOutputBlock();
    juce::AudioBuffer<float> bufferFromPython;

    {
      py::gil_scoped_acquire acquire;

      py::object callable = pythonPluginLike;
      if (py::hasattr(pythonPluginLike, "process")) {
        callable = pythonPluginLike.attr("process");
      }

      const float **channels = (const float **)alloca(
          outputBlock.getNumChannels() * sizeof(float *));

      for (int c = 0; c < outputBlock.getNumChannels(); c++) {
        channels[c] = outputBlock.getChannelPointer(c);
      }

      juce::AudioBuffer<float> bufferForPython((float *const *)channels,
                                               outputBlock.getNumChannels(),
                                               outputBlock.getNumSamples());
      py::array_t<float> arrayForPython = copyJuceBufferIntoPyArray<float>(
          bufferForPython, ChannelLayout::NotInterleaved, 0);

      py::array_t<float> response;
      try {
        response = callable(arrayForPython).cast<py::array_t<float>>();
      } catch (py::error_already_set &e) {
        if (py::hasattr(pythonPluginLike, "process")) {
          py::raise_from(
              e, PyExc_RuntimeError,
              ("PythonPlugin failed to call the \"process\" method of " +
               py::repr(pythonPluginLike).cast<std::string>())
                  .c_str());
        } else {
          py::raise_from(e, PyExc_RuntimeError,
                         ("PythonPlugin failed to call " +
                          py::repr(pythonPluginLike).cast<std::string>())
                             .c_str());
        }

        throw py::error_already_set();
      }

      // TODO: We could avoid a copy here by changing BufferUtils to support an
      // existing AudioBlock to copy into, but that's an overhaul for another
      // day.
      try {
        bufferFromPython = copyPyArrayIntoJuceBuffer<float>(response);
      } catch (std::runtime_error &e) {
        std::throw_with_nested(std::runtime_error(
            "PythonPlugin expected a buffer with zero or more samples of " +
            std::to_string(outputBlock.getNumChannels()) +
            "-channel audio, but was unable to interpret the audio data "
            "returned by " +
            py::repr(pythonPluginLike).cast<std::string>()));
      }

      if (bufferFromPython.getNumSamples() > outputBlock.getNumSamples()) {
        throw std::domain_error(
            "PythonPlugin wrapping " +
            py::repr(pythonPluginLike).cast<std::string>() +
            " returned more samples than provided, which is not supported by "
            "Pedalboard. (Provided " +
            std::to_string(outputBlock.getNumSamples()) + " samples of " +
            std::to_string(outputBlock.getNumChannels()) +
            "-channel audio, but got back " +
            std::to_string(bufferFromPython.getNumSamples()) + " samples of " +
            std::to_string(bufferFromPython.getNumChannels()) +
            "-channel audio.)");
      }

      if (bufferFromPython.getNumChannels() != outputBlock.getNumChannels()) {
        throw std::domain_error(
            "PythonPlugin wrapping " +
            py::repr(pythonPluginLike).cast<std::string>() +
            " returned a different number of channels than provided, which is "
            "not supported by "
            "Pedalboard. (Provided " +
            std::to_string(outputBlock.getNumSamples()) + " samples of " +
            std::to_string(outputBlock.getNumChannels()) +
            "-channel audio, but got back " +
            std::to_string(bufferFromPython.getNumSamples()) + " samples of " +
            std::to_string(bufferFromPython.getNumChannels()) +
            "-channel audio.)");
      }
    }

    outputBlock.copyFrom(bufferFromPython, 0,
                         outputBlock.getNumSamples() -
                             bufferFromPython.getNumSamples(),
                         bufferFromPython.getNumSamples());
    return bufferFromPython.getNumSamples();
  }

  void reset() override {
    py::gil_scoped_acquire gil;
    if (py::hasattr(pythonPluginLike, "reset")) {
      try {
        pythonPluginLike.attr("reset")();
      } catch (py::error_already_set &e) {
        py::raise_from(e, PyExc_RuntimeError,
                       ("PythonPlugin failed to call \"reset\" method on " +
                        py::repr(pythonPluginLike).cast<std::string>())
                           .c_str());
        throw py::error_already_set();
      }
    }
  }

  py::object getPythonObject() { return pythonPluginLike; }

private:
  py::object pythonPluginLike;
};

inline void init_python_plugin(py::module &m) {
  py::class_<PythonPlugin, Plugin, std::shared_ptr<PythonPlugin>>(
      m, "PythonPlugin",
      "A wrapper around a Python object to be called as an audio plugin. "
      "The provided object must either be a callable (i.e.: a function or "
      "lambda) or an object with a process(numpy.ndarray[np.float32]) "
      "method. If the provided object has prepare(sample_rate: float, "
      "num_channels: int, maximum_block_size: int) and reset() methods, "
      "these will be called during processing.")
      .def(py::init([](py::object wrapped) {
             return std::make_unique<PythonPlugin>(wrapped);
           }),
           py::arg("wrapped"))
      .def("__repr__",
           [](PythonPlugin &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.PythonPlugin";
             ss << " wrapped="
                << py::repr(plugin.getPythonObject()).cast<std::string>();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly("wrapped", &PythonPlugin::getPythonObject,
                             "The Python object wrapped by this plugin.");
}

} // namespace Pedalboard
