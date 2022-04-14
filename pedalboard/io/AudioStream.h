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

#include <mutex>
#include <optional>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "../BufferUtils.h"
#include "../JuceHeader.h"
#include "AudioFile.h"
#include "PythonInputStream.h"

namespace py = pybind11;

namespace Pedalboard {

class AudioStream : public std::enable_shared_from_this<AudioStream>,
                    public juce::AudioIODeviceCallback {
public:
  AudioStream(std::string inputDeviceName, std::string outputDeviceName,
              double sampleRate, int bufferSize) {
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = inputDeviceName;
    setup.outputDeviceName = outputDeviceName;
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;

    juce::String defaultDeviceName;
    juce::String error = deviceManager.initialise(2, 2, nullptr, true,
                                                  defaultDeviceName, &setup);
    if (!error.isEmpty()) {
      throw std::domain_error(error.toStdString());
    }
  }

  ~AudioStream() { close(); }

  std::vector<std::shared_ptr<Plugin>> getPluginList() { return plugins; }

  void setPluginList(std::vector<std::shared_ptr<Plugin>> plugins) {
    this->plugins = plugins;

    if (isRunning) {
      for (auto plugin : this->plugins) {
        plugin->prepare(spec);
      }
    }
  }

  void close() { deviceManager.closeAudioDevice(); }

  void start() { deviceManager.addAudioCallback(this); }

  void stop() { deviceManager.removeAudioCallback(this); }

  std::shared_ptr<AudioStream> enter() {
    start();
    return shared_from_this();
  }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    stop();
  }

  static std::vector<std::string> getDeviceNames() {
    juce::AudioDeviceManager deviceManager;

    std::vector<std::string> names;
    for (auto *type : deviceManager.getAvailableDeviceTypes()) {
      for (juce::String name : type->getDeviceNames(false)) {
        names.push_back(name.toStdString());
      }
      for (juce::String name : type->getDeviceNames(true)) {
        names.push_back(name.toStdString());
      }
    }
    return names;
  }

  virtual void audioDeviceIOCallback(const float **inputChannelData,
                                     int numInputChannels,
                                     float **outputChannelData,
                                     int numOutputChannels, int numSamples) {
    for (int i = 0; i < numOutputChannels; i++) {
      const float *inputChannel = inputChannelData[i % numInputChannels];
      std::memcpy((char *)outputChannelData[i], (char *)inputChannel,
                  numSamples * sizeof(float));
    }

    auto ioBlock = juce::dsp::AudioBlock<float>(
        outputChannelData, numOutputChannels, 0, numSamples);
    juce::dsp::ProcessContextReplacing<float> context(ioBlock);

    for (auto plugin : plugins) {
      int outputSamples = plugin->process(context);
    }
  }

  virtual void audioDeviceAboutToStart(juce::AudioIODevice *device) {
    spec.sampleRate = deviceManager.getAudioDeviceSetup().sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(
        deviceManager.getAudioDeviceSetup().bufferSize);
    spec.numChannels = static_cast<juce::uint32>(
        device->getActiveOutputChannels().countNumberOfSetBits());

    for (auto plugin : plugins) {
      plugin->prepare(spec);
    }
    isRunning = true;
  }

  virtual void audioDeviceStopped() {
    for (auto plugin : plugins) {
      plugin->reset();
    }
    isRunning = false;
  }

private:
  juce::AudioDeviceManager deviceManager;
  juce::dsp::ProcessSpec spec;
  bool isRunning = false;

  std::vector<std::shared_ptr<Plugin>> plugins;
};

inline void init_audio_stream(py::module &m) {
  py::class_<AudioStream, std::shared_ptr<AudioStream>>(
      m, "AudioStream",
      "A class that pipes audio from an input device to an output device, "
      "passing it through a Pedalboard to add effects.")
      .def(
          py::init([](std::string inputDeviceName, std::string outputDeviceName,
                      double sampleRate, int bufferSize) {
            return std::make_shared<AudioStream>(
                inputDeviceName, outputDeviceName, sampleRate, bufferSize);
          }),
          py::arg("input_device_name"), py::arg("output_device_name"),
          py::arg("sample_rate"), py::arg("buffer_size") = 512)
      .def("start", &AudioStream::start, "Start processing audio.")
      .def("stop", &AudioStream::stop, "Stop processing audio.")
      .def("__enter__", &AudioStream::enter)
      .def("__exit__", &AudioStream::exit)
      .def("__repr__",
           [](const AudioStream &stream) {
             std::ostringstream ss;
             ss << "<pedalboard.io.AudioStream";
             ss << " at " << &stream;
             ss << ">";
             return ss.str();
           })
      .def_property("plugins", &AudioStream::getPluginList,
                    &AudioStream::setPluginList,
                    "List of plugins currently in the chain.")
      .def_property_readonly_static(
          "device_names",
          [](py::object *obj) { return AudioStream::getDeviceNames(); },
          "The currently-available device names.");
}
} // namespace Pedalboard