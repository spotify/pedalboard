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

#include <chrono>
#include <thread>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "../BufferUtils.h"
#include "../JuceHeader.h"
#include "../plugins/Chain.h"
#include "AudioFile.h"
#include "PythonInputStream.h"

namespace py = pybind11;

namespace Pedalboard {

class AudioStream : public std::enable_shared_from_this<AudioStream>,
                    public juce::AudioIODeviceCallback {
public:
  AudioStream(std::string inputDeviceName, std::string outputDeviceName,
              double sampleRate, int bufferSize)
      : pedalboard(
            std::make_shared<Chain>(std::vector<std::shared_ptr<Plugin>>())),
        livePedalboard(std::vector<std::shared_ptr<Plugin>>()) {
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

  ~AudioStream() {
    stop();
    close();
  }

  std::shared_ptr<Chain> getPedalboard() { return pedalboard; }

  void setPedalboard(std::shared_ptr<Chain> chain) { pedalboard = chain; }

  void close() { deviceManager.closeAudioDevice(); }

  void start() {
    isRunning = true;
    changeObserverThread =
        std::thread(&AudioStream::propagateChangesToAudioThread, this);
    deviceManager.addAudioCallback(this);
  }

  void stop() {
    deviceManager.removeAudioCallback(this);
    isRunning = false;
    if (changeObserverThread.joinable()) {
      changeObserverThread.join();
    }
  }

  void propagateChangesToAudioThread() {
    while (isRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      // Check to see if the live pedalboard and the regular pedalboard
      // differ, and apply any changes as necessary:
      if (pedalboard->getAllPlugins() != livePedalboard.getAllPlugins()) {
        livePedalboard.getPlugins().clear();
        for (auto plugin : pedalboard->getPlugins()) {
          plugin->prepare(spec);
          livePedalboard.getPlugins().push_back(plugin);
        }
      }
    }
  }

  void stream() {
    start();
    while (isRunning) {
      if (PyErr_CheckSignals() != 0) {
        break;
      }

      {
        py::gil_scoped_release release;

        // Let other Python threads run:
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    stop();
    throw py::error_already_set();
  }

  std::shared_ptr<AudioStream> enter() {
    start();
    return shared_from_this();
  }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    stop();
  }

  static std::vector<std::string> getDeviceNames(bool isInput) {
    juce::AudioDeviceManager deviceManager;

    std::vector<std::string> names;
    for (auto *type : deviceManager.getAvailableDeviceTypes()) {
      for (juce::String name : type->getDeviceNames(isInput)) {
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

    // TODO: Should getPlugins() here have a lock around it?
    for (auto plugin : livePedalboard.getPlugins()) {
      int outputSamples = plugin->process(context);
    }
  }

  virtual void audioDeviceAboutToStart(juce::AudioIODevice *device) {
    spec.sampleRate = deviceManager.getAudioDeviceSetup().sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(
        deviceManager.getAudioDeviceSetup().bufferSize);
    spec.numChannels = static_cast<juce::uint32>(
        device->getActiveOutputChannels().countNumberOfSetBits());

    for (auto plugin : livePedalboard.getPlugins()) {
      plugin->prepare(spec);
    }
  }

  virtual void audioDeviceStopped() {
    for (auto plugin : livePedalboard.getPlugins()) {
      plugin->reset();
    }
  }

  bool getIsRunning() const { return isRunning; }

  juce::AudioDeviceManager::AudioDeviceSetup getAudioDeviceSetup() const {
    return deviceManager.getAudioDeviceSetup();
  }

private:
  juce::AudioDeviceManager deviceManager;
  juce::dsp::ProcessSpec spec;
  bool isRunning = false;

  std::shared_ptr<Chain> pedalboard;

  // A "live" pedalboard, called from the audio thread.
  // This is not exposed to Python, and is only updated from C++.
  Chain livePedalboard;

  // A background thread, independent of the audio thread, that
  // watches for any changes to the Pedalboard object (which may
  // happen in Python) and copies those changes over to data
  // structures used by the audio thread.
  std::thread changeObserverThread;
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
          py::arg("sample_rate") = 44100, py::arg("buffer_size") = 512)
      .def("stream", &AudioStream::stream,
           "Stream audio from input to output, through the `plugins` on this "
           "AudioStream object, until a KeyboardInterrupt is received.")
      .def_property_readonly("running", &AudioStream::getIsRunning,
                             "True iff this stream is currently streaming live "
                             "audio from input to output.")
      .def("__enter__", &AudioStream::enter)
      .def("__exit__", &AudioStream::exit)
      .def("__repr__",
           [](const AudioStream &stream) {
             std::ostringstream ss;
             ss << "<pedalboard.io.AudioStream";
             auto audioDeviceSetup = stream.getAudioDeviceSetup();
             ss << " input_device_name="
                << audioDeviceSetup.inputDeviceName.toStdString();
             ss << " output_device_name="
                << audioDeviceSetup.outputDeviceName.toStdString();
             ss << " sample_rate="
                << juce::String(audioDeviceSetup.sampleRate, 2).toStdString();
             ss << " buffer_size=" << audioDeviceSetup.bufferSize;
             if (stream.getIsRunning()) {
               ss << " running";
             } else {
               ss << " not running";
             }
             ss << " at " << &stream;
             ss << ">";
             return ss.str();
           })
      .def_property(
          "pedalboard", &AudioStream::getPedalboard,
          &AudioStream::setPedalboard,
          "The Pedalboard object currently processing the live effects chain.")
      .def_property_readonly_static(
          "input_device_names",
          [](py::object *obj) { return AudioStream::getDeviceNames(true); },
          "The currently-available input device names.")
      .def_property_readonly_static(
          "output_device_names",
          [](py::object *obj) { return AudioStream::getDeviceNames(false); },
          "The currently-available output device names.");
}
} // namespace Pedalboard