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
#include "ReadableAudioFile.h"
#include "StreamResampler.h"

namespace py = pybind11;

namespace Pedalboard {

static inline int inputBufferSizeFor(ResamplingQuality quality) {
  switch (quality) {
  case ResamplingQuality::ZeroOrderHold:
    return 1;
  case ResamplingQuality::Linear:
    return 2;
  case ResamplingQuality::CatmullRom:
    return 4;
  case ResamplingQuality::Lagrange:
    return 5;
  case ResamplingQuality::WindowedSinc:
    return 200;
  case ResamplingQuality::WindowedSinc256:
    return 256 * 4;
  case ResamplingQuality::WindowedSinc128:
    return 128 * 4;
  case ResamplingQuality::WindowedSinc64:
    return 64 * 4;
  case ResamplingQuality::WindowedSinc32:
    return 32 * 4;
  case ResamplingQuality::WindowedSinc16:
    return 16 * 4;
  case ResamplingQuality::WindowedSinc8:
    return 8 * 4;
  default:
    throw std::runtime_error("Unknown resampling quality (" +
                             std::to_string((int)quality) +
                             "); this is an internal "
                             "Pedalboard error and should be reported.");
  }
  return 0;
}

class ResampledReadableAudioFile
    : public AbstractReadableAudioFile,
      public std::enable_shared_from_this<ResampledReadableAudioFile> {
public:
  ResampledReadableAudioFile(std::shared_ptr<AbstractReadableAudioFile> audioFile,
                             float targetSampleRate, ResamplingQuality quality)
      : audioFile(audioFile),
        resampler(audioFile->getSampleRateAsDouble(), targetSampleRate,
                  audioFile->getNumChannels(), quality) {}

  std::variant<double, long> getSampleRate() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);

    double integerPart;
    double fractionalPart =
        std::modf(resampler.getTargetSampleRate(), &integerPart);

    if (fractionalPart > 0) {
      return resampler.getTargetSampleRate();
    } else {
      return (long)(resampler.getTargetSampleRate());
    }
  }

  double getSampleRateAsDouble() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return resampler.getTargetSampleRate();
  }

  long long getLengthInSamples() const override {
    double underlyingLengthInSamples = (double)audioFile->getLengthInSamples();
    double underlyingSampleRate = audioFile->getSampleRateAsDouble();

    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    double length =
        ((underlyingLengthInSamples * resampler.getTargetSampleRate()) /
         underlyingSampleRate);
    if (resampler.getOutputLatency() > 0) {
      length -= (std::round(resampler.getOutputLatency()) -
                 resampler.getOutputLatency());
    }
    return (long long)length;
  }

  double getDuration() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getDuration();
  }

  long getNumChannels() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getNumChannels();
  }

  bool exactDurationKnown() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->exactDurationKnown();
  }

  std::string getFileFormat() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getFileFormat();
  }

  std::string getFileDatatype() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getFileDatatype();
  }

  ResamplingQuality getQuality() const {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return resampler.getQuality();
  }

  py::array_t<float> read(std::variant<double, long long> numSamplesVariant) override {
    long long numSamples = parseNumSamples(numSamplesVariant);
    if (numSamples == 0)
      throw std::domain_error(
          "ResampledReadableAudioFile will not read an entire file at once, "
          "due to the possibility that a file may be larger than available "
          "memory. Please pass a number of frames to read (available from "
          "the 'frames' attribute).");

    juce::AudioBuffer<float> resampledBuffer;
    {
      py::gil_scoped_release release;
      resampledBuffer = readInternal(numSamples);
    }

    PythonException::raise();
    return copyJuceBufferIntoPyArray(resampledBuffer,
                                     ChannelLayout::NotInterleaved, 0);
  }

  /**
   * Read samples from the underlying audio file, resample them, and return a
   * juce::AudioBuffer containing the result without holding the GIL.
   *
   * @param numSamples The number of samples to read.
   * @return juce::AudioBuffer<float> The resulting audio.
   */
  juce::AudioBuffer<float> readInternal(long long numSamples) {
    // Note: We take a "write" lock here as calling readInternal will
    // advance internal state:
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile. Note "
          "that using multiple concurrent readers on the same AudioFile "
          "object will produce nondeterministic results.");
    }

    long long samplesInResampledBuffer = 0;
    juce::AudioBuffer<float> resampledBuffer(audioFile->getNumChannels(),
                                             numSamples);

    // Any samples in the existing outputBuffer from last time
    // should be copied into resampledBuffer:
    int samplesToPullFromOutputBuffer =
        std::min(outputBuffer.getNumSamples(), (int)numSamples);
    if (samplesToPullFromOutputBuffer > 0) {
      for (int c = 0; c < resampledBuffer.getNumChannels(); c++) {
        resampledBuffer.copyFrom(c, 0, outputBuffer, c, 0,
                                 samplesToPullFromOutputBuffer);
      }
      samplesInResampledBuffer += samplesToPullFromOutputBuffer;

      // Remove the used samples from outputBuffer:
      if (outputBuffer.getNumSamples() - samplesToPullFromOutputBuffer) {
        for (int c = 0; c < resampledBuffer.getNumChannels(); c++) {
          // Use std::memmove instead of copyFrom here, as copyFrom is not
          // overlap-safe.
          std::memmove(
              outputBuffer.getWritePointer(c),
              outputBuffer.getReadPointer(c, samplesToPullFromOutputBuffer),
              (outputBuffer.getNumSamples() - samplesToPullFromOutputBuffer) *
                  sizeof(float));
        }
      }
      outputBuffer.setSize(outputBuffer.getNumChannels(),
                           outputBuffer.getNumSamples() -
                               samplesToPullFromOutputBuffer,
                           /* keepExistingContent */ true);
    }

    long long inputSamplesRequired =
        (long long)(((numSamples - samplesToPullFromOutputBuffer) *
                     audioFile->getSampleRateAsDouble()) /
                    resampler.getTargetSampleRate());

    // Make a juce::AudioBuffer that contains contiguous memory,
    // which we can pass to readInternal:
    std::vector<float> contiguousSourceSampleBuffer;
    std::vector<float *> contiguousSourceSampleBufferPointers =
        std::vector<float *>(audioFile->getNumChannels());

    while (samplesInResampledBuffer < numSamples) {
      // Cut or expand the contiguousSourceSampleBuffer to the required size:
      contiguousSourceSampleBuffer.resize(
          // Note: we need at least one element in this buffer or else we'll
          // have no channel pointers to pass into the juce::AudioFile
          // constructor!
          std::max(1LL, audioFile->getNumChannels() * inputSamplesRequired));
      std::fill_n(contiguousSourceSampleBuffer.begin(),
                  contiguousSourceSampleBuffer.size(), 0);
      for (int c = 0; c < audioFile->getNumChannels(); c++) {
        contiguousSourceSampleBufferPointers[c] =
            contiguousSourceSampleBuffer.data() + (c * inputSamplesRequired);
      }

      juce::AudioBuffer<float> sourceSamples = juce::AudioBuffer<float>(
          contiguousSourceSampleBufferPointers.data(),
          audioFile->getNumChannels(), inputSamplesRequired);
      std::optional<juce::AudioBuffer<float>> resamplerInput;

      if (inputSamplesRequired > 0) {
        // Read from the underlying audioFile using the public read() interface.
        // We need to acquire the GIL since read() creates Python objects.
        long long samplesRead = 0;
        {
          py::gil_scoped_acquire acquire;
          py::array_t<float> readResult = audioFile->read(inputSamplesRequired);
          py::buffer_info bufInfo = readResult.request();
          samplesRead = bufInfo.shape[1]; // shape is (channels, samples)

          // Copy data from the numpy array to our contiguous buffer
          float *srcPtr = static_cast<float *>(bufInfo.ptr);
          for (int c = 0; c < audioFile->getNumChannels(); c++) {
            for (long long i = 0; i < samplesRead; i++) {
              contiguousSourceSampleBuffer[c * inputSamplesRequired + i] =
                  srcPtr[c * samplesRead + i];
            }
          }
        }

        // Resize the sourceSamples buffer to the number of samples read,
        // without reallocating the memory underneath
        // (still points at contiguousSourceSampleBuffer):
        sourceSamples.setSize(audioFile->getNumChannels(), samplesRead,
                              /* keepExistingContent */ true,
                              /* clearExtraSpace */ false,
                              /* avoidReallocating */ true);

        if (samplesRead < inputSamplesRequired) {
          for (int c = 0; c < audioFile->getNumChannels(); c++) {
            contiguousSourceSampleBufferPointers[c] =
                contiguousSourceSampleBuffer.data() + (c * samplesRead);
          }
          sourceSamples = juce::AudioBuffer<float>(
              contiguousSourceSampleBufferPointers.data(),
              audioFile->getNumChannels(), samplesRead);
        }

        // If the underlying source ran out of samples, tell the resampler that
        // we're done by feeding in an empty optional rather than an empty
        // buffer:
        if (sourceSamples.getNumSamples() == 0) {
          resamplerInput = {};
        } else {
          resamplerInput = {sourceSamples};
        }
      } else {
        resamplerInput = {sourceSamples};
      }

      // TODO: Provide an alternative interface to write to the output buffer
      juce::AudioBuffer<float> newResampledSamples =
          resampler.process(resamplerInput);

      int offsetInNewResampledSamples = 0;
      if (newResampledSamples.getNumSamples() >
          numSamples - samplesInResampledBuffer) {
        int samplesToCache = newResampledSamples.getNumSamples() -
                             (numSamples - samplesInResampledBuffer);
        outputBuffer.setSize(newResampledSamples.getNumChannels(),
                             samplesToCache);
        for (int c = 0; c < outputBuffer.getNumChannels(); c++) {
          outputBuffer.copyFrom(c, 0, newResampledSamples, c,
                                newResampledSamples.getNumSamples() -
                                    samplesToCache,
                                samplesToCache);
        }
      }

      if (!resamplerInput && newResampledSamples.getNumSamples() == 0) {
        resampledBuffer.setSize(resampledBuffer.getNumChannels(),
                                samplesInResampledBuffer,
                                /* keepExistingContent */ true);
        break;
      }

      int startSample = samplesInResampledBuffer;
      int samplesToCopy = std::min(
          newResampledSamples.getNumSamples() - offsetInNewResampledSamples,
          (int)(numSamples - samplesInResampledBuffer));
      if (samplesToCopy > 0) {
        for (int c = 0; c < resampledBuffer.getNumChannels(); c++) {
          resampledBuffer.copyFrom(c, startSample, newResampledSamples, c,
                                   offsetInNewResampledSamples, samplesToCopy);
        }
      }
      samplesInResampledBuffer += samplesToCopy;

      // TODO: Tune this carefully to avoid unnecessary calls to read()
      // Too large, and we buffer too much audio using too much memory
      // Too short, and we slow down
      inputSamplesRequired = 1;
    }
    positionInTargetSampleRate += resampledBuffer.getNumSamples();
    return resampledBuffer;
  }

  void seek(long long targetPosition) override {
    py::gil_scoped_release release;
    seekInternal(targetPosition);
    PythonException::raise();
  }

  void seekInternal(long long targetPosition) override {
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile. Note "
          "that using multiple concurrent readers on the same AudioFile "
          "object will produce nondeterministic results.");
    }
    long long positionToSeekToIncludingBuffers = targetPosition;

    long long targetPositionInSourceSampleRate =
        std::max(0LL, (long long)(((double)positionToSeekToIncludingBuffers *
                                   resampler.getSourceSampleRate()) /
                                  resampler.getTargetSampleRate()));

    targetPositionInSourceSampleRate -=
        inputBufferSizeFor(resampler.getQuality());

    long long maximumOverflow = (long long)std::ceil(
        resampler.getSourceSampleRate() / resampler.getTargetSampleRate());
    targetPositionInSourceSampleRate -= std::max(0LL, maximumOverflow);

    double floatingPositionInTargetSampleRate =
        std::max(0.0, ((double)targetPositionInSourceSampleRate *
                       resampler.getTargetSampleRate()) /
                          resampler.getSourceSampleRate());

    positionInTargetSampleRate =
        (long long)(floatingPositionInTargetSampleRate);

    resampler.reset();

    long long inputSamplesUsed =
        resampler.advanceResamplerState(positionInTargetSampleRate);
    targetPositionInSourceSampleRate = inputSamplesUsed;

    audioFile->seekInternal(std::max(0LL, targetPositionInSourceSampleRate));

    outputBuffer.setSize(0, 0);

    const long long chunkSize = 1024 * 1024;
    for (long long i = positionInTargetSampleRate; i < targetPosition;
         i += chunkSize) {
      long long numSamples = std::min(chunkSize, targetPosition - i);
      this->readInternal(numSamples);
    }
  }

  long long tell() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return positionInTargetSampleRate;
  }

  void close() override {
    py::gil_scoped_release release;
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile; it cannot "
          "be closed until the other thread completes its operation.");
    }
    _isClosed = true;
  }

  bool isClosed() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    if (audioFile->isClosed())
      return true;

    // ...but we do need a read lock here:
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return _isClosed;
  }

  bool isSeekable() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->isSeekable();
  }

  std::optional<std::string> getFilename() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getFilename();
  }

  PythonInputStream *getPythonInputStream() const override {
    // No need for a ScopedReadLock here, as audioFile is const:
    return audioFile->getPythonInputStream();
  }

  std::shared_ptr<AbstractReadableAudioFile> enter() override {
    return shared_from_this();
  }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) override {
    bool shouldThrow = PythonException::isPending();
    close();

    if (shouldThrow || PythonException::isPending())
      throw py::error_already_set();
  }

  std::string getClassName() const override {
    return "ResampledReadableAudioFile";
  }

private:
  const std::shared_ptr<AbstractReadableAudioFile> audioFile;
  StreamResampler<float> resampler;
  juce::AudioBuffer<float> outputBuffer;
  long long positionInTargetSampleRate = 0;
  juce::ReadWriteLock objectLock;
  bool _isClosed = false;
};

inline py::class_<ResampledReadableAudioFile, AbstractReadableAudioFile,
                  std::shared_ptr<ResampledReadableAudioFile>>
declare_resampled_readable_audio_file(py::module &m) {
  return py::class_<ResampledReadableAudioFile, AbstractReadableAudioFile,
                    std::shared_ptr<ResampledReadableAudioFile>>(
      m, "ResampledReadableAudioFile",
      R"(
A class that wraps an audio file for reading, while resampling
the audio stream on-the-fly to a new sample rate.

*Introduced in v0.6.0.*

Reading, seeking, and all other basic file I/O operations are supported (except for
:meth:`read_raw`).

:class:`ResampledReadableAudioFile` should usually
be used via the :meth:`resampled_to` method on :class:`ReadableAudioFile`:

::

   with AudioFile("my_file.mp3").resampled_to(22_050) as f:
       f.samplerate # => 22050
       first_ten_seconds = f.read(int(f.samplerate * 10))

Fractional (real-valued, non-integer) sample rates are supported.

Under the hood, :class:`ResampledReadableAudioFile` uses a stateful
:class:`StreamResampler` instance, which uses a constant amount of
memory to resample potentially-unbounded streams of audio. The audio
output by :class:`ResampledReadableAudioFile` will always be identical
to the result obtained by passing the entire audio file through a
:class:`StreamResampler`, with the added benefits of allowing chunked
reads, seeking through files, and using a constant amount of memory.
)");
}

inline void init_resampled_readable_audio_file(
    py::module &m, py::class_<ResampledReadableAudioFile, AbstractReadableAudioFile,
                              std::shared_ptr<ResampledReadableAudioFile>>
                       &pyResampledReadableAudioFile) {
  // Note: Most methods are inherited from AbstractReadableAudioFile.
  // We only define class-specific methods and override docstrings where needed.
  pyResampledReadableAudioFile
      .def(py::init(
               [](std::shared_ptr<AbstractReadableAudioFile> audioFile,
                  float targetSampleRate,
                  ResamplingQuality quality) -> ResampledReadableAudioFile * {
                 // This definition is only here to provide nice docstrings.
                 throw std::runtime_error(
                     "Internal error: __init__ should never be called, as this "
                     "class implements __new__.");
               }),
           py::arg("audio_file"), py::arg("target_sample_rate"),
           py::arg("resampling_quality") = ResamplingQuality::WindowedSinc32)
      .def_static(
          "__new__",
          [](const py::object *,
             std::shared_ptr<AbstractReadableAudioFile> audioFile,
             float targetSampleRate, ResamplingQuality quality) {
            return std::make_shared<ResampledReadableAudioFile>(
                audioFile, targetSampleRate, quality);
          },
          py::arg("cls"), py::arg("audio_file"), py::arg("target_sample_rate"),
          py::arg("resampling_quality") = ResamplingQuality::WindowedSinc32)
      .def_property_readonly(
          "resampling_quality", &ResampledReadableAudioFile::getQuality,
          "The resampling algorithm used to resample from the original file's "
          "sample rate to the ``target_sample_rate``.");
}
} // namespace Pedalboard
