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
    return (long)(audioFile->getDuration() * resampler.getTargetSampleRate());
  }

  double getDuration() const { return audioFile->getDuration(); }

  long getNumChannels() const { return audioFile->getNumChannels(); }

  std::string getFileFormat() const { return audioFile->getFileFormat(); }

  std::string getFileDatatype() const { return audioFile->getFileDatatype(); }

  py::array_t<float> read(long long numSamples) {
    const juce::ScopedLock scopedLock(objectLock);

    double durationSeconds =
        (double)numSamples / resampler.getTargetSampleRate();
    long long numSamplesInSourceSampleRate =
        (long long)std::ceil(durationSeconds * audioFile->getSampleRate());

    numSamplesInSourceSampleRate +=
        (resampler.getInputLatency() - resampler.getInputSamplesBuffered());

    juce::AudioBuffer<float> sourceSamples = copyPyArrayIntoJuceBuffer(
        audioFile->read(numSamplesInSourceSampleRate));

    juce::AudioBuffer<float> resampledBuffer;
    {
      py::gil_scoped_release release;
      std::optional<juce::AudioBuffer<float>> resamplerInput = {sourceSamples};
      resampledBuffer = resampler.process(resamplerInput);
      if (resampledBuffer.getNumSamples() < numSamples) {
        // Flush the resampler to produce the expected new number of output
        // samples:
        long long numSamplesMissing =
            numSamples - resampledBuffer.getNumSamples();
        long long numSamplesMissingInSourceSampleRate = std::ceil(
            ((double)numSamplesMissing / resampler.getTargetSampleRate()) *
            audioFile->getSampleRate());

        resamplerInput = {};
        juce::AudioBuffer<float> flushedOutput = resampler.process(
            resamplerInput, numSamplesMissingInSourceSampleRate);

        int startSample = resampledBuffer.getNumSamples();
        resampledBuffer.setSize(resampledBuffer.getNumChannels(),
                                resampledBuffer.getNumSamples() +
                                    flushedOutput.getNumSamples(),
                                /* keepExistingContent */ true);

        for (int c = 0; c < resampledBuffer.getNumChannels(); c++) {
          resampledBuffer.copyFrom(c, startSample, flushedOutput, c, 0,
                                   flushedOutput.getNumSamples());
        }
      }
    }

    return copyJuceBufferIntoPyArray(resampledBuffer,
                                     ChannelLayout::NotInterleaved, 0);
  }

  void seek(long long targetPosition) {
    double targetPositionSeconds =
        (double)targetPosition / (double)resampler.getTargetSampleRate();
    // TODO: Account for resampler latency here!
    audioFile->seek(
        (long long)(targetPositionSeconds * audioFile->getSampleRate()));
  }

  long long tell() const {
    // TODO: Account for resampler latency here!
    double currentPositionSeconds =
        (double)audioFile->tell() / (double)audioFile->getSampleRate();
    return (long long)(currentPositionSeconds *
                       resampler.getTargetSampleRate());
  }

  void close() { audioFile->close(); }

  bool isClosed() const { return audioFile->isClosed(); }

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
  juce::CriticalSection objectLock;
};

inline py::class_<ResampledReadableAudioFile, AudioFile,
                  std::shared_ptr<ResampledReadableAudioFile>>
declare_resampled_readable_audio_file(py::module &m) {
  return py::class_<ResampledReadableAudioFile, AudioFile,
                    std::shared_ptr<ResampledReadableAudioFile>>(
      m, "ResampledReadableAudioFile",
      R"(
A class that wraps an audio file for reading, while resampling
the audio stream to a new sample rate during reading.
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
