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

#include <pybind11/pybind11.h>

#include "../juce_overrides/juce_PatchedFLACAudioFormat.h"
#include "../juce_overrides/juce_PatchedMP3AudioFormat.h"
#include "../juce_overrides/juce_PatchedWavAudioFormat.h"
#include "LameMP3AudioFormat.h"

namespace py = pybind11;

namespace Pedalboard {

/**
 * Convert a Python path-like object (str, bytes, or os.PathLike) to a
 * std::string. This mimics os.fspath() behavior without requiring
 * std::filesystem::path (which requires macOS 10.15+).
 *
 * NOTE: This function calls Python APIs and requires the GIL to be held.
 */
inline std::string pathToString(py::object path) {
  // If it's already a string, just return it
  if (py::isinstance<py::str>(path)) {
    return path.cast<std::string>();
  }

  // Try calling os.fspath() to handle PathLike objects
  try {
    py::object os = py::module_::import("os");
    py::object fspath = os.attr("fspath");
    py::object result = fspath(path);
    return result.cast<std::string>();
  } catch (py::error_already_set &e) {
    throw py::type_error(
        "expected str, bytes, or os.PathLike object, not " +
        std::string(py::str(path.get_type().attr("__name__"))));
  }
}

static constexpr const unsigned int DEFAULT_AUDIO_BUFFER_SIZE_FRAMES = 8192;

/**
 * Registers audio formats for reading and writing in a deterministic (but
 * configurable) order.
 */
void registerPedalboardAudioFormats(juce::AudioFormatManager &manager,
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

class AudioFile {};

} // namespace Pedalboard
