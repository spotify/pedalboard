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

#include <optional>
#include <variant>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "../juce_overrides/juce_PatchedFLACAudioFormat.h"
#include "../juce_overrides/juce_PatchedMP3AudioFormat.h"
#include "../juce_overrides/juce_PatchedWavAudioFormat.h"
#include "LameMP3AudioFormat.h"

namespace py = pybind11;

namespace Pedalboard {

// Forward declaration
class PythonInputStream;

static constexpr const unsigned int DEFAULT_AUDIO_BUFFER_SIZE_FRAMES = 8192;

/**
 * Registers audio formats for reading and writing in a deterministic (but
 * configurable) order.
 */
inline void registerPedalboardAudioFormats(juce::AudioFormatManager &manager,
                                           bool forWriting) {
  manager.registerFormat(new juce::PatchedWavAudioFormat(), true);
  manager.registerFormat(new juce::AiffAudioFormat(), false);
  manager.registerFormat(new juce::PatchedFlacAudioFormat(), false);

#if JUCE_USE_OGGVORBIS
  manager.registerFormat(new juce::OggVorbisAudioFormat(), false);
#endif

  if (forWriting) {
    // Prefer our own custom MP3 format (which only writes, doesn't read) over
    // PatchedMP3AudioFormat (which only reads, doesn't write)
    manager.registerFormat(new LameMP3AudioFormat(), false);
  } else {
    manager.registerFormat(new juce::PatchedMP3AudioFormat(), false);
#if JUCE_MAC || JUCE_IOS
    manager.registerFormat(new juce::CoreAudioFormat(), false);
#endif
  }

#if JUCE_USE_WINDOWS_MEDIA_FORMAT
  manager.registerFormat(new juce::WindowsMediaAudioFormat(), false);
#endif
}

/**
 * Base marker class for all audio file types.
 */
class AudioFile {};

/**
 * Abstract interface for readable audio files.
 *
 * This interface defines the common API shared by ReadableAudioFile,
 * ResampledReadableAudioFile, and ChannelConvertedReadableAudioFile,
 * allowing them to be used interchangeably.
 */
class AbstractReadableAudioFile : public AudioFile {
public:
  virtual ~AbstractReadableAudioFile() = default;

  // Sample rate and duration
  virtual std::variant<double, long> getSampleRate() const = 0;
  virtual double getSampleRateAsDouble() const = 0;
  virtual long long getLengthInSamples() const = 0;
  virtual double getDuration() const = 0;

  // Channel info
  virtual long getNumChannels() const = 0;

  // File metadata
  virtual bool exactDurationKnown() const = 0;
  virtual std::string getFileFormat() const = 0;
  virtual std::string getFileDatatype() const = 0;

  // Reading
  virtual py::array_t<float> read(std::variant<double, long long> numSamples) = 0;

  // Seeking
  virtual void seek(long long position) = 0;
  virtual void seekInternal(long long position) = 0;
  virtual long long tell() const = 0;

  // State
  virtual void close() = 0;
  virtual bool isClosed() const = 0;
  virtual bool isSeekable() const = 0;

  // File info
  virtual std::optional<std::string> getFilename() const = 0;
  virtual PythonInputStream *getPythonInputStream() const = 0;

  // Context manager support
  virtual std::shared_ptr<AbstractReadableAudioFile> enter() = 0;
  virtual void exit(const py::object &type, const py::object &value,
                    const py::object &traceback) = 0;

  // For __repr__
  virtual std::string getClassName() const = 0;
};

} // namespace Pedalboard
