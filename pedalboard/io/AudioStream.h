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
  AudioStream(std::optional<std::string> inputDeviceName,
              std::optional<std::string> outputDeviceName,
              std::optional<std::shared_ptr<Chain>> pedalboard,
              std::optional<double> sampleRate, std::optional<int> bufferSize,
              bool allowFeedback, int numInputChannels, int numOutputChannels)
      : pedalboard(pedalboard ? *pedalboard
                              : std::make_shared<Chain>(
                                    std::vector<std::shared_ptr<Plugin>>())),
        livePedalboard(std::vector<std::shared_ptr<Plugin>>()) {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    if (inputDeviceName) {
      setup.inputDeviceName = *inputDeviceName;
    } else {
      numInputChannels = 0;
    }

    if (outputDeviceName) {
      setup.outputDeviceName = *outputDeviceName;
    } else {
      numOutputChannels = 0;
    }

    // A value of 0 indicates we want to use whatever the device default is:
    setup.sampleRate = sampleRate ? *sampleRate : 0;

    // This is our buffer between the audio hardware and the audio thread,
    // so this can be a lot smaller:
    setup.bufferSize = bufferSize ? *bufferSize : 512;

    if (inputDeviceName && outputDeviceName && !allowFeedback &&
        juce::String(*inputDeviceName).containsIgnoreCase("microphone") &&
        juce::String(*outputDeviceName).containsIgnoreCase("speaker")) {
      throw std::runtime_error(
          "The audio input device passed to AudioStream looks like a "
          "microphone, and the output device looks like a speaker. This setup "
          "may cause feedback. To create an AudioStream anyways, pass "
          "`allow_feedback=True` to the AudioStream constructor.");
    }

    if ((!inputDeviceName ||
         (inputDeviceName.has_value() && inputDeviceName.value().empty())) &&
        (!outputDeviceName ||
         (outputDeviceName.has_value() && outputDeviceName.value().empty()))) {
      throw std::runtime_error("At least one of `input_device_name` or "
                               "`output_device_name` must be provided.");
    }

    if (inputDeviceName) {
      recordBufferFifo =
          std::make_unique<juce::AbstractFifo>(setup.bufferSize + 1);
      recordBuffer = std::make_unique<juce::AudioBuffer<float>>(
          numInputChannels, setup.bufferSize + 1);
    }

    if (outputDeviceName) {
      playBufferFifo =
          std::make_unique<juce::AbstractFifo>(setup.bufferSize + 1);
      playBuffer = std::make_unique<juce::AudioBuffer<float>>(
          numOutputChannels, setup.bufferSize + 1);
    }

    juce::String defaultDeviceName;
    juce::String error =
        deviceManager.initialise(numInputChannels, numOutputChannels, nullptr,
                                 true, defaultDeviceName, &setup);
    if (!error.isEmpty()) {
      throw std::domain_error(error.toStdString());
    }

    // We'll re-open the audio device when we need it
    deviceManager.closeAudioDevice();
#else
    throw std::runtime_error("AudioStream is not supported on this platform.");
#endif
  }

  ~AudioStream() {
    stop();
    close();
  }

  std::shared_ptr<Chain> getPedalboard() { return pedalboard; }

  void setPedalboard(std::shared_ptr<Chain> chain) { pedalboard = chain; }

  void close() {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    deviceManager.closeAudioDevice();
#endif
  }

  void start() {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    if (isRunning) {
      throw std::runtime_error("This AudioStream is already running.");
    }

    deviceManager.restartLastAudioDevice();

    isRunning = true;
    numDroppedInputFrames = 0;
    changeObserverThread =
        std::thread(&AudioStream::propagateChangesToAudioThread, this);
    deviceManager.addAudioCallback(this);
#endif
  }

  void stop() {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    deviceManager.removeAudioCallback(this);
    isRunning = false;
    if (changeObserverThread.joinable()) {
      changeObserverThread.join();
    }
    if (recordBufferFifo) {
      recordBufferFifo->reset();
    }
    if (playBufferFifo) {
      playBufferFifo->reset();
    }
    close();
#endif
  }

  void propagateChangesToAudioThread() {
    while (isRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));

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
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    if (!getNumInputChannels() || !getNumOutputChannels())
      throw std::runtime_error(
          "This AudioStream object was not created with both an input and an "
          "output device, so calling run() would do nothing.");

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
#else
    throw std::runtime_error("AudioStream is not supported on this platform.");
#endif
  }

  std::shared_ptr<AudioStream> enter() {
    start();
    return shared_from_this();
  }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    bool shouldThrow = PythonException::isPending();
    stop();
    close();

    if (shouldThrow || PythonException::isPending())
      throw py::error_already_set();
  }

  static std::vector<std::string> getDeviceNames(bool isInput) {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    juce::AudioDeviceManager deviceManager;

    std::vector<std::string> names;
    for (auto *type : deviceManager.getAvailableDeviceTypes()) {
      for (juce::String name : type->getDeviceNames(isInput)) {
        names.push_back(name.toStdString());
      }
    }
    return names;
#else
    return {};
#endif
  }

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
  template <typename Setup> static auto getSetupInfo(Setup &s, bool isInput) {
    struct SetupInfo {
      // double brackets so that we get the expression type, i.e. a (possibly
      // const) reference
      decltype((s.inputDeviceName)) name;
      decltype((s.inputChannels)) channels;
      decltype((s.useDefaultInputChannels)) useDefault;
    };

    return isInput ? SetupInfo{s.inputDeviceName, s.inputChannels,
                               s.useDefaultInputChannels}
                   : SetupInfo{s.outputDeviceName, s.outputChannels,
                               s.useDefaultOutputChannels};
  }
#endif

  static std::optional<std::string>
  getDefaultDeviceName(bool isInput, int numChannelsNeeded) {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    juce::AudioDeviceManager deviceManager;
    deviceManager.getAvailableDeviceTypes();

    juce::AudioDeviceManager::AudioDeviceSetup setup;

    if (auto *type = deviceManager.getCurrentDeviceTypeObject()) {
      const auto info = getSetupInfo(setup, isInput);

      if (numChannelsNeeded > 0 && info.name.isEmpty()) {
        std::string deviceName =
            type->getDeviceNames(isInput)[type->getDefaultDeviceIndex(isInput)]
                .toStdString();
        if (!deviceName.empty()) {
          return {deviceName};
        }
      }
    }
#endif
    return {};
  }

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
  virtual void audioDeviceIOCallback(const float **inputChannelData,
                                     int numInputChannels,
                                     float **outputChannelData,
                                     int numOutputChannels, int numSamples) {
    // Live processing mode: run the input audio through a Pedalboard object.
    if (playBufferFifo && recordBufferFifo) {
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
          std::unique_lock<std::mutex> lock(pedalboard->mutex,
                                            std::try_to_lock);
          // If someone's running audio through this plugin in parallel
          // (offline, or in a different AudioStream object) then don't corrupt
          // its state by calling it here too; instead, just skip it:
          if (lock.owns_lock()) {
            plugin->process(context);
          }
        }
      }
    } else if (recordBufferFifo) {
      // If Python wants audio input, then copy the audio into the record
      // buffer:
      for (int attempt = 0; attempt < 2; attempt++) {
        int samplesWritten = 0;
        {
          const auto scope = recordBufferFifo->write(numSamples);

          if (scope.blockSize1 > 0)
            for (int i = 0; i < numInputChannels; i++) {
              std::memcpy(
                  (char *)recordBuffer->getWritePointer(i, scope.startIndex1),
                  (char *)inputChannelData[i],
                  scope.blockSize1 * sizeof(float));
            }

          if (scope.blockSize2 > 0)
            for (int i = 0; i < numInputChannels; i++) {
              std::memcpy(
                  (char *)recordBuffer->getWritePointer(i, scope.startIndex2),
                  (char *)(inputChannelData[i] + scope.blockSize1),
                  scope.blockSize2 * sizeof(float));
            }

          samplesWritten = scope.blockSize1 + scope.blockSize2;
        }

        if (samplesWritten < numSamples) {
          numDroppedInputFrames += numSamples - samplesWritten;
          // We've dropped some frames during recording; not great.
          // Favour capturing more recent audio instead, so clear out enough
          // space in the buffer to keep up and retry:
          const auto scope = recordBufferFifo->read(numSamples);

          // Do nothing here; as soon as `scope` goes out of scope,
          // the buffer will now allow us to append `numSamples`.
        } else {
          break;
        }
      }
    } else if (playBufferFifo) {
      for (int i = 0; i < numOutputChannels; i++) {
        std::memset((char *)outputChannelData[i], 0,
                    numSamples * sizeof(float));
      }

      const auto scope = playBufferFifo->read(numSamples);

      if (scope.blockSize1 > 0)
        for (int i = 0; i < numOutputChannels; i++) {
          std::memcpy((char *)outputChannelData[i],
                      (char *)playBuffer->getReadPointer(i, scope.startIndex1),
                      scope.blockSize1 * sizeof(float));
        }

      if (scope.blockSize2 > 0)
        for (int i = 0; i < numOutputChannels; i++) {
          std::memcpy((char *)(outputChannelData[i] + scope.blockSize1),
                      (char *)playBuffer->getReadPointer(i, scope.startIndex2),
                      scope.blockSize2 * sizeof(float));
        }
    } else {
      for (int i = 0; i < numOutputChannels; i++) {
        std::memset((char *)outputChannelData[i], 0,
                    numSamples * sizeof(float));
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
#endif

  bool getIsRunning() const { return isRunning; }

  int getDroppedInputFrameCount() const { return numDroppedInputFrames; }

  bool getIgnoreDroppedInput() const { return ignoreDroppedInput; }

  void setIgnoreDroppedInput(bool ignore) { ignoreDroppedInput = ignore; }

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
  juce::AudioDeviceManager::AudioDeviceSetup getAudioDeviceSetup() const {
    return deviceManager.getAudioDeviceSetup();
  }
#endif

  int writeIntoBuffer(const juce::AbstractFifo::ScopedWrite &scope, int offset,
                      juce::AudioBuffer<float> &from,
                      juce::AudioBuffer<float> &to) {
    bool inputIsMono = from.getNumChannels() == 1;
    int numChannels = inputIsMono ? to.getNumChannels() : from.getNumChannels();

    if (scope.blockSize1 > 0) {
      for (int c = 0; c < numChannels; c++) {
        to.copyFrom(c, scope.startIndex1, from, inputIsMono ? 0 : c, offset,
                    scope.blockSize1);
      }
    }

    if (scope.blockSize2 > 0) {
      for (int c = 0; c < numChannels; c++) {
        to.copyFrom(c, scope.startIndex2, from, inputIsMono ? 0 : c,
                    offset + scope.blockSize1, scope.blockSize2);
      }
    }
    return scope.blockSize1 + scope.blockSize2;
  }

  void writeAllAtOnce(juce::AudioBuffer<float> buffer) {
    if (!playBufferFifo) {
      throw std::runtime_error(
          "This AudioStream object was not created with an output device, so "
          "it cannot write audio data.");
    }

    if (isRunning) {
      throw std::runtime_error(
          "writeAllAtOnce() called when the stream is already running. "
          "This is an internal Pedalboard error and should be reported.");
    }

    start();
    // Write this audio buffer to the output device, via the FIFO, checking
    // for Python signals along the way (as this method is usually called
    // by passing the audio all at once from Python):

    bool errorSet = false;
    for (int i = 0; i < buffer.getNumSamples();) {
      errorSet = PyErr_CheckSignals() != 0;
      if (errorSet) {
        break;
      }

      py::gil_scoped_release release;

      int numSamplesToWrite =
          std::min(playBufferFifo->getFreeSpace(), buffer.getNumSamples() - i);

      const auto scope = playBufferFifo->write(numSamplesToWrite);
      i += writeIntoBuffer(scope, i, buffer, *playBuffer);
    }

    stop();

    if (errorSet) {
      throw py::error_already_set();
    }
  }

  int getNumInputChannels() const {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    return deviceManager.getAudioDeviceSetup()
        .inputChannels.countNumberOfSetBits();
#else
    return 0;
#endif
  }

  int getNumOutputChannels() const {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    return deviceManager.getAudioDeviceSetup()
        .outputChannels.countNumberOfSetBits();
#else
    return 0;
#endif
  }

  void write(juce::AudioBuffer<float> buffer) {
    if (!playBufferFifo) {
      throw std::runtime_error(
          "This AudioStream object was not created with an output device, so "
          "it cannot write audio data.");
    }

    if (!isRunning) {
      writeAllAtOnce(buffer);
      return;
    }

    py::gil_scoped_release release;

    // Write this audio buffer to the output device, via the FIFO:
    for (int i = 0; i < buffer.getNumSamples();) {
      int numSamplesToWrite =
          std::min(playBufferFifo->getFreeSpace(), buffer.getNumSamples() - i);

      const auto scope = playBufferFifo->write(numSamplesToWrite);
      i += writeIntoBuffer(scope, i, buffer, *playBuffer);
    }
  }

  juce::AudioBuffer<float> readAllAtOnce(int numSamples) {
    // Read this number of samples from the audio device, via its FIFO:
    if (!recordBufferFifo) {
      throw std::runtime_error(
          "This AudioStream object was not created with an input device, so "
          "it cannot read audio data.");
    }

    if (isRunning) {
      throw std::runtime_error(
          "readAllAtOnce() called when the stream is already running. "
          "This is an internal Pedalboard error and should be reported.");
    }

    juce::AudioBuffer<float> returnBuffer(getNumInputChannels(), numSamples);

    start();
    bool errorSet = false;
    // Read the desired number of samples from the output device, via the FIFO:
    for (int i = 0; i < returnBuffer.getNumSamples();) {
      errorSet = PyErr_CheckSignals() != 0;
      if (errorSet) {
        break;
      }

      py::gil_scoped_release release;

      int numSamplesToRead = std::min(recordBufferFifo->getNumReady(),
                                      returnBuffer.getNumSamples() - i);

      const auto scope = recordBufferFifo->read(numSamplesToRead);
      if (scope.blockSize1 > 0) {
        for (int c = 0; c < returnBuffer.getNumChannels(); c++) {
          returnBuffer.copyFrom(c, i, *recordBuffer, c, scope.startIndex1,
                                scope.blockSize1);
        }
      }

      if (scope.blockSize2 > 0) {
        for (int c = 0; c < returnBuffer.getNumChannels(); c++) {
          returnBuffer.copyFrom(c, i + scope.blockSize1, *recordBuffer, c,
                                scope.startIndex2, scope.blockSize2);
        }
      }

      i += scope.blockSize1 + scope.blockSize2;
    }
    stop();

    numDroppedInputFrames = 0;

    // Always return the buffer, even if an error was set:
    if (errorSet) {
      throw py::error_already_set();
    }
    return returnBuffer;
  }

  juce::AudioBuffer<float> read(int numSamples) {
    // Read this number of samples from the audio device, via its FIFO:
    if (!recordBufferFifo) {
      throw std::runtime_error(
          "This AudioStream object was not created with an input device, so "
          "it cannot read audio data.");
    }

    if (!isRunning) {
      return readAllAtOnce(numSamples);
    }

    if (numSamples == 0) {
      numSamples = recordBufferFifo->getNumReady();
    }

    if (numDroppedInputFrames && !ignoreDroppedInput) {
      int numDropped = numDroppedInputFrames;
      numDroppedInputFrames = 0;
      throw std::runtime_error(
          std::to_string(numDropped) +
          " frames of audio data were dropped since the last call to read(). "
          "To prevent audio from being lost during recording, ensure that you "
          "call read() as quickly as possible or increase your buffer size.");
    }

    py::gil_scoped_release release;

    juce::AudioBuffer<float> returnBuffer(getNumInputChannels(), numSamples);

    // Read the desired number of samples from the output device, via the FIFO:
    for (int i = 0; i < returnBuffer.getNumSamples();) {
      int numSamplesToRead = std::min(recordBufferFifo->getNumReady(),
                                      returnBuffer.getNumSamples() - i);

      const auto scope = recordBufferFifo->read(numSamplesToRead);
      if (scope.blockSize1 > 0) {
        for (int c = 0; c < returnBuffer.getNumChannels(); c++) {
          returnBuffer.copyFrom(c, i, *recordBuffer, c, scope.startIndex1,
                                scope.blockSize1);
        }
      }

      if (scope.blockSize2 > 0) {
        for (int c = 0; c < returnBuffer.getNumChannels(); c++) {
          returnBuffer.copyFrom(c, i + scope.blockSize1, *recordBuffer, c,
                                scope.startIndex2, scope.blockSize2);
        }
      }

      i += scope.blockSize1 + scope.blockSize2;
    }

    numDroppedInputFrames = 0;

    return returnBuffer;
  }

  double getSampleRate() const {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
    return deviceManager.getAudioDeviceSetup().sampleRate;
#else
    return 0;
#endif
  }

  std::optional<int> getNumBufferedInputFrames() const {
    if (!recordBufferFifo) {
      return {};
    }
    return {recordBufferFifo->getNumReady()};
  }

private:
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
  juce::AudioDeviceManager deviceManager;
#endif
  juce::dsp::ProcessSpec spec;
  bool isRunning = false;

  std::shared_ptr<Chain> pedalboard;

  // A simple spin-lock mutex that we can try to acquire from
  // the audio thread, allowing us to avoid modifying state that's
  // currently being used for rendering.
  // The `livePedalboard` object already has a std::mutex on it,
  // but we want something that's faster and callable from the
  // audio thread.
  juce::SpinLock livePedalboardMutex;

  // A "live" pedalboard, called from the audio thread.
  // This is not exposed to Python, and is only updated from C++.
  Chain livePedalboard;

  // A background thread, independent of the audio thread, that
  // watches for any changes to the Pedalboard object (which may
  // happen in Python) and copies those changes over to data
  // structures used by the audio thread.
  std::thread changeObserverThread;

  // Two buffers that can be written to by the audio thread:
  std::unique_ptr<juce::AbstractFifo> recordBufferFifo;
  std::unique_ptr<juce::AbstractFifo> playBufferFifo;
  std::unique_ptr<juce::AudioBuffer<float>> recordBuffer;
  std::unique_ptr<juce::AudioBuffer<float>> playBuffer;

  std::atomic<long long> numDroppedInputFrames{0};
  bool ignoreDroppedInput = false;
};

inline void init_audio_stream(py::module &m) {
  py::class_<AudioStream, std::shared_ptr<AudioStream>>(m, "AudioStream",
                                                        R"(
A class that allows interacting with live streams of audio from an input
audio device (i.e.: a microphone, audio interface, etc) and/or to an
output device (speaker, headphones), allowing access to the audio
stream from within Python code.

Use :py:meth:`AudioStream.play` to play audio data to your speakers::

    # Play a 10-second chunk of an audio file:
    with AudioFile("my_audio_file.mp3") as f:
        chunk = f.read(f.samplerate * 10)
    AudioStream.play(chunk, f.samplerate)

Or use :py:meth:`AudioStream.write` to stream audio in chunks::

    # Play an audio file by looping through it in chunks:
    with AudioStream(output_device="default") as stream:
        with AudioFile("my_audio_file.mp3") as f:
            while f.tell() < f.frames:
                # Decode and play 512 samples at a time:
                stream.write(f.read(512))

:class:`AudioStream` may also be used to pass live audio through a :class:`Pedalboard`::

   # Pass both an input and output device name to connect both ends:
   input_device_name = AudioStream.default_input_device_name
   output_device_name = AudioStream.default_output_device_name
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

   # Or use AudioStream synchronously:
   stream = AudioStream(input_device_name, output_device_name)
   stream.plugins.append(Reverb(wet_level=1.0))
   stream.run()  # Run the stream until Ctrl-C is received

.. warning::
    The :class:`AudioStream` class implements a context manager interface
    to ensure that audio streams are never left "dangling" (i.e.: running in
    the background without being stopped).
    
    While it is possible to call the :meth:`__enter__` method directly to run an
    audio stream in the background, this can have some nasty side effects. If the
    :class:`AudioStream` object is no longer reachable (not bound to a variable,
    not in scope, etc), the audio stream will continue to run forever, and
    won't stop until the Python interpreter exits.

    To run an :class:`AudioStream` in the background, use Python's
    :py:mod:`threading` module to run the stream object method on a
    background thread, allowing for easier cleanup.

*Introduced in v0.7.0 for macOS and Windows. Linux support introduced in v0.9.14.*

:py:meth:`read` *and* :py:meth:`write` *methods introduced in v0.9.12.*
)")
      .def(py::init([](std::optional<std::string> inputDeviceName,
                       std::optional<std::string> outputDeviceName,
                       std::optional<std::shared_ptr<Chain>> pedalboard,
                       std::optional<double> sampleRate,
                       std::optional<int> bufferSize, bool allowFeedback,
                       int numInputChannels, int numOutputChannels) {
             return std::make_shared<AudioStream>(
                 inputDeviceName, outputDeviceName, pedalboard, sampleRate,
                 bufferSize, allowFeedback, numInputChannels,
                 numOutputChannels);
           }),
           py::arg("input_device_name") = py::none(),
           py::arg("output_device_name") = py::none(),
           py::arg("plugins") = py::none(), py::arg("sample_rate") = py::none(),
           py::arg("buffer_size") = py::none(),
           py::arg("allow_feedback") = false, py::arg("num_input_channels") = 1,
           py::arg("num_output_channels") = 2)
      .def("run", &AudioStream::stream,
           "Start streaming audio from input to output, passing the audio "
           "stream through the :py:attr:`plugins` on this AudioStream object. "
           "This call will block the current thread until a "
           ":py:exc:`KeyboardInterrupt` (``Ctrl-C``) is received.")
      .def_property_readonly("running", &AudioStream::getIsRunning,
                             ":py:const:`True` if this stream is currently "
                             "streaming live "
                             "audio, :py:const:`False` otherwise.")
      .def("__enter__", &AudioStream::enter,
           "Use this :class:`AudioStream` as a context manager. Entering the "
           "context manager will immediately start the audio stream, sending "
           "audio through to the output device.")
      .def("__exit__", &AudioStream::exit,
           "Exit the context manager, ending the audio stream. Once called, "
           "the audio stream will be stopped (i.e.: :py:attr:`running` will "
           "be "
           ":py:const:`False`).")
      .def("__repr__",
           [](const AudioStream &stream) {
             std::ostringstream ss;
             ss << "<pedalboard.io.AudioStream";
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
             auto audioDeviceSetup = stream.getAudioDeviceSetup();

             if (stream.getNumInputChannels() > 0) {
               ss << " input_device_name=\""
                  << audioDeviceSetup.inputDeviceName.toStdString() << "\"";
             } else {
               ss << " input_device_name=None";
             }

             if (stream.getNumOutputChannels() > 0) {
               ss << " output_device_name=\""
                  << audioDeviceSetup.outputDeviceName.toStdString() << "\"";
             } else {
               ss << " output_device_name=None";
             }

             ss << " sample_rate="
                << juce::String(audioDeviceSetup.sampleRate, 2).toStdString();
             ss << " buffer_size=" << audioDeviceSetup.bufferSize;
             if (stream.getIsRunning()) {
               ss << " running";
             } else {
               ss << " not running";
             }
#endif
             ss << " at " << &stream;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "buffer_size",
          [](AudioStream &stream) {
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
            return stream.getAudioDeviceSetup().bufferSize;
#else
                                 return 0;
#endif
          },
          "The size (in frames) of the buffer used between the audio "
          "hardware "
          "and Python.")
      .def_property("plugins", &AudioStream::getPedalboard,
                    &AudioStream::setPedalboard,
                    "The Pedalboard object that this AudioStream will use to "
                    "process audio.")
      .def_property_readonly(
          "dropped_input_frame_count", &AudioStream::getDroppedInputFrameCount,
          "The number of frames of audio that were dropped since the last "
          "call "
          "to :py:meth:`read`. To prevent audio from being dropped during "
          "recording, ensure that you call :py:meth:`read` as often as "
          "possible or increase your buffer size.")
      .def_property_readonly(
          "sample_rate", &AudioStream::getSampleRate,
          "The sample rate that this stream is operating at.")
      .def_property_readonly("num_input_channels",
                             &AudioStream::getNumInputChannels,
                             "The number of input channels on the input "
                             "device. Will be ``0`` if "
                             "no input device is connected.")
      .def_property_readonly(
          "num_output_channels", &AudioStream::getNumOutputChannels,
          "The number of output channels on the output "
          "device. Will be ``0`` if no output device is connected.")
      .def_property(
          "ignore_dropped_input", &AudioStream::getIgnoreDroppedInput,
          &AudioStream::setIgnoreDroppedInput,
          "Controls the behavior of the :py:meth:`read` method when audio "
          "data "
          "is dropped. If this property is false (the default), the "
          ":py:meth:`read` method will raise a :py:exc:`RuntimeError` if any "
          "audio data is dropped in between calls. If this property is true, "
          "the :py:meth:`read` method will return the most recent audio data "
          "available, even if some audio data was dropped.\n\n.. note::\n    "
          "The :py:attr:`dropped_input_frame_count` property is unaffected "
          "by "
          "this setting.")
      .def(
          "write",
          [](AudioStream &stream,
             const py::array_t<float, py::array::c_style> inputArray,
             float sampleRate) {
            if (sampleRate != stream.getSampleRate()) {
              throw std::runtime_error(
                  "The sample rate provided to `write` (" +
                  std::to_string(sampleRate) +
                  " Hz) does not match the output device's sample rate (" +
                  std::to_string(stream.getSampleRate()) +
                  " Hz). To write audio data to the output device, the "
                  "sample "
                  "rate of the audio must match the output device's sample "
                  "rate.");
            }
            stream.write(copyPyArrayIntoJuceBuffer(inputArray));
          },
          py::arg("audio"), py::arg("sample_rate"),
          "Write (play) audio data to the output device. This method will "
          "block until the provided audio buffer is played in its "
          "entirety.\n\nIf the provided sample rate does not match the "
          "output "
          "device's sample rate, an error will be thrown. In this case, you "
          "can use :py:class:`StreamResampler` to resample the audio before "
          "calling :py:meth:`write`.")
      .def(
          "read",
          [](AudioStream &stream, int numSamples) {
            return copyJuceBufferIntoPyArray(stream.read(numSamples),
                                             ChannelLayout::NotInterleaved, 0);
          },
          py::arg("num_samples") = 0,
          "Read (record) audio data from the input device. When called with "
          "no "
          "arguments, this method returns all of the available audio data in "
          "the buffer. If `num_samples` is provided, this method will block "
          "until the desired number of samples have been received. The audio "
          "is recorded at the :py:attr:`sample_rate` of this stream.\n\n.. "
          "warning::\n    Recording audio is a **real-time** operation, so "
          "if "
          "your code doesn't call :py:meth:`read` quickly enough, some audio "
          "will be lost. To warn about this, :py:meth:`read` will throw an "
          "exception if audio data is dropped. This behavior can be disabled "
          "by setting :py:attr:`ignore_dropped_input` to :py:const:`True`. "
          "The "
          "number of dropped samples since the last call to :py:meth:`read` "
          "can be retrieved by accessing the "
          ":py:attr:`dropped_input_frame_count` property.")
      .def_property_readonly(
          "buffered_input_sample_count",
          &AudioStream::getNumBufferedInputFrames,
          "The number of frames of audio that are currently in the input "
          "buffer. This number will change rapidly, and will be "
          ":py:const:`None` if no input device is connected.")
      .def("close", &AudioStream::close,
           "Close the audio stream, stopping the audio device and releasing "
           "any resources. After calling close, this AudioStream object is no "
           "longer usable.")
      .def_static(
          "play",
          [](const py::array_t<float, py::array::c_style> audio,
             float sampleRate, std::optional<std::string> outputDeviceName) {
            juce::AudioBuffer<float> buffer = copyPyArrayIntoJuceBuffer(audio);
            AudioStream(std::nullopt, outputDeviceName ? *outputDeviceName : "",
                        std::nullopt, {sampleRate}, std::nullopt, false, 0,
                        buffer.getNumChannels())
                .write(buffer);
          },
          py::arg("audio"), py::arg("sample_rate"),
          py::arg("output_device_name") = py::none(),
          "Play audio data to the speaker, headphones, or other output "
          "device. "
          "This method will block until the audio is finished playing.")
      .def_property_readonly_static(
          "input_device_names",
          [](py::object *obj) -> std::vector<std::string> {
            return AudioStream::getDeviceNames(true);
          },
          "The input devices (i.e.: microphones, audio interfaces, etc.) "
          "currently available on the current machine.")
      .def_property_readonly_static(
          "output_device_names",
          [](py::object *obj) -> std::vector<std::string> {
            return AudioStream::getDeviceNames(false);
          },
          "The output devices (i.e.: speakers, headphones, etc.) currently "
          "available on the current machine.")
      .def_property_readonly_static(
          "default_input_device_name",
          [](py::object *obj) -> std::optional<std::string> {
            return AudioStream::getDefaultDeviceName(true, 1);
          },
          "The name of the default input device (i.e.: microphone, audio "
          "interface, etc.) currently available on the current machine. May "
          "be :py:const:`None` if no input devices are present.")
      .def_property_readonly_static(
          "default_output_device_name",
          [](py::object *obj) -> std::optional<std::string> {
            return AudioStream::getDefaultDeviceName(false, 2);
          },
          "The name of the default output device (i.e.: speakers, "
          "headphones, etc.) currently available on the current machine. May "
          "be :py:const:`None` if no output devices are present.");
}
} // namespace Pedalboard