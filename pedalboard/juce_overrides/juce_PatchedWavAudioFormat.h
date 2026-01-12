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

#include "../JuceHeader.h"
#include "juce_PatchedMP3AudioFormat.h"

namespace juce {

/**
 * A patched version of WavAudioFormat that adds support for WAV files
 * containing compressed audio data (WAVE_FORMAT_MPEGLAYER3, format tag
 * 0x55).
 *
 * These files are correct WAV files that use MP3 compression for the audio
 * data, wrapped in a standard RIFF/WAV container.
 */
class JUCE_API PatchedWavAudioFormat : public WavAudioFormat {
public:
  PatchedWavAudioFormat() : WavAudioFormat() {}
  ~PatchedWavAudioFormat() override {}

  AudioFormatReader *createReaderFor(InputStream *sourceStream,
                                     bool deleteStreamIfOpeningFails) override {
    auto streamStartPos = sourceStream->getPosition();

    // Helper to delegate to the parent WavAudioFormat implementation
    auto useDefaultReader = [&]() {
      sourceStream->setPosition(streamStartPos);
      return WavAudioFormat::createReaderFor(sourceStream,
                                             deleteStreamIfOpeningFails);
    };

    // Read the RIFF header
    auto firstChunkType = sourceStream->readInt();
    if (firstChunkType != chunkName("RIFF") &&
        firstChunkType != chunkName("RF64")) {
      return useDefaultReader();
    }

    // Skip the size field
    sourceStream->readInt();

    // Check for WAVE identifier
    if (sourceStream->readInt() != chunkName("WAVE")) {
      return useDefaultReader();
    }

    // Look for the fmt chunk to check the format tag
    while (!sourceStream->isExhausted()) {
      auto chunkType = sourceStream->readInt();
      auto length = (uint32)sourceStream->readInt();
      auto chunkEnd = sourceStream->getPosition() + length + (length & 1);

      if (chunkType == chunkName("fmt ")) {
        auto format = (unsigned short)sourceStream->readShort();

        switch (format) {
        // Formats we handle specially:
        case 0x55: // WAVE_FORMAT_MPEGLAYER3
          return createMP3ReaderForWav(sourceStream, chunkEnd, streamStartPos,
                                       deleteStreamIfOpeningFails);

        default:
          // Check for known-but-unsupported formats and throw helpful errors
          const char *unsupportedCodecName = getUnsupportedCodecName(format);
          if (unsupportedCodecName != nullptr) {
            if (deleteStreamIfOpeningFails)
              delete sourceStream;
            throw std::domain_error(
                "This WAV file uses the " + std::string(unsupportedCodecName) +
                " audio codec (format tag 0x" + toHexString(format) +
                "), which is not supported. "
                "Please convert the file to a standard PCM WAV, FLAC, or MP3 "
                "format before loading.");
          }

          // All other formats: delegate to JUCE's WavAudioFormat
          return useDefaultReader();
        }
      }

      sourceStream->setPosition(chunkEnd);
    }

    // Couldn't find fmt chunk - let JUCE handle it
    return useDefaultReader();
  }

private:
  /**
   * Creates an MP3 reader for a WAV file containing MP3-compressed audio data.
   * Finds the data chunk and wraps it in a SubregionStream for the MP3 decoder.
   */
  AudioFormatReader *createMP3ReaderForWav(InputStream *sourceStream,
                                           int64 fmtChunkEnd,
                                           int64 streamStartPos,
                                           bool deleteStreamIfOpeningFails) {
    sourceStream->setPosition(fmtChunkEnd);

    while (!sourceStream->isExhausted()) {
      auto dataChunkType = sourceStream->readInt();
      auto dataLength = (uint32)sourceStream->readInt();

      if (dataChunkType == chunkName("data")) {
        // Found the data chunk - the MP3 data starts here
        auto dataStart = sourceStream->getPosition();

        // Create a SubregionStream that only reads the MP3 data
        auto subStream = std::make_unique<SubregionStream>(
            sourceStream, dataStart, dataLength, deleteStreamIfOpeningFails);

        // Use the patched MP3 format to read the MP3 data
        PatchedMP3AudioFormat mp3Format;
        return mp3Format.createReaderFor(subStream.release(), true);
      }

      sourceStream->setPosition(sourceStream->getPosition() + dataLength +
                                (dataLength & 1));
    }

    // Couldn't find data chunk
    sourceStream->setPosition(streamStartPos);
    if (deleteStreamIfOpeningFails)
      delete sourceStream;
    return nullptr;
  }

  static constexpr int chunkName(const char *name) noexcept {
    return (int)ByteOrder::littleEndianInt(name);
  }

  static std::string toHexString(unsigned short value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", value);
    return std::string(buf);
  }

  /**
   * Returns a human-readable name for known-but-unsupported WAV codec formats.
   * Returns nullptr for unknown formats (which will get a generic error).
   */
  static const char *getUnsupportedCodecName(unsigned short format) {
    // Format tags from mmreg.h / RFC 2361: https://www.rfc-editor.org/rfc/rfc2361.html
    switch (format) {
    case 0x0002:
      return "Microsoft ADPCM";
    case 0x0006:
      return "A-law";
    case 0x0007:
      return "mu-law (u-law)";
    case 0x0010:
      return "OKI ADPCM";
    case 0x0011:
      return "IMA ADPCM (DVI ADPCM)";
    case 0x0012:
      return "MediaSpace ADPCM";
    case 0x0013:
      return "Sierra ADPCM";
    case 0x0014:
      return "G.723 ADPCM";
    case 0x0015:
      return "DIGISTD";
    case 0x0016:
      return "DIGIFIX";
    case 0x0017:
      return "Dialogic OKI ADPCM";
    case 0x0020:
      return "Yamaha ADPCM";
    case 0x0021:
      return "SONARC";
    case 0x0022:
      return "DSP Group TrueSpeech";
    case 0x0023:
      return "ECHOSC1";
    case 0x0024:
      return "Audiofile AF36";
    case 0x0025:
      return "APTX";
    case 0x0026:
      return "Audiofile AF10";
    case 0x0030:
      return "Dolby AC-2";
    case 0x0031:
      return "GSM 6.10";
    case 0x0040:
      return "G.721 ADPCM";
    case 0x0041:
      return "G.728 CELP";
    case 0x0050:
      return "MPEG";
    case 0x0052:
      return "RT24";
    case 0x0053:
      return "PAC";
    case 0x0061:
      return "G.726 ADPCM";
    case 0x0062:
      return "G.722 ADPCM";
    case 0x0064:
      return "G.722.1";
    case 0x0065:
      return "G.728";
    case 0x0066:
      return "G.726";
    case 0x0067:
      return "G.722";
    case 0x0069:
      return "G.729";
    case 0x0070:
      return "VSELP";
    case 0x0075:
      return "VOXWARE";
    case 0x00FF:
      return "AAC";
    case 0x0111:
      return "VIVO G.723";
    case 0x0112:
      return "VIVO Siren";
    case 0x0160:
      return "Windows Media Audio v1";
    case 0x0161:
      return "Windows Media Audio v2";
    case 0x0162:
      return "Windows Media Audio Pro";
    case 0x0163:
      return "Windows Media Audio Lossless";
    case 0x0200:
      return "Creative ADPCM";
    case 0x0202:
      return "Creative FastSpeech8";
    case 0x0203:
      return "Creative FastSpeech10";
    case 0x1000:
      return "Olivetti GSM";
    case 0x1001:
      return "Olivetti ADPCM";
    case 0x1002:
      return "Olivetti CELP";
    case 0x1003:
      return "Olivetti SBC";
    case 0x1004:
      return "Olivetti OPR";
    case 0x1100:
      return "LH Codec";
    case 0x1400:
      return "Norris";
    case 0x1500:
      return "SoundSpace Musicompress";
    case 0x2000:
      return "Dolby AC-3 (SPDIF)";
    case 0x2001:
      return "DTS";

    default:
      // Unknown format - don't provide a specific error
      // Let it fall through to normal processing which may fail with generic
      // error
      return nullptr;
    }
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchedWavAudioFormat)
};

} // namespace juce

