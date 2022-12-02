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

#include "../juce_overrides/juce_PatchedFLACAudioFormat.h"
#include "../juce_overrides/juce_PatchedMP3AudioFormat.h"
#include "AudioFile.h"
#include "LameMP3AudioFormat.h"

namespace Pedalboard {

static constexpr const unsigned int DEFAULT_AUDIO_BUFFER_SIZE_FRAMES = 8192;

/**
 * Registers audio formats for reading and writing in a deterministic (but
 * configurable) order. On different platforms, different formats are handled by
 * different backends (i.e.: CoreAudioFormat handles MP3 on macOS, but only for
 * reading) so this method allows us to ensure reproducibility in tests.
 */
void registerPedalboardAudioFormats(juce::AudioFormatManager &manager,
                                    bool forWriting, bool crossPlatformOnly) {
  manager.registerFormat(new juce::WavAudioFormat(), true);
  manager.registerFormat(new juce::AiffAudioFormat(), false);
  manager.registerFormat(new juce::PatchedFlacAudioFormat(), false);

#if JUCE_USE_OGGVORBIS
  manager.registerFormat(new juce::OggVorbisAudioFormat(), false);
#endif

  if (forWriting) {
    // Prefer our own custom MP3 format (which only writes, doesn't read) over
    // MP3AudioFormat (which only reads, doesn't write)
    manager.registerFormat(new LameMP3AudioFormat(), false);
  } else {
    // On macOS, CoreAudio can read MP3s better (more fault-tolerantly) than
    // MP3AudioFormat can. But sometimes, we still want to use the built-in MP3
    // reader so that we can get identical parsing behaviour on both macOS and
    // Linux. To do so, we use this flag:

    if (crossPlatformOnly) {
      manager.registerFormat(new juce::MP3AudioFormat(), false);
    } else {
#if JUCE_MAC || JUCE_IOS
      manager.registerFormat(new juce::CoreAudioFormat(), false);
#else
      manager.registerFormat(new juce::MP3AudioFormat(), false);
#endif
    }
  }

#if JUCE_USE_WINDOWS_MEDIA_FORMAT
  manager.registerFormat(new juce::WindowsMediaAudioFormat(), false);
#endif
}

class AudioFile {};

} // namespace Pedalboard