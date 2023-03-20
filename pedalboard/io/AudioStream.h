/*
 * pedalboard
 * Copyright 2023 Spotify AB
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

namespace py = pybind11;

namespace Pedalboard {

class AudioStream : public std::enable_shared_from_this<AudioStream>
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    ,
                    public juce::AudioIODeviceCallback
#endif
{
public:
  AudioStream(std::string inputDeviceName, std::string outputDeviceName,
              std::optional<std::shared_ptr<Chain>> pedalboard,
              std::optional<double> sampleRate, int bufferSize,
              bool allowFeedback)
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
      : pedalboard(pedalboard ? *pedalboard
                              : std::make_shared<Chain>(
                                    std::vector<std::shared_ptr<Plugin>>())),
        livePedalboard(std::vector<std::shared_ptr<Plugin>>())
#endif
  {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = inputDeviceName;
    setup.outputDeviceName = outputDeviceName;
    // A value of 0 indicates we want to use whatever the device default is:
    setup.sampleRate = sampleRate ? *sampleRate : 0;
    setup.bufferSize = bufferSize;

    if (!allowFeedback &&
        juce::String(inputDeviceName).containsIgnoreCase("microphone") &&
        juce::String(outputDeviceName).containsIgnoreCase("speaker")) {
      throw std::runtime_error(
          "The audio input device passed to AudioStream looks like a "
          "microphone, and the output device looks like a speaker. This setup "
          "may cause feedback. To create an AudioStream anyways, pass "
          "`allow_feedback=True` to the AudioStream constructor.");
    }

    juce::String defaultDeviceName;
    juce::String error = deviceManager.initialise(2, 2, nullptr, true,
                                                  defaultDeviceName, &setup);
    if (!error.isEmpty()) {
      throw std::domain_error(error.toStdString());
    }
#else
    throw std::runtime_error("AudioStream is not supported on this platform.");
#endif
  }

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
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

      // Make sure nobody modifies the Python-side object while we're reading
      // it (without taking the GIL, which would be expensive)
      std::unique_lock<std::mutex> lock(pedalboard->mutex, std::try_to_lock);
      if (lock.owns_lock()) {
        // We can read livePedalboard's plugins without a lock, as we're the
        // only writer:
        if (pedalboard->getAllPlugins() != livePedalboard.getAllPlugins()) {
          // But if we need to write, then we need to try to get the lock:
          juce::SpinLock::ScopedTryLockType tryLock(livePedalboardMutex);
          if (tryLock.isLocked()) {
            livePedalboard.getPlugins().clear();

            for (auto plugin : pedalboard->getPlugins()) {
              plugin->prepare(spec);
              livePedalboard.getPlugins().push_back(plugin);
            }
          }
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
    bool shouldThrow = PythonException::isPending();
    stop();

    if (shouldThrow || PythonException::isPending())
      throw py::error_already_set();
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

    juce::SpinLock::ScopedTryLockType tryLock(livePedalboardMutex);
    if (tryLock.isLocked()) {
      for (auto plugin : livePedalboard.getPlugins()) {
        std::unique_lock<std::mutex> lock(pedalboard->mutex, std::try_to_lock);
        // If someone's running audio through this plugin in parallel (offline,
        // or in a different AudioStream object) then don't corrupt its state by
        // calling it here too; instead, just skip it:
        if (lock.owns_lock()) {
          plugin->process(context);
        }
      }
    }
  }

  virtual void audioDeviceAboutToStart(juce::AudioIODevice *device) {
    spec.sampleRate = deviceManager.getAudioDeviceSetup().sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(
        deviceManager.getAudioDeviceSetup().bufferSize);
    spec.numChannels = static_cast<juce::uint32>(
        device->getActiveOutputChannels().countNumberOfSetBits());

    juce::SpinLock::ScopedLockType lock(livePedalboardMutex);
    for (auto plugin : livePedalboard.getPlugins()) {
      plugin->prepare(spec);
    }
  }

  virtual void audioDeviceStopped() {
    juce::SpinLock::ScopedLockType lock(livePedalboardMutex);

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

  // A simple spin-lock mutex that we can try to acquire from
  // the audio thread, allowing us to avoid modifying state that's
  // currently being used for rendering.
  // The `livePedalboard` object already has a std::mutex on it,
  // but we want something that's faster and callable from the
  // dudio thread.
  juce::SpinLock livePedalboardMutex;

  // A "live" pedalboard, called from the audio thread.
  // This is not exposed to Python, and is only updated from C++.
  Chain livePedalboard;

  // A background thread, independent of the audio thread, that
  // watches for any changes to the Pedalboard object (which may
  // happen in Python) and copies those changes over to data
  // structures used by the audio thread.
  std::thread changeObserverThread;
#endif
};

inline void init_audio_stream(py::module &m) {
  auto audioStream =
      py::class_<AudioStream, std::shared_ptr<AudioStream>>(m, "AudioStream",
                                                            R"(
A class that streams audio from an input audio device (i.e.: a microphone,
audio interface, etc) to an output device (speaker, headphones),
passing it through a :class:`pedalboard.Pedalboard` to add effects.

:class:`AudioStream` may be used as a context manager::

   input_device_name = AudioStream.input_device_names[0]
   output_device_name = AudioStream.output_device_names[0]
   with AudioStream(input_device_name, output_device_name) as stream:
       # In this block, audio is streaming through `stream`!
       # Audio will be coming out of your speakers at this point.

       # Add plugins to the live audio stream:
       reverb = Reverb()
       stream.plugins.append(reverb)

       # Change plugin properties as the stream is running:
       reverb.wet_level = 1.0

       # Delete plugins:
       del stream.plugins[0]


:class:`AudioStream` may also be used synchronously::

   stream = AudioStream(ogg_buffer)
   stream.plugins.append(Reverb(wet_level=1.0))
   stream.run()  # Run the stream until Ctrl-C is received

.. note::
    This class uses C++ under the hood to ensure speed, thread safety,
    and avoid any locking concerns with Python's Global Interpreter Lock.
    Audio data processed by :class:`AudioStream` is not available to
    Python code; the only way to interact with the audio stream is through
    the :py:attr:`plugins` attribute.

.. warning::
    The :class:`AudioStream` class implements a context manager interface
    to ensure that audio streams are never left "dangling" (i.e.: running in
    the background without being stopped).
    
    While it is possible to call the :meth:`__enter__` method directly to run an
    audio stream in the background, this can have some nasty side effects. If the
    :class:`AudioStream` object is no longer reachable (not bound to a variable,
    not in scope, etc), the audio stream will continue to play back forever, and
    won't stop until the Python interpreter exits.

    To run an :class:`AudioStream` in the background, use Python's
    :py:mod:`threading` module to call the synchronous :meth:`run` method on a
    background thread, allowing for easier cleanup.

*Introduced in v0.7.0. Not supported on Linux.*
)")
          .def(py::init([](std::string inputDeviceName,
                           std::string outputDeviceName,
                           std::optional<std::shared_ptr<Chain>> pedalboard,
                           std::optional<double> sampleRate, int bufferSize,
                           bool allowFeedback) {
                 return std::make_shared<AudioStream>(
                     inputDeviceName, outputDeviceName, pedalboard, sampleRate,
                     bufferSize, allowFeedback);
               }),
               py::arg("input_device_name"), py::arg("output_device_name"),
               py::arg("plugins") = py::none(),
               py::arg("sample_rate") = py::none(),
               py::arg("buffer_size") = 512, py::arg("allow_feedback") = false);
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
  audioStream
      .def("run", &AudioStream::stream,
           "Start streaming audio from input to output, passing the audio "
           "stream  through the :py:attr:`plugins` on this AudioStream object. "
           "This call will block the current thread until a "
           ":py:exc:`KeyboardInterrupt` (``Ctrl-C``) is received.")
      .def_property_readonly(
          "running", &AudioStream::getIsRunning,
          ":py:const:`True` if this stream is currently streaming live "
          "audio from input to output, :py:const:`False` otherwise.")
      .def("__enter__", &AudioStream::enter,
           "Use this :class:`AudioStream` as a context manager. Entering the "
           "context manager will immediately start the audio stream, sending "
           "audio through to the output device.")
      .def("__exit__", &AudioStream::exit,
           "Exit the context manager, ending the audio stream. Once called, "
           "the audio stream will be stopped (i.e.: :py:attr:`running` will be "
           ":py:const:`False`).")
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
      .def_property("plugins", &AudioStream::getPedalboard,
                    &AudioStream::setPedalboard,
                    "The Pedalboard object that this AudioStream will use to "
                    "process audio.");
#endif
  audioStream
      .def_property_readonly_static(
          "input_device_names",
          [](py::object *obj) -> std::vector<std::string> {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
            return AudioStream::getDeviceNames(true);
#else
            return {};
#endif
          },
          "The input devices (i.e.: microphones, audio interfaces, etc.) "
          "currently available on the current machine.")
      .def_property_readonly_static(
          "output_device_names",
          [](py::object *obj) -> std::vector<std::string> {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
            return AudioStream::getDeviceNames(false);
#else
            return {};
#endif
          },
          "The output devices (i.e.: speakers, headphones, etc.) currently "
          "available on the current machine.");
}
} // namespace Pedalboard