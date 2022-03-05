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
#include "./FileLikeWrapper.h"

namespace py = pybind11;

// Returned by the Ogg Vorbis code if an error occurs:
// 2^32 - 131 ("OV_EINVAL")
long OGG_FORMAT_ERROR = 4294967165;

namespace Pedalboard {

/**
 * A juce::AudioFile subclass that fetches its
 * data from a provided Python file-like object.
 */
class AudioFile {
public:
  static constexpr const int BUFFER_SIZE = 1024 * 16;

  AudioFile(std::string filename) {
      formatManager.registerBasicFormats();
      juce::File file(filename);
      reader.reset(formatManager.createReaderFor(file));
      if (!reader) {
          throw std::domain_error("Unable to open audio file: " + filename);
      }
  }

  AudioFile(std::unique_ptr<PythonInputStream> inputStream, std::string formatHint) {
    if (!inputStream) {
      throw std::domain_error("file-like must not be None");
    }

    // There are a couple important edge cases to consider here:
    //  - To auto-detect the audio format, an input stream must be seekable 
    //  - Some formats detect length by doing three commands in sequence:
    //     - setPosition(getTotalLength())
    //     - getPosition()
    //     - setPosition(0)
    // TODO: Write a custom BufferedInputStream-like subclass that buffers:
    //  - the first ~10kB of the file
    //  - the previous ~10kB prior to the current read pointer
    //  - allows seeking to the start of the file and back to the pointer
    if (inputStream->isSeekable() && formatHint.empty()) {
      formatManager.registerBasicFormats();
      reader.reset(formatManager.createReaderFor(std::move(inputStream)));  
    } else {
      if (formatHint.empty()) {
        // TODO: Try to identify the file type somehow
        // TODO: Can we subclass InputStream to allow peeking at the first 4 bytes?
        // urllib3.response.HTTPResponse provides `getheader("Content-Type")`
        throw std::runtime_error("Passing a file-like requires a file format hint to be passed as the second argument.");
      }

      if (formatHint.find("ogg") != std::string::npos) {
        juce::OggVorbisAudioFormat format;
        reader.reset(format.createReaderFor(inputStream.get(), false));
      } else if (formatHint.find("mp3") != std::string::npos) {
        juce::MP3AudioFormat format;
        reader.reset(format.createReaderFor(inputStream.get(), false));
      } else if (formatHint.find("wav") != std::string::npos) {
        juce::WavAudioFormat format;
        reader.reset(format.createReaderFor(inputStream.get(), false));
      } else {
        throw std::domain_error("Unknown format hint: expected one of 'ogg', 'mp3', or 'wav'.");
      }

      if (reader) inputStream.release();
    }
    

    if (!reader) {
      throw std::domain_error("Unable to open audio file from stream.");
    }

    raisePythonErrorIfNecessary();
  }

  void raisePythonErrorIfNecessary() {
    ((PythonInputStream *)reader->input)->raisePythonErrorIfNecessary();
  }

  bool isSeekable() {
    ((PythonInputStream *)reader->input)->isSeekable();
  }

  double getSampleRate() {
    raisePythonErrorIfNecessary();
    return reader->sampleRate;
  }

  long getLengthInSamples() {
    raisePythonErrorIfNecessary();

    if (reader->lengthInSamples < 0 || reader->lengthInSamples == OGG_FORMAT_ERROR) {
      throw std::domain_error("Unknown number of samples available in file.");
    }
    return reader->lengthInSamples;
  }

  long getNumChannels() {
    raisePythonErrorIfNecessary();
    return reader->numChannels;
  }

  py::array_t<float> read(long long numSamples) {
    raisePythonErrorIfNecessary();

    if (numSamples == 0) throw std::domain_error("AudioFile will not read an entire file at once, due to the possibility that a file may be larger than available memory");

    const juce::ScopedLock scopedLock(objectLock);

    // Allocate a buffer to return of up to numSamples:
    int numChannels = reader->numChannels;
    numSamples = std::min(numSamples, reader->lengthInSamples - currentPosition);
    py::array_t<float> buffer = py::array_t<float>({(int) numChannels, (int)numSamples});

    py::buffer_info outputInfo = buffer.request();

    {
      py::gil_scoped_release release;

      float *readPointers[numChannels];
      for (int c = 0; c < numChannels; c++) {
        readPointers[c] = ((float *)outputInfo.ptr) + (numSamples * c);
      }

      reader->read(readPointers, numChannels, currentPosition, numSamples);
    }

    currentPosition += numSamples;
    return buffer;
  }

  void seek(long long targetPosition) {
    if (!isSeekable()) throw std::runtime_error("Underlying stream is not seekable.");
    raisePythonErrorIfNecessary();

    const juce::ScopedLock scopedLock(objectLock);
    if (targetPosition > reader->lengthInSamples) throw std::domain_error("Seek out of bounds");
    if (targetPosition < 0) throw std::domain_error("Seek out of bounds");
    currentPosition = targetPosition;
  }

  long long tell() {
    raisePythonErrorIfNecessary();

    const juce::ScopedLock scopedLock(objectLock);
    return currentPosition;
  }

private:
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReader> reader = nullptr;
  juce::CriticalSection objectLock;

  int currentPosition = 0;
};


inline void init_audiofile(py::module &m) {
  py::class_<AudioFile>(
      m, "AudioFile",
      "An audio file reader interface, with native support for Ogg Vorbis, MP3, Wave, FLAC, and AIFF files.")
      .def(py::init([](std::string filename) {
             return std::make_unique<AudioFile>(filename);
           }),
           py::arg("filename"))
      .def(py::init([](py::object fileLike, std::string formatHint) {
            return std::make_unique<AudioFile>(std::make_unique<PythonInputStream>(fileLike), formatHint);
           }),
           py::arg("file_like"), py::arg("format_hint") = "")
      .def("read", &AudioFile::read)
      .def("seek", &AudioFile::seek)
      .def("tell", &AudioFile::tell)
      .def_property_readonly("samplerate", &AudioFile::getSampleRate)
      .def_property_readonly("channels", &AudioFile::getNumChannels)
      .def_property_readonly("frames", &AudioFile::getLengthInSamples);
}
} // namespace Pedalboard