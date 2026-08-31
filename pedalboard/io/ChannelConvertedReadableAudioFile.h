/*
 * pedalboard
 * Copyright 2026 Spotify AB
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
#include "ResampledReadableAudioFile.h"

namespace py = pybind11;

namespace Pedalboard {

/**
 * A wrapper class that converts audio channel counts on-the-fly.
 * Wraps any AbstractReadableAudioFile (ReadableAudioFile, ResampledReadableAudioFile, etc.)
 */
class ChannelConvertedReadableAudioFile
    : public AbstractReadableAudioFile,
      public std::enable_shared_from_this<ChannelConvertedReadableAudioFile> {
public:
  ChannelConvertedReadableAudioFile(
      std::shared_ptr<AbstractReadableAudioFile> audioFile, int targetNumChannels)
      : wrappedFile(audioFile), targetNumChannels(targetNumChannels) {
    if (targetNumChannels < 1) {
      throw std::domain_error("Target number of channels must be at least 1.");
    }

    int sourceNumChannels = wrappedFile->getNumChannels();

    // Only allow well-defined channel conversions:
    // - Any -> mono (average all channels)
    // - Mono -> any (duplicate mono to all channels)
    // - Same channel count (no conversion needed)
    // Disallow ambiguous conversions like stereo <-> multichannel
    if (sourceNumChannels != targetNumChannels && sourceNumChannels != 1 &&
        targetNumChannels != 1) {
      throw std::domain_error(
          "Channel conversion from " + std::to_string(sourceNumChannels) +
          " to " + std::to_string(targetNumChannels) +
          " channels is not supported. Only conversions to/from mono (1 "
          "channel) are well-defined. To convert to mono first, use "
          ".mono().with_channels(" +
          std::to_string(targetNumChannels) + ").");
    }
  }

  std::variant<double, long> getSampleRate() const override {
    return wrappedFile->getSampleRate();
  }

  double getSampleRateAsDouble() const override {
    return wrappedFile->getSampleRateAsDouble();
  }

  long long getLengthInSamples() const override {
    return wrappedFile->getLengthInSamples();
  }

  double getDuration() const override {
    return wrappedFile->getDuration();
  }

  long getNumChannels() const override { return targetNumChannels; }

  bool exactDurationKnown() const override {
    return wrappedFile->exactDurationKnown();
  }

  std::string getFileFormat() const override {
    return wrappedFile->getFileFormat();
  }

  std::string getFileDatatype() const override {
    return wrappedFile->getFileDatatype();
  }

  py::array_t<float> read(std::variant<double, long long> numSamplesVariant) override {
    long long numSamples = parseNumSamples(numSamplesVariant);
    if (numSamples == 0)
      throw std::domain_error(
          "ChannelConvertedReadableAudioFile will not read an entire file "
          "at once, "
          "due to the possibility that a file may be larger than available "
          "memory. Please pass a number of frames to read (available from "
          "the 'frames' attribute).");

    juce::AudioBuffer<float> convertedBuffer;
    {
      py::gil_scoped_release release;
      convertedBuffer = readInternal(numSamples);
    }

    PythonException::raise();
    return copyJuceBufferIntoPyArray(convertedBuffer,
                                     ChannelLayout::NotInterleaved, 0);
  }

  /**
   * Read samples from the underlying audio file, convert channels, and return a
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

    int sourceNumChannels = wrappedFile->getNumChannels();

    // Read from the underlying file and copy data while holding the GIL
    juce::AudioBuffer<float> outputBuffer;
    {
      py::gil_scoped_acquire acquire;
      py::array_t<float> sourceArray = wrappedFile->read(numSamples);

      auto sourceInfo = sourceArray.request();
      long long actualSamplesRead = sourceInfo.shape[1];

      if (actualSamplesRead == 0) {
        return juce::AudioBuffer<float>(targetNumChannels, 0);
      }

      // Create output buffer and copy data while we still have access to the array
      outputBuffer.setSize(targetNumChannels, actualSamplesRead);
      float *sourcePtr = static_cast<float *>(sourceInfo.ptr);
      copyChannelData(outputBuffer, sourcePtr, sourceNumChannels, actualSamplesRead);
    }

    return outputBuffer;
  }

  void copyChannelData(juce::AudioBuffer<float> &outputBuffer, float *sourcePtr,
                       int sourceNumChannels, long long actualSamplesRead) {
    // Note: The constructor validates that only well-defined conversions are
    // allowed (to/from mono, or same channel count), so we only need to handle
    // those cases here.

    if (targetNumChannels == sourceNumChannels) {
      // No conversion needed, just copy
      for (int c = 0; c < targetNumChannels; c++) {
        outputBuffer.copyFrom(c, 0, sourcePtr + (c * actualSamplesRead),
                              actualSamplesRead);
      }
    } else if (targetNumChannels == 1) {
      // Mix down to mono: average all channels (SIMD-optimized)
      float *outputPtr = outputBuffer.getWritePointer(0);
      float channelVolume = 1.0f / sourceNumChannels;

      // Start with first channel
      juce::FloatVectorOperations::copy(outputPtr, sourcePtr,
                                        (int)actualSamplesRead);

      // Add remaining channels
      for (int c = 1; c < sourceNumChannels; c++) {
        float *channelPtr = sourcePtr + (c * actualSamplesRead);
        juce::FloatVectorOperations::add(outputPtr, channelPtr,
                                         (int)actualSamplesRead);
      }

      // Apply volume scaling
      juce::FloatVectorOperations::multiply(outputPtr, channelVolume,
                                            (int)actualSamplesRead);
    } else {
      // Upmix from mono (sourceNumChannels == 1): duplicate to all channels
      float *monoPtr = sourcePtr;
      for (int c = 0; c < targetNumChannels; c++) {
        outputBuffer.copyFrom(c, 0, monoPtr, actualSamplesRead);
      }
    }
  }

  void seek(long long targetPosition) override {
    wrappedFile->seek(targetPosition);
    PythonException::raise();
  }

  void seekInternal(long long targetPosition) override {
    wrappedFile->seekInternal(targetPosition);
  }

  long long tell() const override {
    return wrappedFile->tell();
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
    if (wrappedFile->isClosed())
      return true;

    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return _isClosed;
  }

  bool isSeekable() const override {
    return wrappedFile->isSeekable();
  }

  std::optional<std::string> getFilename() const override {
    return wrappedFile->getFilename();
  }

  PythonInputStream *getPythonInputStream() const override {
    return wrappedFile->getPythonInputStream();
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
    return "ChannelConvertedReadableAudioFile";
  }

private:
  const std::shared_ptr<AbstractReadableAudioFile> wrappedFile;
  const int targetNumChannels;
  juce::ReadWriteLock objectLock;
  bool _isClosed = false;
};

inline py::class_<ChannelConvertedReadableAudioFile, AbstractReadableAudioFile,
                  std::shared_ptr<ChannelConvertedReadableAudioFile>>
declare_readable_audio_file_with_channel_conversion(py::module &m) {
  return py::class_<ChannelConvertedReadableAudioFile, AbstractReadableAudioFile,
                    std::shared_ptr<ChannelConvertedReadableAudioFile>>(
      m, "ChannelConvertedReadableAudioFile",
      R"(
A class that wraps an audio file for reading, while converting
the audio stream on-the-fly to a new channel count.

*Introduced in v0.9.22.*

Reading, seeking, and all other basic file I/O operations are supported (except for
:meth:`read_raw`).

:class:`ChannelConvertedReadableAudioFile` should usually
be used via the :meth:`with_channels` method on :class:`ReadableAudioFile`
or :class:`ResampledReadableAudioFile`:

::

   with AudioFile("my_stereo_file.mp3").mono() as f:
       f.num_channels # => 1
       mono_audio = f.read(int(f.samplerate * 10))

   with AudioFile("my_mono_file.wav").stereo() as f:
       f.num_channels # => 2
       stereo_audio = f.read(int(f.samplerate * 10))

   with AudioFile("my_file.wav").with_channels(6) as f:
       f.num_channels # => 6
       surround_audio = f.read(int(f.samplerate * 10))

When converting from stereo (or multi-channel) to mono, all channels are
averaged together with equal weighting. When converting from mono to
stereo (or multi-channel), the mono signal is duplicated to all output
channels. Other conversions (stereo to multi-channel, multi-channel to
stereo, etc) are not currently supported.
)");
}

inline void init_readable_audio_file_with_channel_conversion(
    py::module &m,
    py::class_<ChannelConvertedReadableAudioFile, AbstractReadableAudioFile,
               std::shared_ptr<ChannelConvertedReadableAudioFile>>
        &pyChannelConvertedReadableAudioFile) {
  // Note: Most methods are inherited from AbstractReadableAudioFile.
  // We only define class-specific methods here.
  pyChannelConvertedReadableAudioFile
      .def(py::init([](std::shared_ptr<AbstractReadableAudioFile> audioFile,
                       int targetNumChannels)
                        -> ChannelConvertedReadableAudioFile * {
             // This definition is only here to provide nice docstrings.
             throw std::runtime_error(
                 "Internal error: __init__ should never be called, as this "
                 "class implements __new__.");
           }),
           py::arg("audio_file"), py::arg("num_channels"))
      .def_static(
          "__new__",
          [](const py::object *, std::shared_ptr<AbstractReadableAudioFile> audioFile,
             int targetNumChannels) {
            return std::make_shared<ChannelConvertedReadableAudioFile>(
                audioFile, targetNumChannels);
          },
          py::arg("cls"), py::arg("audio_file"), py::arg("num_channels"));
}

// Implementation of init_abstract_readable_audio_file_methods declared in
// AudioFileInit.h
inline void init_abstract_readable_audio_file_methods(
    py::class_<AbstractReadableAudioFile, AudioFile,
               std::shared_ptr<AbstractReadableAudioFile>>
        &pyAbstractReadableAudioFile) {
  pyAbstractReadableAudioFile
      .def(
          "resampled_to",
          [](std::shared_ptr<AbstractReadableAudioFile> file,
             double targetSampleRate, ResamplingQuality quality)
              -> std::shared_ptr<AbstractReadableAudioFile> {
            if (file->getSampleRateAsDouble() == targetSampleRate)
              return file;

            return std::make_shared<ResampledReadableAudioFile>(
                file, targetSampleRate, quality);
          },
          py::arg("target_sample_rate"),
          py::arg("quality") = ResamplingQuality::WindowedSinc32,
          "Return a :class:`ResampledReadableAudioFile` that will "
          "automatically resample this audio file to the provided "
          "`target_sample_rate`, using a constant amount of memory.\n\nIf "
          "`target_sample_rate` matches the existing sample rate of the file, "
          "the original file will be returned.\n\n*Introduced in v0.6.0.*")
      .def(
          "with_channels",
          [](std::shared_ptr<AbstractReadableAudioFile> file,
             int targetNumChannels)
              -> std::shared_ptr<AbstractReadableAudioFile> {
            if (file->getNumChannels() == targetNumChannels)
              return file;

            return std::make_shared<ChannelConvertedReadableAudioFile>(
                file, targetNumChannels);
          },
          py::arg("num_channels"),
          "Return a :class:`ChannelConvertedReadableAudioFile` that will "
          "automatically convert the channel count of this audio file to the "
          "provided `num_channels`.\n\nIf `num_channels` matches the existing "
          "channel count of the file, the original file will be "
          "returned.\n\nWhen converting from stereo (or multi-channel) to "
          "mono, all channels are averaged together with equal weighting. When "
          "converting from mono to stereo (or multi-channel), the mono signal "
          "is duplicated to all output channels.\n\n*Introduced in v0.9.17.*")
      .def(
          "mono",
          [](std::shared_ptr<AbstractReadableAudioFile> file)
              -> std::shared_ptr<AbstractReadableAudioFile> {
            if (file->getNumChannels() == 1)
              return file;

            return std::make_shared<ChannelConvertedReadableAudioFile>(
                file, 1);
          },
          "Return a :class:`ChannelConvertedReadableAudioFile` that will "
          "automatically convert this audio file to mono (1 channel).\n\nIf "
          "this file is already mono, the original file will be "
          "returned.\n\nWhen converting from stereo (or multi-channel) to "
          "mono, all channels are averaged together with equal "
          "weighting.\n\n*Introduced in v0.9.17.*")
      .def(
          "stereo",
          [](std::shared_ptr<AbstractReadableAudioFile> file)
              -> std::shared_ptr<AbstractReadableAudioFile> {
            if (file->getNumChannels() == 2)
              return file;

            return std::make_shared<ChannelConvertedReadableAudioFile>(
                file, 2);
          },
          "Return a :class:`ChannelConvertedReadableAudioFile` that will "
          "automatically convert this audio file to stereo (2 "
          "channels).\n\nIf this file is already stereo, the original file "
          "will be returned.\n\nWhen converting from mono to stereo, the mono "
          "signal is duplicated to both channels. When converting from "
          "multi-channel (3 or more channels) to stereo, only the first two "
          "channels are kept.\n\n*Introduced in v0.9.17.*");
}

} // namespace Pedalboard
