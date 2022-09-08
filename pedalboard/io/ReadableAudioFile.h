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

class ReadableAudioFile
    : public AudioFile,
      public std::enable_shared_from_this<ReadableAudioFile> {
public:
  ReadableAudioFile(std::string filename) : filename(filename) {
    formatManager.registerBasicFormats();
    juce::File file(filename);

    if (!file.existsAsFile()) {
      throw std::domain_error(
          "Failed to open audio file: file does not exist: " + filename);
    }

    // createReaderFor(juce::File) is fast, as it only looks at file extension:
    reader.reset(formatManager.createReaderFor(file));
    if (!reader) {
      // This is slower but more thorough:
      reader.reset(formatManager.createReaderFor(file.createInputStream()));

      // Known bug: the juce::MP3Reader class will parse formats that are not
      // MP3 and pretend like they are, producing garbage output. For now, if we
      // parse MP3 from an input stream that's not explicitly got ".mp3" on the
      // end, ignore it.
      if (reader && reader->getFormatName() == "MP3 file") {
        throw std::domain_error(
            "Failed to open audio file: file \"" + filename +
            "\" does not seem to be of a known or supported format. (If trying "
            "to open an MP3 file, ensure the filename ends with '.mp3'.)");
      }
    }

    if (!reader)
      throw std::domain_error(
          "Failed to open audio file: file \"" + filename +
          "\" does not seem to be of a known or supported format.");
  }

  ReadableAudioFile(std::unique_ptr<PythonInputStream> inputStream) {
    formatManager.registerBasicFormats();

    if (!inputStream->isSeekable()) {
      PythonException::raise();
      throw std::domain_error("Failed to open audio file-like object: input "
                              "stream must be seekable.");
    }

    if (!reader) {
      auto originalStreamPosition = inputStream->getPosition();

      for (int i = 0; i < formatManager.getNumKnownFormats(); i++) {
        auto *af = formatManager.getKnownFormat(i);

        if (auto *r = af->createReaderFor(inputStream.get(), false)) {
          inputStream.release();
          reader.reset(r);
          break;
        }

        // createReaderFor may have thrown a Python exception, under the hood
        // which we need to check for before blindly continuing:
        PythonException::raise();

        inputStream->setPosition(originalStreamPosition);
        if (inputStream->getPosition() != originalStreamPosition) {
          throw std::runtime_error(
              "Input file-like object did not seek to the expected position. "
              "The provided file-like object must be fully seekable to allow "
              "reading audio files.");
        }
      }

      // Known bug: the juce::MP3Reader class will parse formats that are not
      // MP3 and pretend like they are, producing garbage output. For now, if we
      // parse MP3 from an input stream that's not explicitly got ".mp3" on the
      // end, ignore it.
      if (reader && reader->getFormatName() == "MP3 file") {
        bool fileLooksLikeAnMP3 = false;
        if (auto filename = getPythonInputStream()->getFilename()) {
          fileLooksLikeAnMP3 = juce::File(*filename).hasFileExtension("mp3");
        }

        if (!fileLooksLikeAnMP3) {
          PythonException::raise();
          throw std::domain_error(
              "Failed to open audio file-like object: stream does not seem to "
              "contain a known or supported format. (If trying to open an MP3 "
              "file, pass a file-like with a \"name\" attribute ending with "
              "\".mp3\".)");
        }
      }
    }

    PythonException::raise();

    if (!reader)
      throw std::domain_error(
          "Failed to open audio file-like object: " +
          inputStream->getRepresentation() +
          " does not seem to contain a known or supported format.");

    PythonException::raise();
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

  double getDuration() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return reader->lengthInSamples / reader->sampleRate;
  }

  long getNumChannels() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return reader->numChannels;
  }

  std::string getFileFormat() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    return reader->getFormatName().toStdString();
  }

  std::string getFileDatatype() const {
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    if (reader->usesFloatingPointData) {
      switch (reader->bitsPerSample) {
      case 16: // OGG returns 16-bit int data, but internally stores floats
      case 32:
        return "float32";
      case 64:
        return "float64";
      default:
        return "unknown";
      }
    } else {
      switch (reader->bitsPerSample) {
      case 8:
        return "int8";
      case 16:
        return "int16";
      case 24:
        return "int24";
      case 32:
        return "int32";
      case 64:
        return "int64";
      default:
        return "unknown";
      }
    }
  }

  py::array_t<float, py::array::c_style> read(long long numSamples) {
    if (numSamples == 0)
      throw std::domain_error(
          "ReadableAudioFile will not read an entire file at once, due to the "
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

      // If the file being read does not have enough content, it _should_ pad
      // the rest of the array with zeroes. Unfortunately, this does not seem to
      // be true in practice, so we pre-zero the array to be returned here:
      std::memset((void *)outputInfo.ptr, 0,
                  numChannels * numSamples * sizeof(float));

      float **channelPointers = (float **)alloca(numChannels * sizeof(float *));
      for (int c = 0; c < numChannels; c++) {
        channelPointers[c] = ((float *)outputInfo.ptr) + (numSamples * c);
      }

      if (reader->usesFloatingPointData || reader->bitsPerSample == 32) {
        auto readResult = reader->read(channelPointers, numChannels,
                                       currentPosition, numSamples);
        PythonException::raise();

        if (!readResult) {
          throw std::runtime_error("Failed to read from file.");
        }
      } else {
        // If the audio is stored in an integral format, read it as integers
        // and do the floating-point conversion ourselves to work around
        // floating-point imprecision in JUCE when reading formats smaller than
        // 32-bit (i.e.: 16-bit audio is off by about 0.003%)
        auto readResult =
            reader->readSamples((int **)channelPointers, numChannels, 0,
                                currentPosition, numSamples);
        PythonException::raise();
        if (!readResult) {
          throw std::runtime_error("Failed to read from file.");
        }

        // When converting 24-bit, 16-bit, or 8-bit data from int to float,
        // the values provided by the above read() call are shifted left
        // (such that the least significant bits are all zero)
        // JUCE will then divide these values by 0x7FFFFFFF, even though
        // the least significant bits are zero, effectively losing precision.
        // Instead, here we set the scale factor appropriately.
        int maxValueAsInt;
        switch (reader->bitsPerSample) {
        case 24:
          maxValueAsInt = 0x7FFFFF00;
          break;
        case 16:
          maxValueAsInt = 0x7FFF0000;
          break;
        case 8:
          maxValueAsInt = 0x7F000000;
          break;
        default:
          throw std::runtime_error("Not sure how to convert data from " +
                                   std::to_string(reader->bitsPerSample) +
                                   " bits per sample to floating point!");
        }
        float scaleFactor = 1.0f / static_cast<float>(maxValueAsInt);

        for (int c = 0; c < numChannels; c++) {
          juce::FloatVectorOperations::convertFixedToFloat(
              channelPointers[c], (const int *)channelPointers[c], scaleFactor,
              static_cast<int>(numSamples));
        }
      }
    }

    currentPosition += numSamples;
    return buffer;
  }

  py::array readRaw(long long numSamples) {
    if (numSamples == 0)
      throw std::domain_error(
          "ReadableAudioFile will not read an entire file at once, due to the "
          "possibility that a file may be larger than available memory. Please "
          "pass a number of frames to read (available from the 'frames' "
          "attribute).");

    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    if (reader->usesFloatingPointData) {
      return read(numSamples);
    } else {
      switch (reader->bitsPerSample) {
      case 32:
        return readInteger<int>(numSamples);
      case 16:
        return readInteger<short>(numSamples);
      case 8:
        return readInteger<char>(numSamples);
      default:
        throw std::runtime_error("Not sure how to read " +
                                 std::to_string(reader->bitsPerSample) +
                                 "-bit audio data!");
      }
    }
  }

  template <typename SampleType>
  py::array_t<SampleType> readInteger(long long numSamples) {
    if (reader->usesFloatingPointData) {
      throw std::runtime_error(
          "Can't call readInteger with a floating point file!");
    }

    // Allocate a buffer to return of up to numSamples:
    int numChannels = reader->numChannels;
    numSamples =
        std::min(numSamples, reader->lengthInSamples - currentPosition);
    py::array_t<SampleType> buffer =
        py::array_t<SampleType>({(int)numChannels, (int)numSamples});

    py::buffer_info outputInfo = buffer.request();

    {
      py::gil_scoped_release release;
      if (reader->bitsPerSample > 16) {
        if (sizeof(SampleType) < 4) {
          throw std::runtime_error("Output array not wide enough to store " +
                                   std::to_string(reader->bitsPerSample) +
                                   "-bit integer data.");
        }

        std::memset((void *)outputInfo.ptr, 0,
                    numChannels * numSamples * sizeof(SampleType));

        int **channelPointers = (int **)alloca(numChannels * sizeof(int *));
        for (int c = 0; c < numChannels; c++) {
          channelPointers[c] = ((int *)outputInfo.ptr) + (numSamples * c);
        }

        auto readResult = reader->readSamples(channelPointers, numChannels, 0,
                                              currentPosition, numSamples);
        PythonException::raise();
        if (!readResult) {
          throw std::runtime_error("Failed to read from file.");
        }
      } else {
        // Read the file in smaller chunks, converting from int32 to the
        // appropriate output format as we go:
        std::vector<std::vector<int>> intBuffers;
        intBuffers.resize(numChannels);

        int **channelPointers = (int **)alloca(numChannels * sizeof(int *));
        for (long long startSample = 0; startSample < numSamples;
             startSample += DEFAULT_AUDIO_BUFFER_SIZE_FRAMES) {
          int samplesToRead =
              std::min(numSamples - startSample,
                       (long long)DEFAULT_AUDIO_BUFFER_SIZE_FRAMES);

          for (int c = 0; c < numChannels; c++) {
            intBuffers[c].resize(samplesToRead);
            channelPointers[c] = intBuffers[c].data();
          }

          auto readResult =
              reader->readSamples(channelPointers, numChannels, 0,
                                  currentPosition + startSample, samplesToRead);

          PythonException::raise();

          if (!readResult) {
            throw std::runtime_error("Failed to read from file.");
          }

          // Convert the data in intBuffers to the output format:
          char shift = 32 - reader->bitsPerSample;
          for (int c = 0; c < numChannels; c++) {
            SampleType *outputChannelPointer =
                (((SampleType *)outputInfo.ptr) + (c * numSamples));
            for (int i = 0; i < samplesToRead; i++) {
              outputChannelPointer[startSample + i] = intBuffers[c][i] >> shift;
            }
          }
        }
      }
    }

    currentPosition += numSamples;
    return buffer;
  }

  void seek(long long targetPosition) {
    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    if (targetPosition > reader->lengthInSamples)
      throw std::domain_error("Cannot seek beyond end of file (" +
                              std::to_string(reader->lengthInSamples) +
                              " frames).");
    if (targetPosition < 0)
      throw std::domain_error("Cannot seek before start of file.");
    currentPosition = targetPosition;
  }

  long long tell() const {
    const juce::ScopedLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");
    return currentPosition;
  }

  void close() {
    const juce::ScopedLock scopedLock(objectLock);
    reader.reset();
  }

  bool isClosed() const {
    const juce::ScopedLock scopedLock(objectLock);
    return !reader;
  }

  bool isSeekable() const {
    const juce::ScopedLock scopedLock(objectLock);

    // At the moment, ReadableAudioFile instances are always seekable, as
    // they're backed by files.
    return !isClosed();
  }

  std::optional<std::string> getFilename() const { return filename; }

  PythonInputStream *getPythonInputStream() const {
    if (!filename.empty()) {
      return nullptr;
    }
    if (!reader) {
      return nullptr;
    }

    // the AudioFormatReader retains exclusive ownership over the input stream,
    // so we have to cast here instead of holding a shared_ptr:
    return (PythonInputStream *)reader->input;
  }

  std::shared_ptr<ReadableAudioFile> enter() { return shared_from_this(); }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    close();
  }

private:
  juce::AudioFormatManager formatManager;
  std::string filename;
  std::unique_ptr<juce::AudioFormatReader> reader;
  juce::CriticalSection objectLock;

  int currentPosition = 0;
};

inline py::class_<ReadableAudioFile, AudioFile,
                  std::shared_ptr<ReadableAudioFile>>
declare_readable_audio_file(py::module &m) {
  return py::class_<ReadableAudioFile, AudioFile,
                    std::shared_ptr<ReadableAudioFile>>(m, "ReadableAudioFile",
                                                        R"(
A class that wraps an audio file for reading, with native support for Ogg Vorbis,
MP3, WAV, FLAC, and AIFF files on all operating systems. Other formats may also
be readable depending on the operating system and installed system libraries:

 - macOS: ``.3g2``, ``.3gp``, ``.aac``, ``.ac3``, ``.adts``, ``.aif``,
   ``.aifc``, ``.aiff``, ``.amr``, ``.au``, ``.bwf``, ``.caf``,
   ``.ec3``, ``.flac``, ``.latm``, ``.loas``, ``.m4a``, ``.m4b``,
   ``.m4r``, ``.mov``, ``.mp1``, ``.mp2``, ``.mp3``, ``.mp4``,
   ``.mpa``, ``.mpeg``, ``.ogg``, ``.qt``, ``.sd2``,
   ``.snd``, ``.w64``, ``.wav``, ``.xhe``
 - Windows: ``.aif``, ``.aiff``, ``.flac``, ``.mp3``, ``.ogg``,
   ``.wav``, ``.wma``
 - Linux: ``.aif``, ``.aiff``, ``.flac``, ``.mp3``, ``.ogg``,
   ``.wav``

Use :meth:`pedalboard.io.get_supported_read_formats()` to see which
formats or file extensions are supported on the current platform.

(Note that although an audio file may have a certain file extension, its
contents may be encoded with a compression algorithm unsupported by
Pedalboard.)

.. note::
    You probably don't want to use this class directly: passing the
    same arguments to :class:`AudioFile` will work too, and allows using
    :class:`AudioFile` just like you'd use ``open(...)`` in Python.

)");
}

class ResampledReadableAudioFile;

inline void init_readable_audio_file(
    py::module &m,
    py::class_<ReadableAudioFile, AudioFile, std::shared_ptr<ReadableAudioFile>>
        &pyReadableAudioFile) {
  pyReadableAudioFile
      .def(py::init([](std::string filename) -> ReadableAudioFile * {
             // This definition is only here to provide nice docstrings.
             throw std::runtime_error(
                 "Internal error: __init__ should never be called, as this "
                 "class implements __new__.");
           }),
           py::arg("filename"))
      .def(py::init([](py::object filelike) -> ReadableAudioFile * {
             // This definition is only here to provide nice docstrings.
             throw std::runtime_error(
                 "Internal error: __init__ should never be called, as this "
                 "class implements __new__.");
           }),
           py::arg("file_like"))
      .def_static(
          "__new__",
          [](const py::object *, std::string filename) {
            return std::make_shared<ReadableAudioFile>(filename);
          },
          py::arg("cls"), py::arg("filename"))
      .def_static(
          "__new__",
          [](const py::object *, py::object filelike) {
            if (!isReadableFileLike(filelike)) {
              throw py::type_error(
                  "Expected either a filename or a file-like object (with "
                  "read, seek, seekable, and tell methods), but received: " +
                  py::repr(filelike).cast<std::string>());
            }

            return std::make_shared<ReadableAudioFile>(
                std::make_unique<PythonInputStream>(filelike));
          },
          py::arg("cls"), py::arg("file_like"))
      .def(
          "read", &ReadableAudioFile::read, py::arg("num_frames") = 0,
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
      .def(
          "read_raw", &ReadableAudioFile::readRaw, py::arg("num_frames") = 0,
          "Read the given number of frames (samples in each channel) from this "
          "audio file at the current position.\n\nAudio samples are returned "
          "as a multi-dimensional :class:`numpy.array` with the shape "
          "``(channels, samples)``; i.e.: a stereo audio file "
          "will have shape ``(2, <length>)``. Returned data is in the raw "
          "format stored by the underlying file (one of ``int8``, ``int16``, "
          "``int32``, or ``float32``).")
      .def("seekable", &ReadableAudioFile::isSeekable,
           "Returns True if this file is currently open and calls to seek() "
           "will work.")
      .def("seek", &ReadableAudioFile::seek, py::arg("position"),
           "Seek this file to the provided location in frames. Future reads "
           "will start from this position.")
      .def("tell", &ReadableAudioFile::tell,
           "Return the current position of the read pointer in this audio "
           "file, in frames. This value will increase as :meth:`read` is "
           "called, and may decrease if :meth:`seek` is called.")
      .def("close", &ReadableAudioFile::close,
           "Close this file, rendering this object unusable.")
      .def("__enter__", &ReadableAudioFile::enter,
           "Use this :class:`ReadableAudioFile` as a context manager, "
           "automatically closing the file and releasing resources when the "
           "context manager exits.")
      .def("__exit__", &ReadableAudioFile::exit,
           "Stop using this :class:`ReadableAudioFile` as a context manager, "
           "close the file, release its resources.")
      .def("__repr__",
           [](const ReadableAudioFile &file) {
             std::ostringstream ss;
             ss << "<pedalboard.io.ReadableAudioFile";

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
          "name", &ReadableAudioFile::getFilename,
          "The name of this file.\n\nIf this :class:`ReadableAudioFile` was "
          "opened from a file-like object, this will be ``None``.")
      .def_property_readonly("closed", &ReadableAudioFile::isClosed,
                             "True iff this file is closed (and no longer "
                             "usable), False otherwise.")
      .def_property_readonly("samplerate", &ReadableAudioFile::getSampleRate,
                             "The sample rate of this file in samples "
                             "(per channel) per second (Hz).")
      .def_property_readonly("num_channels", &ReadableAudioFile::getNumChannels,
                             "The number of channels in this file.")
      .def_property_readonly(
          "frames", &ReadableAudioFile::getLengthInSamples,
          "The total number of frames (samples per "
          "channel) in this file.\n\nFor example, if this "
          "file contains 10 seconds of stereo audio at sample "
          "rate of 44,100 Hz, ``frames`` will return ``441,000``.")
      .def_property_readonly("duration", &ReadableAudioFile::getDuration,
                             "The duration of this file in seconds (``frames`` "
                             "divided by ``samplerate``).")
      .def_property_readonly(
          "file_dtype", &ReadableAudioFile::getFileDatatype,
          "The data type (``\"int16\"``, ``\"float32\"``, etc) stored "
          "natively by this file.\n\nNote that :meth:`read` will always "
          "return a ``float32`` array, regardless of the value of this "
          "property. Use :meth:`read_raw` to read data from the file in its "
          "``file_dtype``.")
      .def(
          "resampled_to",
          [](std::shared_ptr<ReadableAudioFile> file, double targetSampleRate,
             ResamplingQuality quality) {
            return std::make_shared<ResampledReadableAudioFile>(
                file, targetSampleRate, quality);
          },
          py::arg("target_sample_rate"),
          py::arg("quality") = ResamplingQuality::WindowedSinc,
          "Return a :class:`ResampledReadableAudioFile` that will "
          "automatically resample this :class:`ReadableAudioFile` to the "
          "provided `target_sample_rate`, using a constant amount of "
          "memory.\n\n*Introduced in v0.6.0.*");

  m.def("get_supported_read_formats", []() {
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

    std::sort(
        output.begin(), output.end(),
        [](const std::string lhs, const std::string rhs) { return lhs < rhs; });

    return output;
  });
}
} // namespace Pedalboard
