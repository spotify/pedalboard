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
#include "../juce_overrides/juce_PatchedMP3AudioFormat.h"
#include "AudioFile.h"
#include "PythonInputStream.h"

namespace py = pybind11;

namespace Pedalboard {

inline long long parseNumSamples(std::variant<double, long long> numSamples) {
  // Unfortunately, std::visit cannot be used here due to macOS version
  // issues: https://stackoverflow.com/q/52310835/679081
  if (auto *i = std::get_if<long long>(&numSamples)) {
    return *i;
  } else if (auto *i = std::get_if<double>(&numSamples)) {
    double integerPart;
    double fractionalPart = std::modf(*i, &integerPart);
    if (fractionalPart != 0) {
      throw std::domain_error(
          "ReadableAudioFile cannot read a fractional "
          "number of samples; was asked to read " +
          std::to_string(*i) +
          " samples. Please provide a whole (integer) number of "
          "samples to read instead.");
    }
    return (long long)integerPart;
  } else {
    throw std::domain_error(
        "ReadableAudioFile::read received an input that was not a number!");
  }
}

class ReadableAudioFile
    : public AbstractReadableAudioFile,
      public std::enable_shared_from_this<ReadableAudioFile> {
public:
  ReadableAudioFile(std::string filename) : filename(filename) {
    registerPedalboardAudioFormats(formatManager, false);
    // This is kind of silly, as nobody else has a reference
    // to this object yet; but it prevents some juce assertions in debug builds:
    juce::ScopedWriteLock writeLock(objectLock);

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
    }

    if (!reader)
      throw std::domain_error("Failed to open audio file: file \"" + filename +
                              "\" does not seem to contain audio data in a "
                              "known or supported format.");

    cacheMetadata();
  }

  ReadableAudioFile(std::unique_ptr<PythonInputStream> inputStream) {
    registerPedalboardAudioFormats(formatManager, false);

    // This is kind of silly, as nobody else has a reference
    // to this object yet; but it prevents some juce assertions in debug builds:
    juce::ScopedWriteLock writeLock(objectLock);

    inputStream->setObjectLock(&objectLock);

    if (!inputStream->isSeekable()) {
      PythonException::raise();
      throw std::domain_error("Failed to open audio file-like object: input "
                              "stream " +
                              inputStream->getRepresentation() +
                              " must be seekable.");
    }

    auto originalStreamPosition = inputStream->getPosition();

    if (!reader) {
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
              "Input file-like object " + inputStream->getRepresentation() +
              " did not seek to the expected position. "
              "The provided file-like object must be fully seekable to allow "
              "reading audio files.");
        }
      }
    }

    PythonException::raise();

    if (!reader) {
      std::ostringstream ss;
      try {
        ss.imbue(std::locale(""));
      } catch (const std::runtime_error &e) {
        // On some systems (like Alpine Linux) we only have "C" and "POSIX"
        // locales:
        ss.imbue(std::locale("C"));
      }

      ss << "Failed to open audio file-like object: ";
      ss << inputStream->getRepresentation();

      if (originalStreamPosition != 0) {
        if (originalStreamPosition < inputStream->getTotalLength()) {
          ss << " has its stream position set to " << originalStreamPosition;
          ss << "bytes. Reading from this position did not produce "
                "audio data in a known or supported format.";
        } else {
          ss << " has its stream position set to the end of the stream ("
             << originalStreamPosition;
          ss << " bytes).";
        }
        ss << " Try seeking this file-like object back to its start before "
              "passing it to AudioFile";
      } else if (inputStream->getTotalLength() == 0) {
        ss << " is empty";
      } else {
        ss << " does not seem to contain audio data in a known or supported "
              "format";
      }
      ss << ".";

      throw std::domain_error(ss.str());
    }

    PythonException::raise();
    cacheMetadata();
  }

  void cacheMetadata() {
    sampleRate = reader->sampleRate;
    numChannels = reader->numChannels;
    numFrames = reader->lengthInSamples;

    if (reader->usesFloatingPointData) {
      switch (reader->bitsPerSample) {
      case 16: // OGG returns 16-bit int data, but internally stores floats
      case 32:
        fileDatatype = "float32";
        break;
      case 64:
        fileDatatype = "float64";
        break;
      default:
        fileDatatype = "unknown";
        break;
      }
    } else {
      switch (reader->bitsPerSample) {
      case 8:
        fileDatatype = "int8";
        break;
      case 16:
        fileDatatype = "int16";
        break;
      case 24:
        fileDatatype = "int24";
        break;
      case 32:
        fileDatatype = "int32";
        break;
      case 64:
        fileDatatype = "int64";
        break;
      default:
        fileDatatype = "unknown";
        break;
      }
    }
  }

  std::variant<double, long> getSampleRate() const override {
    double integerPart;
    double fractionalPart = std::modf(sampleRate, &integerPart);

    if (fractionalPart > 0) {
      return sampleRate;
    } else {
      return (long)(sampleRate);
    }
  }

  double getSampleRateAsDouble() const override { return sampleRate; }

  long long getLengthInSamples() const override {
    const juce::ScopedReadLock scopedLock(objectLock);
    return numFrames + (lengthCorrection ? *lengthCorrection : 0);
  }

  double getDuration() const override { return numFrames / getSampleRateAsDouble(); }

  long getNumChannels() const override { return numChannels; }

  std::string getFileFormat() const override {
    const juce::ScopedReadLock scopedLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    return reader->getFormatName().toStdString();
  }

  std::string getFileDatatype() const override { return fileDatatype; }

  py::array_t<float> read(std::variant<double, long long> numSamplesVariant) override {
    long long numSamples = parseNumSamples(numSamplesVariant);

    if (numSamples == 0)
      throw std::domain_error(
          "ReadableAudioFile will not read an entire file at once, due to the "
          "possibility that a file may be larger than available memory. Please "
          "pass a number of frames to read (available from the 'frames' "
          "attribute).");

    std::optional<juce::ScopedReadLock> scopedLock =
        std::optional<juce::ScopedReadLock>(objectLock);

    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    // Allocate a buffer to return of up to numSamples:
    long long numChannels = reader->numChannels;
    numSamples =
        std::min(numSamples, (reader->lengthInSamples +
                              (lengthCorrection ? *lengthCorrection : 0)) -
                                 currentPosition);

    py::array_t<float> buffer =
        py::array_t<float>({(long long)numChannels, (long long)numSamples});

    long long numSamplesToKeep = numSamples;

    // Get the data pointer directly without keeping a buffer_info reference
    // that would prevent resizing later (needed for Python 3.14+ / NumPy 2.x)
    float *outputPtr = buffer.mutable_data();

    {
      py::gil_scoped_release release;
      numSamplesToKeep = readInternal(numChannels, numSamples, outputPtr);

      // After this point, we no longer need to hold the read lock as we don't
      // interact with the reader object anymore. Releasing this early (before
      // re-acquiring the GIL) helps avoid deadlocks:
      scopedLock.reset();
    }

    PythonException::raise();
    if (numSamplesToKeep < numSamples) {
      buffer.resize({(long long)numChannels, (long long)numSamplesToKeep});
    }

    return buffer;
  }

  /**
   * Read the given number of frames (samples in each channel) from this audio
   * file into the given output pointers. This method does not take or hold the
   * GIL, as no Python methods (like the py::array_t constructor) are
   * called directly (except for in the error case).
   *
   * @param numChannels The number of channels to read from the file
   * @param numSamplesToFill The number of frames to read from the file
   * @param outputPointer A pointer to the contiguous floating-point output
   *                      array to write to of shape [numChannels,
   * numSamplesToFill].
   *
   * @return the number of samples that were actually read from the file
   */
  long long readInternal(const long long numChannels,
                         const long long numSamplesToFill,
                         float *outputPointer) {
    // Note: We take a "write" lock here as calling readInternal will
    // advance internal state:
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile. Note "
          "that using multiple concurrent readers on the same AudioFile "
          "object will produce nondeterministic results.");
    }

    // If the file being read does not have enough content, it _should_ pad
    // the rest of the array with zeroes. Unfortunately, this does not seem to
    // be true in practice, so we pre-zero the array to be returned here:
    std::fill_n(outputPointer, numChannels * numSamplesToFill, 0);

    long long numSamples = std::min(
        numSamplesToFill,
        (reader->lengthInSamples + (lengthCorrection ? *lengthCorrection : 0)) -
            currentPosition);

    long long numSamplesToKeep = numSamples;

    float **channelPointers = (float **)alloca(numChannels * sizeof(float *));
    for (long long c = 0; c < numChannels; c++) {
      channelPointers[c] = ((float *)outputPointer) + (numSamples * c);
    }

    if (reader->usesFloatingPointData || reader->bitsPerSample == 32) {
      auto readResult = reader->read(channelPointers, numChannels,
                                     currentPosition, numSamples);

      juce::int64 samplesRead = numSamples;
      if (juce::AudioFormatReaderWithPosition *positionAware =
              dynamic_cast<juce::AudioFormatReaderWithPosition *>(
                  reader.get())) {
        samplesRead = positionAware->getCurrentPosition() - currentPosition;
      }

      bool hitEndOfFile =
          (samplesRead + currentPosition) == reader->lengthInSamples;

      // We read some data, but not as much as we asked for!
      // This will only happen for lossy, header-optional formats
      // like MP3.
      if (samplesRead < numSamples || hitEndOfFile) {
        lengthCorrection =
            (samplesRead + currentPosition) - reader->lengthInSamples;
      } else if (!readResult) {
        PythonException::raise();
        throwReadError(currentPosition, numSamples, samplesRead);
      }
      numSamplesToKeep = samplesRead;
    } else {
      // If the audio is stored in an integral format, read it as integers
      // and do the floating-point conversion ourselves to work around
      // floating-point imprecision in JUCE when reading formats smaller than
      // 32-bit (i.e.: 16-bit audio is off by about 0.003%)
      auto readResult =
          reader->readSamples((int **)channelPointers, numChannels, 0,
                              currentPosition, numSamplesToKeep);
      if (!readResult) {
        PythonException::raise();
        throwReadError(currentPosition, numSamples);
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

      for (long long c = 0; c < numChannels; c++) {
        juce::FloatVectorOperations::convertFixedToFloat(
            channelPointers[c], (const int *)channelPointers[c], scaleFactor,
            static_cast<int>(numSamples));
      }
    }

    currentPosition += numSamplesToKeep;
    return numSamplesToKeep;
  }

  py::array readRaw(std::variant<double, long long> numSamplesVariant) {
    long long numSamples = parseNumSamples(numSamplesVariant);
    if (numSamples == 0)
      throw std::domain_error(
          "ReadableAudioFile will not read an entire file at once, due to the "
          "possibility that a file may be larger than available memory. Please "
          "pass a number of frames to read (available from the 'frames' "
          "attribute).");

    const juce::ScopedReadLock readLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    if (reader->usesFloatingPointData) {
      if (reader->bitsPerSample > 32) {
        throw std::runtime_error(
            "This file contains " + std::to_string(reader->bitsPerSample) +
            "-bit floating-point audio, which cannot be returned without "
            "losing precision. Use read() instead to get 32-bit float data.");
      }
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
    const juce::ScopedReadLock readLock(objectLock);
    if (reader->usesFloatingPointData) {
      throw std::runtime_error(
          "Can't call readInteger with a floating point file!");
    }

    // Allocate a buffer to return of up to numSamples:
    long long numChannels = reader->numChannels;
    numSamples =
        std::min(numSamples, (reader->lengthInSamples +
                              (lengthCorrection ? *lengthCorrection : 0)) -
                                 currentPosition);
    py::array_t<SampleType> buffer = py::array_t<SampleType>(
        {(long long)numChannels, (long long)numSamples});

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
        for (long long c = 0; c < numChannels; c++) {
          channelPointers[c] = ((int *)outputInfo.ptr) + (numSamples * c);
        }

        bool readResult = false;
        {
          ScopedTryWriteLock scopedTryWriteLock(objectLock);
          if (!scopedTryWriteLock.isLocked()) {
            throw std::runtime_error(
                "Another thread is currently reading from this AudioFile. Note "
                "that using multiple concurrent readers on the same AudioFile "
                "object will produce nondeterministic results.");
          }
          readResult = reader->readSamples(channelPointers, numChannels, 0,
                                           currentPosition, numSamples);
        }

        if (!readResult) {
          PythonException::raise();
          throwReadError(currentPosition, numSamples);
        }
      } else {
        // Read the file in smaller chunks, converting from int32 to the
        // appropriate output format as we go:
        std::vector<std::vector<int>> intBuffers;
        intBuffers.resize(numChannels);

        int **channelPointers = (int **)alloca(numChannels * sizeof(int *));
        for (long long startSample = 0; startSample < numSamples;
             startSample += DEFAULT_AUDIO_BUFFER_SIZE_FRAMES) {
          long long samplesToRead =
              std::min(numSamples - startSample,
                       (long long)DEFAULT_AUDIO_BUFFER_SIZE_FRAMES);

          for (long long c = 0; c < numChannels; c++) {
            intBuffers[c].resize(samplesToRead);
            channelPointers[c] = intBuffers[c].data();
          }

          bool readResult = false;
          {
            ScopedTryWriteLock scopedTryWriteLock(objectLock);
            if (!scopedTryWriteLock.isLocked()) {
              throw std::runtime_error(
                  "Another thread is currently reading from this AudioFile. "
                  "Note that using multiple concurrent readers on the same "
                  "AudioFile object will produce nondeterministic results.");
            }

            readResult = reader->readSamples(channelPointers, numChannels, 0,
                                             currentPosition + startSample,
                                             samplesToRead);
          }

          if (!readResult) {
            PythonException::raise();
            throw std::runtime_error("Failed to read from file.");
          }

          // Convert the data in intBuffers to the output format:
          char shift = 32 - reader->bitsPerSample;
          for (long long c = 0; c < numChannels; c++) {
            SampleType *outputChannelPointer =
                (((SampleType *)outputInfo.ptr) + (c * numSamples));
            for (long long i = 0; i < samplesToRead; i++) {
              outputChannelPointer[startSample + i] = intBuffers[c][i] >> shift;
            }
          }
        }
      }
    }

    PythonException::raise();

    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile. "
          "Note that using multiple concurrent readers on the same "
          "AudioFile object will produce nondeterministic results.");
    }
    currentPosition += numSamples;
    return buffer;
  }

  void seek(long long targetPosition) override {
    py::gil_scoped_release release;
    seekInternal(targetPosition);
  }

  void seekInternal(long long targetPosition) override {
    const juce::ScopedReadLock scopedReadLock(objectLock);
    if (!reader)
      throw std::runtime_error("I/O operation on a closed file.");

    long long endOfFile =
        (reader->lengthInSamples + (lengthCorrection ? *lengthCorrection : 0));

    if (targetPosition > endOfFile)
      throw std::domain_error(
          "Cannot seek to position " + std::to_string(targetPosition) +
          " frames, which is beyond end of file (" + std::to_string(endOfFile) +
          " frames) by " + std::to_string(endOfFile - targetPosition) +
          " frames.");

    if (targetPosition < 0)
      throw std::domain_error("Cannot seek before start of file (to position " +
                              std::to_string(targetPosition) + ").");

    // Promote to a write lock as we're now modifying the object:
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile. Note that "
          "using multiple concurrent readers on the same AudioFile object will "
          "produce nondeterministic results.");
    }
    currentPosition = targetPosition;
  }

  long long tell() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedLock(objectLock);
    return currentPosition;
  }

  void close() override {
    ScopedTryWriteLock scopedTryWriteLock(objectLock);
    if (!scopedTryWriteLock.isLocked()) {
      throw std::runtime_error(
          "Another thread is currently reading from this AudioFile; it cannot "
          "be closed until the other thread completes its operation.");
    }
    // Note: This may deallocate a Python object, so must be called with the
    // GIL held:
    reader.reset();
  }

  bool isClosed() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedLock(objectLock);
    return !reader;
  }

  bool isSeekable() const override {
    py::gil_scoped_release release;
    const juce::ScopedReadLock scopedLock(objectLock);

    // At the moment, ReadableAudioFile instances are always seekable, as
    // they're backed by files.
    return reader != nullptr;
  }

  bool exactDurationKnown() const override {
    const juce::ScopedReadLock scopedLock(objectLock);

    if (juce::AudioFormatReaderWithPosition *approximateLengthReader =
            dynamic_cast<juce::AudioFormatReaderWithPosition *>(reader.get())) {
      if (approximateLengthReader->lengthIsApproximate()) {
        // Note: lengthCorrection is an std::optional,
        // so this is checking if it's set; not if it's zero:
        if (!lengthCorrection) {
          // The reader returns an approximate length, and we haven't
          // hit the end of the file yet:
          return false;
        }
      }
    }

    return true;
  }

  std::optional<std::string> getFilename() const override { return filename; }

  PythonInputStream *getPythonInputStream() const override {
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

  std::string getClassName() const override { return "ReadableAudioFile"; }

private:
  void throwReadError(long long currentPosition, long long numSamples,
                      long long samplesRead = -1) {
    std::ostringstream ss;
    try {
      ss.imbue(std::locale(""));
    } catch (const std::runtime_error &e) {
      // On some systems (like Alpine Linux) we only have "C" and "POSIX"
      // locales:
      ss.imbue(std::locale("C"));
    }

    ss << "Failed to read audio data";

    if (getFilename() && !getFilename()->empty()) {
      ss << " from file \"" << *getFilename() << "\"";
    } else if (PythonInputStream *stream = getPythonInputStream()) {
      ss << " from " << stream->getRepresentation();
    }

    ss << "."
       << " Tried to read " << numSamples
       << " frames of audio from frame offset " << currentPosition;

    if (samplesRead != -1) {
      ss << " but only decoded " << samplesRead << " frames";
    }

    if (PythonInputStream *stream = getPythonInputStream()) {
      ss << " and encountered invalid data near byte " << stream->getPosition();
    }
    ss << ".";

    if (PythonInputStream *stream = getPythonInputStream()) {
      if (stream->isExhausted()) {
        ss << " The file may contain invalid data near its end. Try "
              "reading fewer audio frames from the file.";
      }
    }

    // In case any of the calls above to PythonInputStream cause an exception in
    // Python, this line will re-raise those so that the Python exception is
    // visible:
    PythonException::raise();

    throw std::runtime_error(ss.str());
  }

  juce::AudioFormatManager formatManager;
  std::string filename;
  std::unique_ptr<juce::AudioFormatReader> reader;
  juce::ReadWriteLock objectLock;

  double sampleRate;
  long numChannels;
  long numFrames;
  std::string fileDatatype;

  long long currentPosition = 0;

  // Certain files (notably CBR MP3 files) can report the wrong number of
  // frames until the entire file is scanned. This field stores the delta
  // between the actual number of frames and the reported number of frames.
  // If more frames are present in the file than expected, `lengthCorrection`
  // will be greater than 0; if fewer are present, `lengthCorrection` will
  // be less than 0.
  std::optional<long long> lengthCorrection = {};
};

inline py::class_<ReadableAudioFile, AbstractReadableAudioFile,
                  std::shared_ptr<ReadableAudioFile>>
declare_readable_audio_file(py::module &m) {
  return py::class_<ReadableAudioFile, AbstractReadableAudioFile,
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
class ChannelConvertedReadableAudioFile;

inline void init_readable_audio_file(
    py::module &m,
    py::class_<ReadableAudioFile, AbstractReadableAudioFile, std::shared_ptr<ReadableAudioFile>>
        &pyReadableAudioFile) {
  // Note: Most methods are inherited from AbstractReadableAudioFile.
  // We only define class-specific methods here.
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
            if (!isReadableFileLike(filelike) &&
                !tryConvertingToBuffer(filelike)) {
              throw py::type_error(
                  "Expected either a filename, a file-like object (with "
                  "read, seek, seekable, and tell methods) or a memoryview, "
                  "but received: " +
                  py::repr(filelike).cast<std::string>());
            }

            if (std::optional<py::buffer> buf =
                    tryConvertingToBuffer(filelike)) {
              return std::make_shared<ReadableAudioFile>(
                  std::make_unique<PythonMemoryViewInputStream>(*buf,
                                                                filelike));
            } else {
              return std::make_shared<ReadableAudioFile>(
                  std::make_unique<PythonInputStream>(filelike));
            }
          },
          py::arg("cls"), py::arg("file_like"))
      .def("read_raw", &ReadableAudioFile::readRaw, py::arg("num_frames") = 0,
           R"(
Read the given number of frames (samples in each channel) from this audio file at its current position.

``num_frames`` is a required argument, as audio files can be deceptively large. (Consider that 
an hour-long ``.ogg`` file may be only a handful of megabytes on disk, but may decompress to
nearly a gigabyte in memory.) Audio files should be read in chunks, rather than all at once, to avoid 
hard-to-debug memory problems and out-of-memory crashes.

Audio samples are returned as a multi-dimensional :class:`numpy.array` with the shape
``(channels, samples)``; i.e.: a stereo audio file will have shape ``(2, <length>)``.
Returned data is in the raw format stored by the underlying file (one of ``int8``, ``int16``,
``int32``, or ``float32``) and may have any magnitude.

If the file does not contain enough audio data to fill ``num_frames``, the returned
:class:`numpy.array` will contain as many frames as could be read from the file. (In some cases,
passing :py:attr:`frames` as ``num_frames`` may still return less data than expected. See documentation
for :py:attr:`frames` and :py:attr:`exact_duration_known` for more information about situations
in which this may occur.)

.. note::
    For convenience, the ``num_frames`` argument may be a floating-point number. However, if the
    provided number of frames contains a fractional part (i.e.: ``1.01`` instead of ``1.00``) then
    an exception will be thrown, as a fractional number of samples cannot be returned.
)");

  m.def("get_supported_read_formats", []() {
    juce::AudioFormatManager manager;
    registerPedalboardAudioFormats(manager, false);

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
