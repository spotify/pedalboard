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

namespace py = pybind11;

namespace Pedalboard {

static constexpr const unsigned int DEFAULT_AUDIO_BUFFER_SIZE_FRAMES = 8192;

class AudioFile {};

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
      case 32:
        return "int32";
      case 64:
        return "int64";
      default:
        return "unknown";
      }
    }
  }

  py::array_t<float> read(long long numSamples) {
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

      if (!reader->read(channelPointers, numChannels, currentPosition,
                        numSamples)) {
        throw std::runtime_error("Failed to read from file.");
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

  std::string getFilename() const { return filename; }

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
}; // namespace Pedalboard

bool isInteger(double value) {
  double intpart;
  return modf(value, &intpart) == 0.0;
}

class WriteableAudioFile
    : public AudioFile,
      public std::enable_shared_from_this<WriteableAudioFile> {
public:
  WriteableAudioFile(std::string filename, double writeSampleRate,
                     int numChannels = 1, int bitDepth = 16,
                     std::optional<std::string> qualityString = {})
      : filename(filename) {
    pybind11::gil_scoped_release release;

    if (!isInteger(writeSampleRate)) {
      throw std::domain_error(
          "Opening an audio file for writing requires an integer sample rate.");
    }

    if (writeSampleRate == 0) {
      throw std::domain_error(
          "Opening an audio file for writing requires a non-zero sample rate.");
    }

    if (numChannels == 0) {
      throw py::type_error("Opening an audio file for writing requires a "
                           "non-zero num_channels.");
    }

    formatManager.registerBasicFormats();
    juce::File file(filename);

    std::unique_ptr<juce::FileOutputStream> outputStream =
        std::make_unique<juce::FileOutputStream>(file);
    if (!outputStream->openedOk()) {
      throw std::domain_error("Unable to open audio file for writing: " +
                              filename);
    }

    outputStream->setPosition(0);
    outputStream->truncate();

    juce::AudioFormat *format =
        formatManager.findFormatForFileExtension(file.getFileExtension());

    if (!format) {
      if (file.getFileExtension().isEmpty()) {
        throw std::domain_error("No file extension provided - cannot detect "
                                "audio format to write with for file path: " +
                                filename);
      }

      throw std::domain_error(
          "Unable to detect audio format for file extension: " +
          file.getFileExtension().toStdString());
    }

    // Detect the quality level to use based on the string passed in.
    // TODO: implement me!
    int qualityOptionIndex = 0;

    juce::StringPairArray emptyMetadata;
    writer.reset(format->createWriterFor(outputStream.get(), writeSampleRate,
                                         numChannels, bitDepth, emptyMetadata,
                                         qualityOptionIndex));
    if (!writer) {
      // Check common errors first:
      juce::Array<int> possibleSampleRates = format->getPossibleSampleRates();

      if (possibleSampleRates.isEmpty()) {
        throw std::domain_error(
            file.getFileExtension().toStdString() +
            " audio files are not writable with Pedalboard.");
      }

      if (!possibleSampleRates.contains((int)writeSampleRate)) {
        std::ostringstream sampleRateString;
        for (int i = 0; i < possibleSampleRates.size(); i++) {
          sampleRateString << possibleSampleRates[i];
          if (i < possibleSampleRates.size() - 1)
            sampleRateString << ", ";
        }
        throw std::domain_error(
            format->getFormatName().toStdString() +
            " audio files do not support the provided sample rate of " +
            std::to_string(writeSampleRate) +
            "Hz. Supported sample rates: " + sampleRateString.str());
      }

      juce::Array<int> possibleBitDepths = format->getPossibleBitDepths();

      if (possibleBitDepths.isEmpty()) {
        throw std::domain_error(
            file.getFileExtension().toStdString() +
            " audio files are not writable with Pedalboard.");
      }

      if (!possibleBitDepths.contains((int)bitDepth)) {
        std::ostringstream bitDepthString;
        for (int i = 0; i < possibleBitDepths.size(); i++) {
          bitDepthString << possibleBitDepths[i];
          if (i < possibleBitDepths.size() - 1)
            bitDepthString << ", ";
        }
        throw std::domain_error(
            format->getFormatName().toStdString() +
            " audio files do not support the provided bit depth of " +
            std::to_string(bitDepth) +
            " bits. Supported bit depths: " + bitDepthString.str());
      }

      throw std::domain_error("Unable to create audio file writer: " +
                              filename);
    } else {
      outputStream.release();
    }
  }

  template <typename SampleType>
  void write(py::array_t<SampleType, py::array::c_style> inputArray) {
    const juce::ScopedLock scopedLock(objectLock);

    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    py::buffer_info inputInfo = inputArray.request();

    unsigned int numChannels = 0;
    unsigned int numSamples = 0;
    ChannelLayout inputChannelLayout = detectChannelLayout(inputArray);

    // Release the GIL when we do the writing, after we
    // already have a reference to the input array:
    pybind11::gil_scoped_release release;

    if (inputInfo.ndim == 1) {
      numSamples = inputInfo.shape[0];
      numChannels = 1;
    } else if (inputInfo.ndim == 2) {
      // Try to auto-detect the channel layout from the shape
      if (inputInfo.shape[0] == getNumChannels() &&
          inputInfo.shape[1] == getNumChannels()) {
        throw std::runtime_error(
            "Unable to determine shape of audio input! Both dimensions have "
            "the same shape. Expected " +
            std::to_string(getNumChannels()) +
            "-channel audio, with one dimension larger than the other.");
      } else if (inputInfo.shape[1] == getNumChannels()) {
        numSamples = inputInfo.shape[0];
        numChannels = inputInfo.shape[1];
      } else if (inputInfo.shape[0] == getNumChannels()) {
        numSamples = inputInfo.shape[1];
        numChannels = inputInfo.shape[0];
      } else {
        throw std::runtime_error(
            "Unable to determine shape of audio input! Expected " +
            std::to_string(getNumChannels()) + "-channel audio.");
      }
    } else {
      throw std::runtime_error(
          "Number of input dimensions must be 1 or 2 (got " +
          std::to_string(inputInfo.ndim) + ").");
    }

    if (numChannels == 0) {
      // No work to do.
      return;
    } else if (numChannels != getNumChannels()) {
      throw std::runtime_error(
          "WritableAudioFile was opened with num_channels=" +
          std::to_string(getNumChannels()) +
          ", but was passed an array containing " +
          std::to_string(numChannels) + "-channel audio!");
    }

    // Depending on the input channel layout, we need to copy data
    // differently. This loop is duplicated here to move the if statement
    // outside of the tight loop, as we don't need to re-check that the input
    // channel is still the same on every iteration of the loop.
    switch (inputChannelLayout) {
    case ChannelLayout::Interleaved: {
      std::vector<std::vector<SampleType>> deinterleaveBuffers;

      // Use a temporary buffer to chunk the audio input
      // and pass it into the writer, chunk by chunk, rather
      // than de-interleaving the entire buffer at once:
      deinterleaveBuffers.resize(numChannels);

      const SampleType **channelPointers =
          (const SampleType **)alloca(numChannels * sizeof(SampleType *));
      for (int startSample = 0; startSample < numSamples;
           startSample += DEFAULT_AUDIO_BUFFER_SIZE_FRAMES) {
        int samplesToWrite = std::min(numSamples - startSample,
                                      DEFAULT_AUDIO_BUFFER_SIZE_FRAMES);

        for (int c = 0; c < numChannels; c++) {
          deinterleaveBuffers[c].resize(samplesToWrite);
          channelPointers[c] = deinterleaveBuffers[c].data();

          // We're de-interleaving the data here, so we can't use copyFrom.
          for (unsigned int i = 0; i < samplesToWrite; i++) {
            deinterleaveBuffers[c][i] =
                ((SampleType
                      *)(inputInfo.ptr))[((i + startSample) * numChannels) + c];
          }
        }

        if (!write(channelPointers, numChannels, samplesToWrite)) {
          throw std::runtime_error("Unable to write data to audio file.");
        }
      }

      break;
    }
    case ChannelLayout::NotInterleaved: {
      // We can just pass all the data to write:
      const SampleType **channelPointers =
          (const SampleType **)alloca(numChannels * sizeof(SampleType *));
      for (int c = 0; c < numChannels; c++) {
        channelPointers[c] = ((SampleType *)inputInfo.ptr) + (numSamples * c);
      }
      if (!write(channelPointers, numChannels, numSamples)) {
        throw std::runtime_error("Unable to write data to audio file.");
      }
      break;
    }
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }

    framesWritten += numSamples;
  }

  template <typename TargetType, typename InputType,
            unsigned int bufferSize = DEFAULT_AUDIO_BUFFER_SIZE_FRAMES>
  bool writeConvertingTo(const InputType **channels, int numChannels,
                         unsigned int numSamples) {
    printf("in writeConvertingTo<%s, %s>(%d, %d)\n", typeid(TargetType).name(),
           typeid(InputType).name(), numChannels, numSamples);
    std::vector<std::vector<TargetType>> targetTypeBuffers;
    targetTypeBuffers.resize(numChannels);

    const TargetType **channelPointers =
        (const TargetType **)alloca(numChannels * sizeof(TargetType *));
    for (unsigned int startSample = 0; startSample < numSamples;
         startSample += bufferSize) {
      int samplesToWrite = std::min(numSamples - startSample, bufferSize);

      for (int c = 0; c < numChannels; c++) {
        targetTypeBuffers[c].resize(samplesToWrite);
        channelPointers[c] = targetTypeBuffers[c].data();

        if constexpr (std::is_integral<InputType>::value) {
          if constexpr (std::is_integral<TargetType>::value) {
            for (unsigned int i = 0; i < samplesToWrite; i++) {
              // Left-align the samples to use all 32 bits, as JUCE requires:
              targetTypeBuffers[c][i] =
                  ((int)channels[c][startSample + i])
                  << (std::numeric_limits<int>::digits -
                      std::numeric_limits<InputType>::digits);
            }
          } else if constexpr (std::is_same<TargetType, float>::value) {
            constexpr auto scaleFactor =
                1.0f / static_cast<float>(std::numeric_limits<int>::max());
            juce::FloatVectorOperations::convertFixedToFloat(
                targetTypeBuffers[c].data(), channels[c] + startSample,
                scaleFactor, samplesToWrite);
          } else {
            // We should never get here - this would only be true
            // if converting to double, which no formats require:
            static_assert(std::is_integral<InputType>::value &&
                              std::is_same<TargetType, double>::value,
                          "Can't convert to double");
          }
        } else {
          if constexpr (std::is_integral<TargetType>::value) {
            // We should never get here - this would only be true
            // if converting float to int, which JUCE handles for us:
            static_assert(std::is_integral<TargetType>::value &&
                              !std::is_integral<InputType>::value,
                          "Can't convert float to int");
          } else {
            // Converting double to float:
            for (unsigned int i = 0; i < samplesToWrite; i++) {
              targetTypeBuffers[c][i] = channels[c][startSample + i];
            }
          }
        }
      }

      if (!write(channelPointers, numChannels, samplesToWrite)) {
        return false;
      }
    }

    return true;
  }

  template <typename SampleType>
  bool write(const SampleType **channels, int numChannels,
             unsigned int numSamples) {
    printf("in write<%s>(%d, %d)\n", typeid(SampleType).name(), numChannels,
           numSamples);
    if constexpr (std::is_integral<SampleType>::value) {
      if constexpr (std::is_same<SampleType, int>::value) {
        if (writer->isFloatingPoint()) {
          return writeConvertingTo<float>(channels, numChannels, numSamples);
        } else {
          // When supplying int32 data, files come out inverted when writing on
          // Windows! Try to get to the bottom of this:
          printf("About to pass %d samples of %d-channel %s data to "
                 "writer->write\n",
                 numSamples, numChannels, typeid(SampleType).name());
          return writer->write(channels, numSamples);
        }
      } else {
        return writeConvertingTo<int>(channels, numChannels, numSamples);
      }
    } else if constexpr (std::is_same<SampleType, float>::value) {
      if (writer->isFloatingPoint()) {
        // Just pass the floating point data into the writer as if it were
        // integer data. If the writer requires floating-point input data, this
        // works (and is documented!)
        return writer->write((const int **)channels, numSamples);
      } else {
        // Convert floating-point to fixed point, but let JUCE do that for us:
        return writer->writeFromFloatArrays(channels, numChannels, numSamples);
      }
    } else {
      // We must have double-format data:
      return writeConvertingTo<float>(channels, numChannels, numSamples);
    }
  }

  void flush() {
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");
    const juce::ScopedLock scopedLock(objectLock);
    pybind11::gil_scoped_release release;

    if (!writer->flush()) {
      throw std::runtime_error(
          "Unable to flush audio file; is the underlying file seekable?");
    }
  }

  void close() {
    if (!writer)
      throw std::runtime_error("Cannot close closed file.");
    const juce::ScopedLock scopedLock(objectLock);
    writer.reset();
  }

  bool isClosed() const {
    const juce::ScopedLock scopedLock(objectLock);
    return !writer;
  }

  double getSampleRate() const {
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");
    return writer->getSampleRate();
  }

  std::string getFilename() const { return filename; }

  long getFramesWritten() const { return framesWritten; }

  long getNumChannels() const {
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");
    return writer->getNumChannels();
  }

  std::shared_ptr<WriteableAudioFile> enter() { return shared_from_this(); }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    close();
  }

private:
  juce::AudioFormatManager formatManager;
  std::string filename;
  std::unique_ptr<juce::AudioFormatWriter> writer;
  juce::CriticalSection objectLock;
  int framesWritten = 0;
};

inline void init_audiofile(py::module &m) {
  py::class_<AudioFile, std::shared_ptr<AudioFile>>(
      m, "AudioFile", "A base class for readable and writeable audio files.")
      .def(py::init<>()) // Make this class effectively abstract; we can only
                         // instantiate subclasses.
      .def_static(
          "__new__",
          [](const py::object *, std::string filename, std::string mode) {
            if (mode == "r") {
              return std::make_shared<ReadableAudioFile>(filename);
            } else if (mode == "w") {
              throw py::type_error("Opening an audio file for writing requires "
                                   "samplerate and num_channels arguments.");
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") and write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("filename"), py::arg("mode") = "r")
      .def_static(
          "__new__",
          [](const py::object *, std::string filename, std::string mode,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::string> quality) {
            if (mode == "r") {
              throw py::type_error(
                  "Opening an audio file for reading does not require "
                  "samplerate, num_channels, bit_depth, or quality arguments - "
                  "these parameters "
                  "will be read from the file.");
            } else if (mode == "w") {
              if (!sampleRate) {
                throw py::type_error(
                    "Opening an audio file for writing requires a samplerate "
                    "argument to be provided.");
              }

              return std::make_shared<WriteableAudioFile>(
                  filename, sampleRate.value(), numChannels, bitDepth, quality);
            } else {
              throw py::type_error("AudioFile instances can only be opened in "
                                   "read mode (\"r\") and write mode (\"w\").");
            }
          },
          py::arg("cls"), py::arg("filename"), py::arg("mode") = "w",
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none());

  py::class_<ReadableAudioFile, AudioFile, std::shared_ptr<ReadableAudioFile>>(
      m, "ReadableAudioFile",
      "An audio file reader interface, with native support for Ogg Vorbis, "
      "MP3, WAV, FLAC, and AIFF files on all operating systems. On some "
      "platforms, other formats may also be readable. (Use "
      "AudioFile.supported_read_formats and AudioFile.supported_write_formats "
      "to see which formats are supported on the current platform.)")
      .def(py::init([](std::string filename) {
             return std::make_shared<ReadableAudioFile>(filename);
           }),
           py::arg("filename"))
      .def_static(
          "__new__",
          [](const py::object *, std::string filename) {
            return std::make_shared<ReadableAudioFile>(filename);
          },
          py::arg("cls"), py::arg("filename"))
      .def("read", &ReadableAudioFile::read, py::arg("num_frames") = 0,
           "Read the given number of frames (samples in each channel) from the "
           "audio file at the current position. Audio samples are returned in "
           "the shape (channels, samples); i.e.: a stereo audio file will have "
           "shape (2, <length>). Returned data is always in float32 format.")
      .def("seekable", &ReadableAudioFile::isSeekable,
           "Returns True if the file is currently open and calls to seek() "
           "will work.")
      .def("seek", &ReadableAudioFile::seek, py::arg("position"),
           "Seek the file to the provided location in frames.")
      .def("tell", &ReadableAudioFile::tell,
           "Fetch the position in the audio file, in frames.")
      .def("close", &ReadableAudioFile::close,
           "Close the file, rendering this object unusable.")
      .def("__enter__", &ReadableAudioFile::enter)
      .def("__exit__", &ReadableAudioFile::exit)
      .def("__repr__",
           [](const ReadableAudioFile &file) {
             std::ostringstream ss;
             ss << "<pedalboard.io.ReadableAudioFile";
             if (!file.getFilename().empty()) {
               ss << " filename=" << file.getFilename();
             }
             if (file.isClosed()) {
               ss << " closed";
             } else {
               ss << " samplerate=" << file.getSampleRate();
               ss << " channels=" << file.getNumChannels();
               ss << " frames=" << file.getLengthInSamples();
               ss << " file_dtype=" << file.getFileDatatype();
             }
             ss << " at " << &file;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "name", &ReadableAudioFile::getFilename,
          "If the file has been closed, this property will be True.")
      .def_property_readonly(
          "closed", &ReadableAudioFile::isClosed,
          "If the file has been closed, this property will be True.")
      .def_property_readonly("samplerate", &ReadableAudioFile::getSampleRate,
                             "Fetch the sample rate of the file in samples "
                             "(per channel) per second (Hz).")
      .def_property_readonly("channels", &ReadableAudioFile::getNumChannels,
                             "Fetch the number of channels in the file.")
      .def_property_readonly("frames", &ReadableAudioFile::getLengthInSamples,
                             "Fetch the total number of frames (samples per "
                             "channel) in the file.")
      .def_property_readonly(
          "duration", &ReadableAudioFile::getDuration,
          "Fetch the duration of this file (frames divided by sample rate).")
      .def_property_readonly(
          "file_dtype", &ReadableAudioFile::getFileDatatype,
          "Fetch the data type stored natively by the file. Note that read() "
          "will always return a float32 array, regardless of this method.");

  py::class_<WriteableAudioFile, AudioFile,
             std::shared_ptr<WriteableAudioFile>>(
      m, "WriteableAudioFile",
      "An audio file writer interface, with native support for Ogg Vorbis, "
      "MP3, WAV, FLAC, and AIFF files on all operating systems.")
      .def(py::init([](std::string filename, double sampleRate, int numChannels,
                       int bitDepth, std::optional<std::string> quality) {
             return std::make_shared<WriteableAudioFile>(
                 filename, sampleRate, numChannels, bitDepth, quality);
           }),
           py::arg("filename"), py::arg("samplerate"),
           py::arg("num_channels") = 1, py::arg("bit_depth") = 16,
           py::arg("quality") = py::none())
      .def_static(
          "__new__",
          [](const py::object *, std::string filename,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::string> quality) {
            if (!sampleRate) {
              throw py::type_error(
                  "Opening an audio file for writing requires a samplerate "
                  "argument to be provided.");
            }
            return std::make_shared<WriteableAudioFile>(
                filename, sampleRate.value(), numChannels, bitDepth, quality);
          },
          py::arg("cls"), py::arg("filename"),
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array_t<char> samples) {
            file.write<char>(samples);
          },
          py::arg("samples").noconvert())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array_t<short> samples) {
            file.write<short>(samples);
          },
          py::arg("samples").noconvert())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array_t<int> samples) {
            file.write<int>(samples);
          },
          py::arg("samples").noconvert())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array_t<float> samples) {
            file.write<float>(samples);
          },
          py::arg("samples").noconvert())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array_t<double> samples) {
            file.write<double>(samples);
          },
          py::arg("samples").noconvert())
      .def("flush", &WriteableAudioFile::flush)
      .def("close", &WriteableAudioFile::close,
           "Close the file, rendering this object unusable.")
      .def("__enter__", &WriteableAudioFile::enter)
      .def("__exit__", &WriteableAudioFile::exit)
      .def("__repr__",
           [](const WriteableAudioFile &file) {
             std::ostringstream ss;
             ss << "<pedalboard.io.WriteableAudioFile";
             if (!file.getFilename().empty()) {
               ss << " filename=" << file.getFilename();
             }
             if (file.isClosed()) {
               ss << " closed";
             } else {
               ss << " samplerate=" << file.getSampleRate();
               ss << " channels=" << file.getNumChannels();
               ss << " frames=" << file.getFramesWritten();
             }
             ss << " at " << &file;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "closed", &WriteableAudioFile::isClosed,
          "If the file has been closed, this property will be True.")
      .def_property_readonly("samplerate", &WriteableAudioFile::getSampleRate,
                             "Fetch the sample rate of the file in samples "
                             "(per channel) per second (Hz).")
      .def_property_readonly("channels", &WriteableAudioFile::getNumChannels,
                             "Fetch the number of channels in the file.")
      .def_property_readonly("frames", &WriteableAudioFile::getFramesWritten,
                             "Fetch the total number of frames (samples per "
                             "channel) written to the file so far.");

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

  m.def("get_supported_write_formats", []() {
    // JUCE doesn't support writing other formats out-of-the-box on all
    // platforms, and there's no easy way to tell which formats are supported
    // without attempting to create an AudioFileWriter object - so this list is
    // hardcoded for now.
    const std::vector<std::string> formats = {".aiff", ".flac", ".ogg", ".wav"};
    return formats;
  });
}
} // namespace Pedalboard