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

class ResampledReadableAudioFile
    : public AudioFile,
      public std::enable_shared_from_this<ResampledReadableAudioFile> {
public:
  ResampledReadableAudioFile(std::shared_ptr<ReadableAudioFile> audioFile,
                             float targetSampleRate, ResamplingQuality quality)
      : audioFile(audioFile),
        resampler(audioFile->getSampleRate(), targetSampleRate,
                  audioFile->getNumChannels(), quality) {}

  double getSampleRate() const { return resampler.getTargetSampleRate(); }

  long getLengthInSamples() const {
    double length = (((double)audioFile->getLengthInSamples() *
                      resampler.getTargetSampleRate()) /
                     audioFile->getSampleRate());
    if (resampler.getOutputLatency() > 0) {
      length -= (std::round(resampler.getOutputLatency()) -
                 resampler.getOutputLatency());
    }
    return (long)length;
  }

  double getDuration() const { return audioFile->getDuration(); }

  long getNumChannels() const { return audioFile->getNumChannels(); }

  std::string getFileFormat() const { return audioFile->getFileFormat(); }

  std::string getFileDatatype() const { return audioFile->getFileDatatype(); }

  py::array_t<float> read(long long numSamples) {
    const juce::ScopedLock scopedLock(objectLock);

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
                     audioFile->getSampleRate()) /
                    resampler.getTargetSampleRate());

    while (samplesInResampledBuffer < numSamples) {
      std::optional<juce::AudioBuffer<float>> resamplerInput;

      juce::AudioBuffer<float> sourceSamples;
      if (inputSamplesRequired > 0) {
        sourceSamples =
            copyPyArrayIntoJuceBuffer(audioFile->read(inputSamplesRequired),
                                      {ChannelLayout::NotInterleaved});

        if (sourceSamples.getNumSamples() == 0) {
          resamplerInput = {};
        } else {
          resamplerInput =
              std::optional<juce::AudioBuffer<float>>(sourceSamples);
        }
      } else {
        sourceSamples =
            juce::AudioBuffer<float>(audioFile->getNumChannels(), 0);
        resamplerInput = std::optional<juce::AudioBuffer<float>>(sourceSamples);
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
    return copyJuceBufferIntoPyArray(resampledBuffer,
                                     ChannelLayout::NotInterleaved, 0);
  }

  void seek(long long targetPosition) {
    // TODO: This could be done much more efficiently by
    // only seeking as far back as we need to to prime the resampler.
    // That logic is _very_ tricky to write correctly, though.
    audioFile->seek(0);
    resampler.reset();
    positionInTargetSampleRate = 0;
    outputBuffer.setSize(0, 0);

    const long long chunkSize = 1024 * 1024;
    for (long long i = 0; i < targetPosition; i += chunkSize) {
      this->read(std::min(chunkSize, targetPosition - i));
    }
  }

  long long tell() const { return positionInTargetSampleRate; }

  void close() {
    const juce::ScopedLock scopedLock(objectLock);
    _isClosed = true;
  }

  bool isClosed() const { return _isClosed; }

  bool isSeekable() const { return audioFile->isSeekable(); }

  std::optional<std::string> getFilename() const {
    return audioFile->getFilename();
  }

  PythonInputStream *getPythonInputStream() const {
    return audioFile->getPythonInputStream();
  }

  std::shared_ptr<ResampledReadableAudioFile> enter() {
    return shared_from_this();
  }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    close();
  }

private:
  std::shared_ptr<ReadableAudioFile> audioFile;
  StreamResampler<float> resampler;
  juce::AudioBuffer<float> outputBuffer;
  long long positionInTargetSampleRate = 0;
  juce::CriticalSection objectLock;
  bool _isClosed = false;
};

inline py::class_<ResampledReadableAudioFile, AudioFile,
                  std::shared_ptr<ResampledReadableAudioFile>>
declare_resampled_readable_audio_file(py::module &m) {
  return py::class_<ResampledReadableAudioFile, AudioFile,
                    std::shared_ptr<ResampledReadableAudioFile>>(
      m, "ResampledReadableAudioFile",
      R"(
A class that wraps an audio file for reading, while resampling
the audio stream to a new sample rate.
)");
}

inline void init_resampled_readable_audio_file(
    py::module &m, py::class_<ResampledReadableAudioFile, AudioFile,
                              std::shared_ptr<ResampledReadableAudioFile>>
                       &pyResampledReadableAudioFile) {
  pyResampledReadableAudioFile
      .def(py::init(
               [](std::shared_ptr<ReadableAudioFile> audioFile,
                  float targetSampleRate,
                  ResamplingQuality quality) -> ResampledReadableAudioFile * {
                 // This definition is only here to provide nice docstrings.
                 throw std::runtime_error(
                     "Internal error: __init__ should never be called, as this "
                     "class implements __new__.");
               }),
           py::arg("audio_file"), py::arg("target_sample_rate"),
           py::arg("quality"))
      .def_static(
          "__new__",
          [](const py::object *, std::shared_ptr<ReadableAudioFile> audioFile,
             float targetSampleRate, ResamplingQuality quality) {
            return std::make_shared<ResampledReadableAudioFile>(
                audioFile, targetSampleRate, quality);
          },
          py::arg("cls"), py::arg("audio_file"), py::arg("target_sample_rate"),
          py::arg("quality"))
      .def(
          "read", &ResampledReadableAudioFile::read, py::arg("num_frames") = 0,
          "Read the given number of frames (samples in each channel) from this "
          "audio file at its current position.\n\n``num_frames`` is a required "
          "argument, as audio files can be deceptively large. (Consider that "
          "an hour-long ``.ogg`` file may be only a handful of megabytes on "
          "disk, but may decompress to nearly a gigabyte in memory.) Audio "
          "files should be read in chunks, rather than all at once, to avoid "
          "hard-to-debug memory problems and out-of-memory crashes.\n\nAudio "
          "samples are returned as a multi-dimensional :class:`numpy.array` "
          "with the shape ``(channels, samples)``; i.e.: a stereo audio file "
          "will have shape ``(2, <length>)``. Returned data is always in the "
          "``float32`` datatype.\n\nFor most (but not all) audio files, the "
          "minimum possible sample value will be ``-1.0f`` and the maximum "
          "sample value will be ``+1.0f``.")
      .def("seekable", &ResampledReadableAudioFile::isSeekable,
           "Returns True if this file is currently open and calls to seek() "
           "will work.")
      .def("seek", &ResampledReadableAudioFile::seek, py::arg("position"),
           "Seek this file to the provided location in frames. Future reads "
           "will start from this position.")
      .def("tell", &ResampledReadableAudioFile::tell,
           "Return the current position of the read pointer in this audio "
           "file, in frames. This value will increase as :meth:`read` is "
           "called, and may decrease if :meth:`seek` is called.")
      .def("close", &ResampledReadableAudioFile::close,
           "Close this file, rendering this object unusable.")
      .def("__enter__", &ResampledReadableAudioFile::enter,
           "Use this :class:`ResampledReadableAudioFile` as a context manager, "
           "automatically closing the file and releasing resources when the "
           "context manager exits.")
      .def("__exit__", &ResampledReadableAudioFile::exit,
           "Stop using this :class:`ResampledReadableAudioFile` as a context "
           "manager, "
           "close the file, release its resources.")
      .def("__repr__",
           [](const ResampledReadableAudioFile &file) {
             std::ostringstream ss;
             ss << "<pedalboard.io.ResampledReadableAudioFile";

             if (file.getFilename() && !file.getFilename()->empty()) {
               ss << " filename=\"" << *file.getFilename() << "\"";
             } else if (PythonInputStream *stream =
                            file.getPythonInputStream()) {
               ss << " file_like=" << stream->getRepresentation();
             }

             if (file.isClosed()) {
               ss << " closed";
             } else {
               ss << " samplerate=" << file.getSampleRate();
               ss << " num_channels=" << file.getNumChannels();
               ss << " frames=" << file.getLengthInSamples();
               ss << " file_dtype=" << file.getFileDatatype();
             }
             ss << " at " << &file;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "name", &ResampledReadableAudioFile::getFilename,
          "The name of this file.\n\nIf this "
          ":class:`ResampledReadableAudioFile` was "
          "opened from a file-like object, this will be ``None``.")
      .def_property_readonly("closed", &ResampledReadableAudioFile::isClosed,
                             "True iff this file is closed (and no longer "
                             "usable), False otherwise.")
      .def_property_readonly("samplerate",
                             &ResampledReadableAudioFile::getSampleRate,
                             "The sample rate of this file in samples "
                             "(per channel) per second (Hz).")
      .def_property_readonly("num_channels",
                             &ResampledReadableAudioFile::getNumChannels,
                             "The number of channels in this file.")
      .def_property_readonly(
          "frames", &ResampledReadableAudioFile::getLengthInSamples,
          "The total number of frames (samples per "
          "channel) in this file.\n\nFor example, if this "
          "file contains 10 seconds of stereo audio at sample "
          "rate of 44,100 Hz, ``frames`` will return ``441,000``.")
      .def_property_readonly("duration",
                             &ResampledReadableAudioFile::getDuration,
                             "The duration of this file in seconds (``frames`` "
                             "divided by ``samplerate``).")
      .def_property_readonly(
          "file_dtype", &ResampledReadableAudioFile::getFileDatatype,
          "The data type (``\"int16\"``, ``\"float32\"``, etc) stored "
          "natively by this file.\n\nNote that :meth:`read` will always "
          "return a ``float32`` array, regardless of the value of this "
          "property.");
}
} // namespace Pedalboard
