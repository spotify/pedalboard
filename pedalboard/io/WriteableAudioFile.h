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
#include "LameMP3AudioFormat.h"
#include "PythonOutputStream.h"

namespace py = pybind11;

namespace Pedalboard {

bool isInteger(double value) {
  double intpart;
  return modf(value, &intpart) == 0.0;
}

// Per-format overrides for the "worst"/"best"/"fastest"/"slowest"
static inline std::map<std::string, std::pair<std::string, std::string>>
    MIN_MAX_QUALITY_OPTIONS = {
        {"MP3", {"V9 (smallest)", "V0 (best)"}},
};

int determineQualityOptionIndex(juce::AudioFormat *format,
                                const std::string inputString) {
  // Detect the quality level to use based on the string passed in:
  juce::StringArray possibleQualityOptions = format->getQualityOptions();
  int qualityOptionIndex = -1;

  std::string qualityString = juce::String(inputString).trim().toStdString();

  if (!qualityString.empty()) {
    if (inputString == "worst" || inputString == "best" ||
        inputString == "slowest" || inputString == "fastest") {

      const std::string formatName = format->getFormatName().toStdString();

      if (MIN_MAX_QUALITY_OPTIONS.count(formatName) == 1) {
        auto minMaxOptions = MIN_MAX_QUALITY_OPTIONS[formatName];
        if (inputString == "worst" || inputString == "fastest") {
          return possibleQualityOptions.indexOf(minMaxOptions.first);
        } else {
          return possibleQualityOptions.indexOf(minMaxOptions.second);
        }
      } else {
        if (inputString == "worst" || inputString == "fastest" ||
            possibleQualityOptions.isEmpty()) {
          return 0;
        } else {
          return possibleQualityOptions.size() - 1;
        }
      }
    }

    if (!possibleQualityOptions.size()) {
      throw std::domain_error("Unable to parse provided quality value (" +
                              qualityString + "). " +
                              format->getFormatName().toStdString() +
                              "s do not accept quality settings.");
    }

    // Try to match the string against the available options. An exact match
    // is preferred (ignoring case):
    if (qualityOptionIndex == -1 &&
        possibleQualityOptions.contains(qualityString, true)) {
      qualityOptionIndex = possibleQualityOptions.indexOf(qualityString, true);
    }

    // And if no exact match was found, try casting to an integer:
    if (qualityOptionIndex == -1) {
      int numLeadingDigits = 0;
      for (int i = 0; i < qualityString.size(); i++) {
        if (juce::CharacterFunctions::isDigit(qualityString[i])) {
          numLeadingDigits++;
        } else {
          break;
        }
      }

      if (numLeadingDigits) {
        std::string leadingIntValue = qualityString.substr(0, numLeadingDigits);

        // Check to see if any of the valid options start with this option,
        // but make sure we don't select only the prefix of a number
        // (i.e.: if someone gives us "32", don't select "320 kbps")
        for (int i = 0; i < possibleQualityOptions.size(); i++) {
          const juce::String &option = possibleQualityOptions[i];
          if (option.startsWith(leadingIntValue) &&
              option.length() > leadingIntValue.size() &&
              !juce::CharacterFunctions::isDigit(
                  option[leadingIntValue.size()])) {
            qualityOptionIndex = i;
            break;
          }
        }
      } else {
        // If our search string doesn't start with leading digits,
        // check for a substring:
        for (int i = 0; i < possibleQualityOptions.size(); i++) {
          if (possibleQualityOptions[i].containsIgnoreCase(qualityString)) {
            qualityOptionIndex = i;
            break;
          }
        }
      }
    }

    // If we get here, we received a string we were unable to parse,
    // so the user should probably know about it:
    if (qualityOptionIndex == -1) {
      throw std::domain_error(
          "Unable to parse provided quality value (" + qualityString +
          "). Valid values for " + format->getFormatName().toStdString() +
          "s are: " +
          possibleQualityOptions.joinIntoString(", ").toStdString());
    }
  }

  if (qualityOptionIndex == -1) {
    if (possibleQualityOptions.size()) {
      // Choose the best quality by default if possible:
      qualityOptionIndex = possibleQualityOptions.size() - 1;
    } else {
      qualityOptionIndex = 0;
    }
  }

  return qualityOptionIndex;
}

/**
 * A tiny RAII wrapper around juce::FileOutputStream that
 * deletes the file on destruction if it was not written to.
 */
class AutoDeleteFileOutputStream : public juce::FileOutputStream {
public:
  AutoDeleteFileOutputStream(const juce::File &fileToWriteTo,
                             size_t bufferSizeToUse = 16384,
                             bool deleteFileOnDestruction = true)
      : juce::FileOutputStream(fileToWriteTo, bufferSizeToUse),
        deleteFileOnDestruction(deleteFileOnDestruction) {}

  static std::unique_ptr<AutoDeleteFileOutputStream>
  createOutputStream(const juce::File &fileToWriteTo,
                     size_t bufferSizeToUse = 16384) {
    return std::make_unique<AutoDeleteFileOutputStream>(
        fileToWriteTo, bufferSizeToUse, !fileToWriteTo.existsAsFile());
  };

  juce::Result truncate() {
    deleteFileOnDestruction = false;
    return juce::FileOutputStream::truncate();
  }

  virtual bool write(const void *bytes, size_t len) override {
    if (!hasWrittenToFile) {
      setPosition(0);
      truncate();
      hasWrittenToFile = true;
    }

    deleteFileOnDestruction = false;
    return juce::FileOutputStream::write(bytes, len);
  }

  virtual juce::int64 getPosition() override {
    if (!hasWrittenToFile)
      return 0;
    return juce::FileOutputStream::getPosition();
  }

  virtual bool writeRepeatedByte(juce::uint8 byte,
                                 size_t numTimesToRepeat) override {
    if (!hasWrittenToFile) {
      setPosition(0);
      truncate();
      hasWrittenToFile = true;
    }

    deleteFileOnDestruction = false;
    return juce::FileOutputStream::writeRepeatedByte(byte, numTimesToRepeat);
  }

  ~AutoDeleteFileOutputStream() override {
    if (deleteFileOnDestruction) {
      getFile().deleteFile();
    }
  }

private:
  bool deleteFileOnDestruction = false;
  bool hasWrittenToFile = false;
};

class WriteableAudioFile
    : public AudioFile,
      public std::enable_shared_from_this<WriteableAudioFile> {
public:
  WriteableAudioFile(
      std::string filename, double writeSampleRate, int numChannels = 1,
      int bitDepth = 16,
      std::optional<std::variant<std::string, float>> qualityInput = {})
      : WriteableAudioFile(filename, nullptr, writeSampleRate, numChannels,
                           bitDepth, qualityInput) {}

  WriteableAudioFile(
      std::string filename,
      std::unique_ptr<juce::OutputStream> providedOutputStream,
      double writeSampleRate, int numChannels = 1, int bitDepth = 16,
      std::optional<std::variant<std::string, float>> qualityInput = {}) {
    pybind11::gil_scoped_release release;

    // This is kind of silly, as nobody else has a reference
    // to this object yet; but it prevents some juce assertions in debug builds:
    juce::ScopedWriteLock writeLock(objectLock);

    if (!isInteger(writeSampleRate)) {
      throw py::type_error(
          "Opening an audio file for writing requires an integer sample rate.");
    }

    if (writeSampleRate == 0) {
      throw std::domain_error(
          "Opening an audio file for writing requires a non-zero sample rate.");
    }

    if (numChannels == 0) {
      throw std::domain_error("Opening an audio file for writing requires a "
                              "non-zero num_channels.");
    }

    // Tiny quality-of-life improvement to try to detect if people have swapped
    // the num_channels and samplerate arguments:
    if ((numChannels == 48000 || numChannels == 44100 || numChannels == 22050 ||
         numChannels == 11025) &&
        writeSampleRate < 8000) {
      throw std::domain_error(
          "Arguments of num_channels=" + std::to_string(numChannels) +
          " and samplerate=" + std::to_string(writeSampleRate) +
          " were provided when opening a file for writing. These arguments "
          "appear to be flipped, and may cause an invalid audio file to be "
          "written. Try reversing the order of the samplerate "
          "and num_channels arguments.");
    }

    registerPedalboardAudioFormats(formatManager, true);

    std::unique_ptr<juce::OutputStream> outputStream;
    juce::AudioFormat *format = nullptr;
    std::string extension;

    if (PythonOutputStream *pythonOutputStream =
            static_cast<PythonOutputStream *>(providedOutputStream.get())) {
      // Use the pythonOutputStream's filename if possible, falling back to the
      // provided filename string (which should contain an extension) if
      // necessary.
      if (!filename.empty()) {
        extension = filename;
      } else if (auto streamName = pythonOutputStream->getFilename()) {
        // Dummy-stream-filename added here to avoid a JUCE assertion
        // if the stream name doesn't start with a slash.
        juce::File file(
            juce::String(juce::File::getSeparatorString()).toStdString() +
            "dummy-stream-filename-" + *streamName);
        extension = file.getFileExtension().toStdString();
      }

      format = formatManager.findFormatForFileExtension(extension);

      if (!format) {
        if (pythonOutputStream->getFilename()) {
          throw std::domain_error("Unable to detect audio format to use for "
                                  "file-like object with filename: " +
                                  *(pythonOutputStream->getFilename()));
        } else {
          throw std::domain_error(
              "Provided format argument (\"" + filename +
              "\") does not correspond to a supported file type.");
        }
      }

      unsafeOutputStream = pythonOutputStream;
      pythonOutputStream->setObjectLock(&objectLock);
      outputStream = std::move(providedOutputStream);
    } else {
      if (providedOutputStream) {
        outputStream = std::move(providedOutputStream);
      } else {
        juce::File file(filename);
        extension = file.getFileExtension().toStdString();

        outputStream = AutoDeleteFileOutputStream::createOutputStream(file);
        if (!static_cast<AutoDeleteFileOutputStream *>(outputStream.get())
                 ->openedOk()) {
          throw std::domain_error("Unable to open audio file for writing: " +
                                  filename);
        }
      }

      format = formatManager.findFormatForFileExtension(extension);

      if (!format) {
        if (extension.empty()) {
          throw std::domain_error("No file extension provided - cannot detect "
                                  "audio format to write with for filename: " +
                                  filename);
        }

        throw std::domain_error(
            "Unable to detect audio format for file extension: " + extension);
      }
    }

    // Normalize the input to a string here, as we need to do parsing anyways:
    std::string qualityString;
    if (qualityInput) {
      if (auto *q = std::get_if<std::string>(&(*qualityInput))) {
        qualityString = *q;
      } else if (auto *q = std::get_if<float>(&(*qualityInput))) {
        if (isInteger(*q)) {
          qualityString = std::to_string((int)*q);
        } else {
          qualityString = std::to_string(*q);
        }
      } else {
        throw std::runtime_error("Unknown quality type!");
      }
    }

    int qualityOptionIndex = determineQualityOptionIndex(format, qualityString);
    if (format->getQualityOptions().size() > qualityOptionIndex) {
      quality = format->getQualityOptions()[qualityOptionIndex].toStdString();
    }

    juce::StringPairArray emptyMetadata;
    writer.reset(format->createWriterFor(outputStream.get(), writeSampleRate,
                                         numChannels, bitDepth, emptyMetadata,
                                         qualityOptionIndex));
    if (!writer) {
      PythonException::raise();

      // Check common errors first:
      juce::Array<int> possibleSampleRates = format->getPossibleSampleRates();

      if (possibleSampleRates.isEmpty()) {
        throw std::domain_error(
            extension + " audio files are not writable with Pedalboard.");
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
            juce::String(writeSampleRate, 2).toStdString() +
            "Hz. Supported sample rates: " + sampleRateString.str());
      }

      juce::Array<int> possibleBitDepths = format->getPossibleBitDepths();

      if (possibleBitDepths.isEmpty()) {
        throw std::domain_error(
            extension + " audio files are not writable with Pedalboard.");
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

      std::string humanReadableQuality;
      if (qualityString.empty()) {
        humanReadableQuality = "None";
      } else {
        humanReadableQuality = qualityString;
      }

      throw std::domain_error(
          "Unable to create " + format->getFormatName().toStdString() +
          " writer with samplerate=" + std::to_string(writeSampleRate) +
          ", num_channels=" + std::to_string(numChannels) + ", bit_depth=" +
          std::to_string(bitDepth) + ", and quality=" + humanReadableQuality);
    } else {
      // If we have a writer object, it now owns the OutputStream we passed in
      // - so we need to release it before possibly throwing an exception, or
      // the stream will leak.
      outputStream.release();
      try {
        PythonException::raise();
      } catch (const std::exception &e) {
        // AudioFormatWriter objects may call .write() in their destructors,
        // and we need to hold the ScopedWriteLock if they do, so we explicitly
        // free the writer here instead of when WriteableAudioFile gets
        // deallocated. (Note that because the object hasn't finished
        // construction, its destructor will never be called directly.)
        writer.reset();
        throw;
      }
    }
  }

  ~WriteableAudioFile() {
    // We need to release the writer here, as it may call .write() in its
    // destructor, and we need to hold the ScopedWriteLock if it does:
    juce::ScopedWriteLock writeLock(objectLock);
    writer.reset();
  }

  /**
   * A generic type-dispatcher for all writes.
   * pybind11 supports dispatch here, but both pybind11-stubgen
   * and Sphinx currently (2022-07-16) struggle with how to render
   * docstrings of overloaded functions, so we don't overload.
   */
  void write(py::array inputArray) {
    switch (inputArray.dtype().char_()) {
    case 'f':
      return write<float>(py::array_t<float>(inputArray.release(), false));
    case 'd':
      return write<double>(py::array_t<double>(inputArray.release(), false));
    case 'b':
      return write<int8_t>(py::array_t<int8_t>(inputArray.release(), false));
    case 'h':
      return write<int16_t>(py::array_t<int16_t>(inputArray.release(), false));
    case 'i':
#ifdef JUCE_WINDOWS
    // On 64-bit Windows, int32 is a "long".
    case 'l':
#endif
      return write<int32_t>(py::array_t<int32_t>(inputArray.release(), false));
    default:
      throw py::type_error(
          "Writing audio requires an array with a datatype of int8, "
          "int16, int32, float32, or float64. (Got: " +
          py::str(inputArray.attr("dtype")).cast<std::string>() + ")");
    }
  }

  template <typename SampleType>
  void write(py::array_t<SampleType, py::array::c_style> inputArray) {
    const juce::ScopedReadLock scopedReadLock(objectLock);

    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    py::buffer_info inputInfo = inputArray.request();

    unsigned int numChannels = 0;
    unsigned int numSamples = 0;

    if (lastChannelLayout) {
      try {
        lastChannelLayout = detectChannelLayout(inputArray, {getNumChannels()});
      } catch (...) {
        // Use the last cached layout.
      }
    } else {
      // We have no cached layout; detect it now and raise if necessary:
      try {
        lastChannelLayout = detectChannelLayout(inputArray, {getNumChannels()});
      } catch (const std::exception &e) {
        throw std::runtime_error(
            std::string(e.what()) +
            " Provide a non-square array first to allow Pedalboard to "
            "determine which dimension corresponds with the number of channels "
            "and which dimension corresponds with the number of samples.");
      }
    }

    // Release the GIL when we do the writing, after we
    // already have a reference to the input array:
    pybind11::gil_scoped_release release;

    if (inputInfo.ndim == 1) {
      numSamples = inputInfo.shape[0];
      numChannels = 1;
    } else if (inputInfo.ndim == 2) {
      switch (*lastChannelLayout) {
      case ChannelLayout::Interleaved:
        numSamples = inputInfo.shape[0];
        numChannels = inputInfo.shape[1];
        break;
      case ChannelLayout::NotInterleaved:
        numSamples = inputInfo.shape[1];
        numChannels = inputInfo.shape[0];
        break;
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
          "WriteableAudioFile was opened with num_channels=" +
          std::to_string(getNumChannels()) +
          ", but was passed an array containing " +
          std::to_string(numChannels) + "-channel audio!");
    }

    // Depending on the input channel layout, we need to copy data
    // differently. This loop is duplicated here to move the if statement
    // outside of the tight loop, as we don't need to re-check that the input
    // channel is still the same on every iteration of the loop.
    switch (*lastChannelLayout) {
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

        bool writeSuccessful =
            write(channelPointers, numChannels, samplesToWrite);

        if (!writeSuccessful) {
          PythonException::raise();
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

      bool writeSuccessful = write(channelPointers, numChannels, numSamples);

      if (!writeSuccessful) {
        PythonException::raise();
        throw std::runtime_error("Unable to write data to audio file.");
      }
      break;
    }
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }

    {
      ScopedTryWriteLock scopedTryWriteLock(objectLock);
      if (!scopedTryWriteLock.isLocked()) {
        throw std::runtime_error(
            "Another thread is currently writing to this AudioFile. Note "
            "that using multiple concurrent writers on the same AudioFile "
            "object will produce nondeterministic results.");
      }

      framesWritten += numSamples;
    }
  }

  template <typename TargetType, typename InputType,
            unsigned int bufferSize = DEFAULT_AUDIO_BUFFER_SIZE_FRAMES>
  bool writeConvertingTo(const InputType **channels, int numChannels,
                         unsigned int numSamples) {
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
    if constexpr (std::is_integral<SampleType>::value) {
      if constexpr (std::is_same<SampleType, int>::value) {
        if (writer->isFloatingPoint()) {
          return writeConvertingTo<float>(channels, numChannels, numSamples);
        } else {
          ScopedTryWriteLock scopedTryWriteLock(objectLock);
          if (!scopedTryWriteLock.isLocked()) {
            throw std::runtime_error(
                "Another thread is currently writing to this AudioFile. Note "
                "that using multiple concurrent writers on the same AudioFile "
                "object will produce nondeterministic results.");
          }
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
        ScopedTryWriteLock scopedTryWriteLock(objectLock);
        if (!scopedTryWriteLock.isLocked()) {
          throw std::runtime_error(
              "Another thread is currently writing to this AudioFile. Note "
              "that using multiple concurrent writers on the same AudioFile "
              "object will produce nondeterministic results.");
        }
        return writer->write((const int **)channels, numSamples);
      } else {
        // Convert floating-point to fixed point, but let JUCE do that for us:
        ScopedTryWriteLock scopedTryWriteLock(objectLock);
        if (!scopedTryWriteLock.isLocked()) {
          throw std::runtime_error(
              "Another thread is currently writing to this AudioFile. Note "
              "that using multiple concurrent writers on the same AudioFile "
              "object will produce nondeterministic results.");
        }
        return writer->writeFromFloatArrays(channels, numChannels, numSamples);
      }
    } else {
      // We must have double-format data:
      return writeConvertingTo<float>(channels, numChannels, numSamples);
    }
  }

  void flush() {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    bool flushSucceeded = false;
    {
      pybind11::gil_scoped_release release;

      ScopedTryWriteLock scopedTryWriteLock(objectLock);
      if (!scopedTryWriteLock.isLocked()) {
        throw std::runtime_error(
            "Another thread is currently writing to this AudioFile. Note "
            "that using multiple concurrent writers on the same AudioFile "
            "object will produce nondeterministic results.");
      }
      flushSucceeded = writer->flush();
    }

    if (!flushSucceeded) {
      PythonException::raise();
      throw std::runtime_error(
          "Unable to flush audio file; is the underlying file seekable?");
    }
  }

  void close() {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("Cannot close closed file.");

    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently writing to this AudioFile; it cannot "
          "be closed until the other thread completes its operation.");
    }
    writer.reset();
  }

  bool isClosed() const {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    return !writer;
  }

  std::variant<double, long> getSampleRate() const {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    double integerPart;
    double fractionalPart = std::modf(writer->getSampleRate(), &integerPart);

    if (fractionalPart > 0) {
      return writer->getSampleRate();
    } else {
      return (long)(writer->getSampleRate());
    }
  }

  double getSampleRateAsDouble() const {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    return writer->getSampleRate();
  }

  std::string getFilename() const { return filename; }

  long getFramesWritten() const { return framesWritten; }

  std::optional<std::string> getQuality() const { return quality; }

  long getNumChannels() const {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");
    return writer->getNumChannels();
  }

  std::string getFileDatatype() const {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!writer)
      throw std::runtime_error("I/O operation on a closed file.");

    if (writer->isFloatingPoint()) {
      switch (writer->getBitsPerSample()) {
      case 16: // OGG returns 16-bit int data, but internally stores floats
      case 32:
        return "float32";
      case 64:
        return "float64";
      default:
        return "unknown";
      }
    } else {
      switch (writer->getBitsPerSample()) {
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

  std::shared_ptr<WriteableAudioFile> enter() { return shared_from_this(); }

  void exit(const py::object &type, const py::object &value,
            const py::object &traceback) {
    bool shouldThrow = PythonException::isPending();
    close();

    if (shouldThrow || PythonException::isPending())
      throw py::error_already_set();
  }

  PythonOutputStream *getPythonOutputStream() const {
    if (!filename.empty()) {
      return nullptr;
    }
    if (!writer) {
      return nullptr;
    }

    // the AudioFormatWriter retains exclusive ownership over the output stream,
    // and doesn't expose it - so we keep our own reference (which may be
    // deallocated out from under us!)
    return unsafeOutputStream;
  }

private:
  juce::AudioFormatManager formatManager;
  std::string filename;
  std::optional<std::string> quality;
  std::unique_ptr<juce::AudioFormatWriter> writer;
  PythonOutputStream *unsafeOutputStream = nullptr;
  juce::ReadWriteLock objectLock;
  int framesWritten = 0;
  std::optional<ChannelLayout> lastChannelLayout = {};
};

inline py::class_<WriteableAudioFile, AudioFile,
                  std::shared_ptr<WriteableAudioFile>>
declare_writeable_audio_file(py::module &m) {
  return py::class_<WriteableAudioFile, AudioFile,
                    std::shared_ptr<WriteableAudioFile>>(m,
                                                         "WriteableAudioFile",
                                                         R"(
A class that wraps an audio file for writing, with native support for Ogg Vorbis,
MP3, WAV, FLAC, and AIFF files on all operating systems. 

Use :meth:`pedalboard.io.get_supported_write_formats()` to see which
formats or file extensions are supported on the current platform.

Args:
    filename_or_file_like:
        The path to an output file to write to, or a seekable file-like
        binary object (like ``io.BytesIO``) to write to.

    samplerate:
        The sample rate of the audio that will be written to this file.
        All calls to the :meth:`write` method will assume this sample rate
        is used.

    num_channels:
        The number of channels in the audio that will be written to this file.
        All calls to the :meth:`write` method will expect audio with this many
        channels, and will throw an exception if the audio does not contain
        this number of channels.

    bit_depth:
        The bit depth (number of bits per sample) that will be written
        to this file. Used for raw formats like WAV and AIFF. Will have no effect
        on compressed formats like MP3 or Ogg Vorbis.

    quality:
        An optional string or number that indicates the quality level to use
        for the given audio compression codec. Different codecs have different
        compression quality values; numeric values like ``128`` and ``256`` will
        usually indicate the number of kilobits per second used by the codec.
        Some formats, like MP3, support more advanced options like ``V2`` (as
        specified by `the LAME encoder <https://lame.sourceforge.io/>`_) which
        may be passed as a string. The strings ``"best"``, ``"worst"``,
        ``"fastest"``, and ``"slowest"`` will also work for any codec.

.. note::
    You probably don't want to use this class directly: all of the parameters
    accepted by the :class:`WriteableAudioFile` constructor will be accepted by
    :class:`AudioFile` as well, as long as the ``"w"`` mode is passed as the
    second argument.

)");
}

inline void init_writeable_audio_file(
    py::module &m,
    py::class_<WriteableAudioFile, AudioFile,
               std::shared_ptr<WriteableAudioFile>> &pyWriteableAudioFile) {
  pyWriteableAudioFile
      .def(py::init([](std::string filename, double sampleRate, int numChannels,
                       int bitDepth,
                       std::optional<std::variant<std::string, float>> quality)
                        -> WriteableAudioFile * {
             // This definition is only here to provide nice docstrings.
             throw std::runtime_error(
                 "Internal error: __init__ should never be called, as this "
                 "class implements __new__.");
           }),
           py::arg("filename"), py::arg("samplerate"),
           py::arg("num_channels") = 1, py::arg("bit_depth") = 16,
           py::arg("quality") = py::none())
      .def(py::init(
               [](py::object filelike, double sampleRate, int numChannels,
                  int bitDepth,
                  std::optional<std::variant<std::string, float>> quality,
                  std::optional<std::string> format) -> WriteableAudioFile * {
                 // This definition is only here to provide nice docstrings.
                 throw std::runtime_error(
                     "Internal error: __init__ should never be called, as this "
                     "class implements __new__.");
               }),
           py::arg("file_like"), py::arg("samplerate"),
           py::arg("num_channels") = 1, py::arg("bit_depth") = 16,
           py::arg("quality") = py::none(), py::arg("format") = py::none())
      .def_static(
          "__new__",
          [](const py::object *, std::string filename,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::variant<std::string, float>> quality) {
            if (!sampleRate) {
              throw py::type_error(
                  "Opening an audio file for writing requires a samplerate "
                  "argument to be provided.");
            }
            return std::make_shared<WriteableAudioFile>(
                filename, *sampleRate, numChannels, bitDepth, quality);
          },
          py::arg("cls"), py::arg("filename"),
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none())
      .def_static(
          "__new__",
          [](const py::object *, py::object filelike,
             std::optional<double> sampleRate, int numChannels, int bitDepth,
             std::optional<std::variant<std::string, float>> quality,
             std::optional<std::string> format) {
            if (!sampleRate) {
              throw py::type_error(
                  "Opening an audio file for writing requires a samplerate "
                  "argument to be provided.");
            }
            if (!isWriteableFileLike(filelike)) {
              throw py::type_error(
                  "Expected either a filename or a file-like object (with "
                  "write, seek, seekable, and tell methods), but received: " +
                  py::repr(filelike).cast<std::string>());
            }

            auto stream = std::make_unique<PythonOutputStream>(filelike);
            if (!format && !stream->getFilename()) {
              throw py::type_error(
                  "Unable to infer audio file format for writing. Expected "
                  "either a \".name\" property on the provided file-like "
                  "object (" +
                  py::repr(filelike).cast<std::string>() +
                  ") or an explicit file format passed as the \"format=\" "
                  "argument.");
            }

            return std::make_shared<WriteableAudioFile>(
                format.value_or(""), std::move(stream), *sampleRate,
                numChannels, bitDepth, quality);
          },
          py::arg("cls"), py::arg("file_like"),
          py::arg("samplerate") = py::none(), py::arg("num_channels") = 1,
          py::arg("bit_depth") = 16, py::arg("quality") = py::none(),
          py::arg("format") = py::none())
      .def(
          "write",
          [](WriteableAudioFile &file, py::array samples) {
            file.write(samples);
          },
          py::arg("samples").noconvert(),
          "Encode an array of audio data and write "
          "it to this file. The number of channels in the array must match the "
          "number of channels used to open the file. The array may contain "
          "audio in any shape. If the file's bit depth or format does not "
          "match the provided data type, the audio will be automatically "
          "converted.\n\n"
          "Arrays of type int8, int16, int32, float32, and float64 are "
          "supported. If an array of an unsupported ``dtype`` is provided, a "
          "``TypeError`` will be raised.\n\n"
          ".. warning::\n    If an array of shape ``(num_channels, "
          "num_channels)`` is passed to this method before any other audio "
          "data is provided, an exception will be thrown, as the method will "
          "not be able to infer which dimension of the input corresponds to "
          "the number of channels and which dimension corresponds to the "
          "number of samples.\n\n    To avoid this, first call this method "
          "with an array where the number of samples does not match the "
          "number of channels.\n\n    The channel layout from the most "
          "recently "
          "provided input will be cached on the :py:class:`WritableAudioFile` "
          "object and will be used if necessary to disambiguate the array "
          "layout:\n\n"
          "    .. code-block:: python\n\n"
          "        with AudioFile(\"my_file.mp3\", \"w\", 44100, "
          "num_channels=2) as f:\n"
          "            # This will throw an exception:\n"
          "            f.write(np.zeros((2, 2)))  \n"
          "            # But this will work:\n"
          "            f.write(np.zeros((2, 1)))\n"
          "            # And now `f` expects an input shape of (num_channels, "
          "num_samples), so this works:\n"
          "            f.write(np.zeros((2, 2)))  \n"
          "\n"
          "        # Also an option: pass (0, num_channels) or (num_channels, "
          "0) first\n"
          "        # to hint that the input will be in that shape "
          "without writing anything:\n"
          "        with AudioFile(\"my_file.mp3\", \"w\", 44100, "
          "num_channels=2) as f:\n"
          "            # Pass a hint, but write nothing:\n"
          "            f.write(np.zeros((2, 0)))  \n"
          "            # And now `f` expects an input shape of (num_channels, "
          "num_samples), so this works:\n"
          "            f.write(np.zeros((2, 2)))  \n"
          "\n")
      .def("flush", &WriteableAudioFile::flush,
           "Attempt to flush this audio file's contents to disk. Not all "
           "formats support flushing, so this may throw a RuntimeError. (If "
           "this happens, closing the file will reliably force a flush to "
           "occur.)")
      .def("close", &WriteableAudioFile::close,
           "Close this file, flushing its contents to disk and rendering this "
           "object unusable for further writing.")
      .def("__enter__", &WriteableAudioFile::enter)
      .def("__exit__", &WriteableAudioFile::exit)
      .def("__repr__",
           [](const WriteableAudioFile &file) {
             std::ostringstream ss;
             ss << "<pedalboard.io.WriteableAudioFile";

             if (!file.getFilename().empty()) {
               ss << " filename=\"" << file.getFilename() << "\"";
             } else if (PythonOutputStream *stream =
                            file.getPythonOutputStream()) {
               ss << " file_like=" << stream->getRepresentation();
             }

             if (file.isClosed()) {
               ss << " closed";
             } else {
               ss << " samplerate=" << file.getSampleRateAsDouble();
               ss << " num_channels=" << file.getNumChannels();
               if (file.getQuality()) {
                 ss << " quality=\"" << *file.getQuality() << "\"";
               }
               ss << " file_dtype=" << file.getFileDatatype();
             }
             ss << " at " << &file;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "closed", &WriteableAudioFile::isClosed,
          "If this file has been closed, this property will be True.")
      .def_property_readonly(
          "samplerate", &WriteableAudioFile::getSampleRate,
          "The sample rate of this file in samples (per channel) per second "
          "(Hz). Sample rates are represented as floating-point numbers by "
          "default, but this property will be an integer if the file's sample "
          "rate has no fractional part.")
      .def_property_readonly("num_channels",
                             &WriteableAudioFile::getNumChannels,
                             "The number of channels in this file.")
      .def_property_readonly("frames", &WriteableAudioFile::getFramesWritten,
                             "The total number of frames (samples per "
                             "channel) written to this file so far.")
      .def("tell", &WriteableAudioFile::getFramesWritten,
           "Return the current position of the write pointer in this audio "
           "file, in frames at the target sample rate. This value will "
           "increase as :meth:`write` is called, and will never decrease.")
      .def_property_readonly(
          "file_dtype", &WriteableAudioFile::getFileDatatype,
          "The data type stored natively by this file. Note that write(...) "
          "will accept multiple datatypes, regardless of the value of this "
          "property.")
      .def_property_readonly(
          "quality", &WriteableAudioFile::getQuality,
          "The quality setting used to write this file. For many "
          "formats, this may be ``None``.\n\nQuality options differ based on "
          "the audio codec used in the file. Most codecs specify a number of "
          "bits per second in 16- or 32-bit-per-second increments (128 kbps, "
          "160 kbps, etc). Some codecs provide string-like options for "
          "variable bit-rate encoding (i.e. \"V0\" through \"V9\" for MP3). "
          "The strings ``\"best\"``, ``\"worst\"``, ``\"fastest\"``, and "
          "``\"slowest\"`` will also work for any codec.");

  m.def("get_supported_write_formats", []() {
    // JUCE doesn't support writing other formats out-of-the-box on all
    // platforms, and there's no easy way to tell which formats are
    // supported without attempting to create an AudioFileWriter object -
    // so this list is hardcoded for now.
    const std::vector<std::string> formats = {".aiff", ".flac", ".ogg", ".wav",
                                              ".mp3"};
    return formats;
  });
}
} // namespace Pedalboard