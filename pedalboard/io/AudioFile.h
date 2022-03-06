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

#include "../JuceHeader.h"

namespace py = pybind11;

namespace Pedalboard {

class AudioFile : public std::enable_shared_from_this<AudioFile> {
public:
  AudioFile(std::string filename) {
    formatManager.registerBasicFormats();
    juce::File file(filename);
    reader.reset(formatManager.createReaderFor(file));
    if (!reader) {
      throw std::domain_error("Unable to open audio file: " + filename);
    }
  }

  double getSampleRate() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return reader->sampleRate;
  }

  long getLengthInSamples() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return reader->lengthInSamples;
  }

  long getNumChannels() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return reader->numChannels;
  }

  py::array_t<float> read(long long numSamples) {
    if (numSamples == 0)
      throw std::domain_error(
          "AudioFile will not read an entire file at once, due to the "
          "possibility that a file may be larger than available memory. Please "
          "pass a number of frames to read (available from the 'frames' "
          "attribute).");

    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    // Allocate a buffer to return of up to numSamples:
    int numChannels = reader->numChannels;
    numSamples =
        std::min(numSamples, reader->lengthInSamples - currentPosition);
    py::array_t<float> buffer =
        py::array_t<float>({(int)numChannels, (int)numSamples});

    py::buffer_info outputInfo = buffer.request();

    {
      py::gil_scoped_release release;

      float **channelPointers = (float **)alloca(numChannels * sizeof(float *));
      for (int c = 0; c < numChannels; c++) {
        channelPointers[c] = ((float *)outputInfo.ptr) + (numSamples * c);
      }

      reader->read(channelPointers, numChannels, currentPosition, numSamples);
    }

    currentPosition += numSamples;
    return buffer;
  }

  void seek(long long targetPosition) {
    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    if (targetPosition > reader->lengthInSamples)
      throw std::domain_error("Cannot seek beyond end of file (" + std::to_string(reader->lengthInSamples) + " frames).");
    if (targetPosition < 0)
      throw std::domain_error("Cannot seek before start of file.");
    currentPosition = targetPosition;
  }

  long long tell() {
    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return currentPosition;
  }

  void close() {
    const juce::ScopedLock scopedLock(objectLock);
    reader.reset();
  }

  bool isClosed() {
    const juce::ScopedLock scopedLock(objectLock);
    return reader == nullptr;
  }

  bool isSeekable() {
    const juce::ScopedLock scopedLock(objectLock);

    // At the moment, AudioFile instances are always seekable, as they're backed by files.
    return !isClosed();
  }

  std::shared_ptr<AudioFile> enter() { return shared_from_this(); }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    close();
  }

private:
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReader> reader = nullptr;
  juce::CriticalSection objectLock;

  int currentPosition = 0;
};

inline void init_audiofile(py::module &m) {
  py::class_<AudioFile, std::shared_ptr<AudioFile>>(
      m, "AudioFile",
      "An audio file reader interface, with native support for for Ogg Vorbis, "
      "MP3, WAV, FLAC, and AIFF files on all operating systems. On some "
      "platforms, other formats may also be supported. (Use "
      "AudioFile.supported_formats to see which formats are supported on the "
      "current platform.)")
      .def(py::init([](std::string filename) {
             return std::make_unique<AudioFile>(filename);
           }),
           py::arg("filename"))
      .def("read", &AudioFile::read, py::arg("num_frames") = 0,
           "Read the given number of frames (samples in each channel) from the "
           "audio file at the current position. Audio samples are returned in "
           "the shape (channels, samples); i.e.: a stereo audio file will have "
           "shape (2, <length>).")
      .def("seekable", &AudioFile::isSeekable,
           "Returns True if the file is currently open and calls to seek() will work.")
      .def("seek", &AudioFile::seek, py::arg("position"),
           "Seek the file to the provided location in frames.")
      .def("tell", &AudioFile::tell,
           "Fetch the position in the audio file, in frames.")
      .def("close", &AudioFile::close,
           "Close the file, rendering this object unusable.")
      .def("__enter__", &AudioFile::enter)
      .def("__exit__", &AudioFile::exit)
      .def_property_readonly("closed", &AudioFile::isClosed,
                             "If the file has been closed, this property will be True.")
      .def_property_readonly("samplerate", &AudioFile::getSampleRate,
                             "Fetch the sample rate of the file in samples "
                             "(per channel) per second (Hz).")
      .def_property_readonly("channels", &AudioFile::getNumChannels,
                             "Fetch the number of channels in the file.")
      .def_property_readonly("frames", &AudioFile::getLengthInSamples,
                             "Fetch the total number of frames (samples per "
                             "channel) in the file.")
      .def_property_readonly_static(
          "supported_formats", [](py::object /* cls */) {
            juce::AudioFormatManager manager;
            manager.registerBasicFormats();

            std::vector<std::string> formatNames(manager.getNumKnownFormats());
            juce::StringArray extensions;
            for (int i = 0; i < manager.getNumKnownFormats(); i++) {
              auto *format = manager.getKnownFormat(i);
              extensions.addArray(format->getFileExtensions());
            }

            extensions.trim();
            extensions.removeEmptyStrings();
            extensions.removeDuplicates(true);

            std::vector<std::string> output;
            for (juce::String s : extensions) {
              output.push_back(s.toStdString());
            }

            std::sort(output.begin(), output.end(),
                      [](const std::string lhs, const std::string rhs) {
                        return lhs < rhs;
                      });

            return output;
          });
}
} // namespace Pedalboard