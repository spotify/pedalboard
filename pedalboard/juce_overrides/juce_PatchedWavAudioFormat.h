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

// dr_wav for ADPCM decoding (public domain / MIT-0)
#include "dr_wav.h"
#include "dr_wav_config.h"

namespace juce {

/**
 * WAV format tags from mmreg.h / RFC 2361.
 * https://www.rfc-editor.org/rfc/rfc2361.html
 */
enum class WavFormatTag : unsigned short {
  PCM = 0x0001,
  ADPCM = 0x0002,
  IEEEFloat = 0x0003,
  ALaw = 0x0006,
  MuLaw = 0x0007,
  IMAADPCM = 0x0011,
  GSM610 = 0x0031,
  MPEG = 0x0050,
  MPEGLayer3 = 0x0055,
  Extensible = 0xFFFE,
};

/**
 * An AudioFormatReader that uses dr_wav to decode audio formats not natively
 * supported by JUCE: ADPCM (MS and IMA), A-law, µ-law, and 64-bit float.
 * This streams the audio data rather than loading it all into memory.
 */
class DrWavAudioFormatReader : public AudioFormatReader {
public:
  DrWavAudioFormatReader(InputStream *stream)
      : AudioFormatReader(stream, "dr_wav"), inputStream(stream) {

    // Initialize dr_wav with our I/O callbacks
    if (!drwav_init(&wav, drwavReadCallback, drwavSeekCallback,
                    drwavTellCallback, this, nullptr)) {
      // Failed to initialize - set invalid state
      sampleRate = 0;
      numChannels = 0;
      lengthInSamples = 0;
      return;
    }

    wavInitialized = true;

    // Set up AudioFormatReader properties
    sampleRate = static_cast<double>(wav.sampleRate);
    numChannels = static_cast<unsigned int>(wav.channels);
    lengthInSamples = static_cast<int64>(wav.totalPCMFrameCount);

    // For IEEE float formats, report the original bits per sample (32 or 64)
    // For other formats (ADPCM, A-law, µ-law), we decode to float32
    if (wav.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
      bitsPerSample = static_cast<unsigned int>(wav.fmt.bitsPerSample);
      usesFloatingPointData = true;
    } else {
      // ADPCM, A-law, µ-law are decoded to float32
      bitsPerSample = 32;
      usesFloatingPointData = true;
    }
  }

  ~DrWavAudioFormatReader() override {
    if (wavInitialized) {
      drwav_uninit(&wav);
    }
    // Note: inputStream is owned by the base AudioFormatReader class
    // and will be deleted in its destructor
  }

  bool readSamples(int **destChannels, int numDestChannels,
                   int startOffsetInDestBuffer, int64 startSampleInFile,
                   int numSamples) override {
    if (!wavInitialized || numSamples <= 0) {
      return false;
    }

    // Seek to the requested position if needed
    if (startSampleInFile != currentPosition) {
      if (!drwav_seek_to_pcm_frame(
              &wav, static_cast<drwav_uint64>(startSampleInFile))) {
        // Seek failed - fill with zeros
        clearSamplesBeyondFile(destChannels, numDestChannels,
                               startOffsetInDestBuffer, numSamples, 0);
        return true;
      }
      currentPosition = startSampleInFile;
    }

    // Handle reading before start of file
    if (startSampleInFile < 0) {
      auto samplesToZero =
          static_cast<int>(std::min(-startSampleInFile, (int64)numSamples));
      clearSamplesBeyondFile(destChannels, numDestChannels,
                             startOffsetInDestBuffer, samplesToZero, 0);
      startOffsetInDestBuffer += samplesToZero;
      numSamples -= samplesToZero;
      if (numSamples <= 0)
        return true;
      startSampleInFile = 0;
      currentPosition = 0;
      drwav_seek_to_pcm_frame(&wav, 0);
    }

    // Allocate interleaved buffer for dr_wav output
    auto totalChannels = static_cast<int>(wav.channels);
    std::vector<float> interleavedBuffer(
        static_cast<size_t>(numSamples * totalChannels));

    // Read decoded samples
    auto framesRead = drwav_read_pcm_frames_f32(
        &wav, static_cast<drwav_uint64>(numSamples), interleavedBuffer.data());

    currentPosition += static_cast<int64>(framesRead);

    // De-interleave into destination channels
    auto samplesRead = static_cast<int>(framesRead);
    for (int ch = 0; ch < numDestChannels; ++ch) {
      if (destChannels[ch] != nullptr) {
        auto *dest = reinterpret_cast<float *>(destChannels[ch]) +
                     startOffsetInDestBuffer;

        if (ch < totalChannels) {
          // Copy from interleaved source
          for (int i = 0; i < samplesRead; ++i) {
            dest[i] =
                interleavedBuffer[static_cast<size_t>(i * totalChannels + ch)];
          }
        } else {
          // Channel doesn't exist in source - zero fill
          std::fill(dest, dest + samplesRead, 0.0f);
        }

        // Zero any samples beyond what we read
        if (samplesRead < numSamples) {
          std::fill(dest + samplesRead, dest + numSamples, 0.0f);
        }
      }
    }

    return true;
  }

private:
  drwav wav{};
  bool wavInitialized = false;
  InputStream *inputStream; // Borrowed reference (owned by base class)
  int64 currentPosition = 0;

  void clearSamplesBeyondFile(int **destChannels, int numDestChannels,
                              int startOffset, int numSamples,
                              int samplesRead) {
    for (int ch = 0; ch < numDestChannels; ++ch) {
      if (destChannels[ch] != nullptr) {
        auto *dest = reinterpret_cast<float *>(destChannels[ch]) + startOffset +
                     samplesRead;
        std::fill(dest, dest + (numSamples - samplesRead), 0.0f);
      }
    }
  }

  // dr_wav I/O callbacks - bridge to JUCE InputStream
  static size_t drwavReadCallback(void *pUserData, void *pBufferOut,
                                  size_t bytesToRead) {
    auto *reader = static_cast<DrWavAudioFormatReader *>(pUserData);
    return static_cast<size_t>(
        reader->inputStream->read(pBufferOut, static_cast<int>(bytesToRead)));
  }

  static drwav_bool32 drwavSeekCallback(void *pUserData, int offset,
                                        drwav_seek_origin origin) {
    auto *reader = static_cast<DrWavAudioFormatReader *>(pUserData);
    int64 newPos;
    if (origin == DRWAV_SEEK_SET) {
      newPos = offset;
    } else if (origin == DRWAV_SEEK_CUR) {
      newPos = reader->inputStream->getPosition() + offset;
    } else { // DRWAV_SEEK_END
      newPos = reader->inputStream->getTotalLength() + offset;
    }
    return reader->inputStream->setPosition(newPos) ? DRWAV_TRUE : DRWAV_FALSE;
  }

  static drwav_bool32 drwavTellCallback(void *pUserData, drwav_int64 *pCursor) {
    auto *reader = static_cast<DrWavAudioFormatReader *>(pUserData);
    *pCursor = static_cast<drwav_int64>(reader->inputStream->getPosition());
    return DRWAV_TRUE;
  }
};

/**
 * A patched version of WavAudioFormat that adds support for WAV files
 * containing compressed audio data:
 * - WAVE_FORMAT_MPEGLAYER3 (MP3 in WAV container)
 * - WAVE_FORMAT_ADPCM (Microsoft ADPCM)
 * - WAVE_FORMAT_DVI_ADPCM (IMA ADPCM)
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

        switch (static_cast<WavFormatTag>(format)) {
        case WavFormatTag::MPEGLayer3:
          return createMP3ReaderForWav(sourceStream, chunkEnd, streamStartPos,
                                       deleteStreamIfOpeningFails);

        case WavFormatTag::ADPCM:
        case WavFormatTag::IMAADPCM:
        case WavFormatTag::ALaw:
        case WavFormatTag::MuLaw:
          return createDrWavReaderForWav(sourceStream, streamStartPos,
                                         deleteStreamIfOpeningFails);

        case WavFormatTag::IEEEFloat: {
          // JUCE doesn't support 64-bit float WAV, but dr_wav does.
          // Read bitsPerSample from fmt chunk to check.
          // fmt chunk layout after formatTag: channels(2), sampleRate(4),
          // byteRate(4), blockAlign(2), bitsPerSample(2)
          sourceStream->skipNextBytes(12);
          auto bitsPerSample = (unsigned short)sourceStream->readShort();
          if (bitsPerSample == 64) {
            return createDrWavReaderForWav(sourceStream, streamStartPos,
                                           deleteStreamIfOpeningFails);
          }
          // 32-bit float is handled fine by JUCE
          return useDefaultReader();
        }

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

  /**
   * Creates a dr_wav-based reader for WAV files containing compressed audio
   * that JUCE doesn't natively support (ADPCM, A-law, µ-law).
   * Uses dr_wav to decode the audio data on-the-fly (streaming).
   */
  AudioFormatReader *createDrWavReaderForWav(InputStream *sourceStream,
                                             int64 streamStartPos,
                                             bool deleteStreamIfOpeningFails) {
    // Reset to start for dr_wav to parse the WAV header
    sourceStream->setPosition(streamStartPos);

    // Create the streaming reader
    auto reader = std::make_unique<DrWavAudioFormatReader>(sourceStream);

    // Check if initialization succeeded
    if (reader->sampleRate == 0) {
      // Failed to initialize - dr_wav couldn't parse the file.
      // Note: we don't need to delete the stream here even if
      // deleteStreamIfOpeningFails is true, because the reader's destructor
      // will handle it via the base class AudioFormatReader.
      return nullptr;
    }

    return reader.release();
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
    // clang-format off
    // Format tags from mmreg.h / RFC 2361: https://www.rfc-editor.org/rfc/rfc2361.html
    switch (format) {
    case 0x0010: return "OKI ADPCM";
    case 0x0012: return "MediaSpace ADPCM";
    case 0x0013: return "Sierra ADPCM";
    case 0x0014: return "G.723 ADPCM";
    case 0x0015: return "DIGISTD";
    case 0x0016: return "DIGIFIX";
    case 0x0017: return "Dialogic OKI ADPCM";
    case 0x0020: return "Yamaha ADPCM";
    case 0x0021: return "SONARC";
    case 0x0022: return "DSP Group TrueSpeech";
    case 0x0023: return "ECHOSC1";
    case 0x0024: return "Audiofile AF36";
    case 0x0025: return "APTX";
    case 0x0026: return "Audiofile AF10";
    case 0x0030: return "Dolby AC-2";
    case 0x0031: return "GSM 6.10";
    case 0x0040: return "G.721 ADPCM";
    case 0x0041: return "G.728 CELP";
    case 0x0050: return "MPEG";
    case 0x0052: return "RT24";
    case 0x0053: return "PAC";
    case 0x0061: return "G.726 ADPCM";
    case 0x0062: return "G.722 ADPCM";
    case 0x0064: return "G.722.1";
    case 0x0065: return "G.728";
    case 0x0066: return "G.726";
    case 0x0067: return "G.722";
    case 0x0069: return "G.729";
    case 0x0070: return "VSELP";
    case 0x0075: return "VOXWARE";
    case 0x00FF: return "AAC";
    case 0x0111: return "VIVO G.723";
    case 0x0112: return "VIVO Siren";
    case 0x0160: return "Windows Media Audio v1";
    case 0x0161: return "Windows Media Audio v2";
    case 0x0162: return "Windows Media Audio Pro";
    case 0x0163: return "Windows Media Audio Lossless";
    case 0x0200: return "Creative ADPCM";
    case 0x0202: return "Creative FastSpeech8";
    case 0x0203: return "Creative FastSpeech10";
    case 0x1000: return "Olivetti GSM";
    case 0x1001: return "Olivetti ADPCM";
    case 0x1002: return "Olivetti CELP";
    case 0x1003: return "Olivetti SBC";
    case 0x1004: return "Olivetti OPR";
    case 0x1100: return "LH Codec";
    case 0x1400: return "Norris";
    case 0x1500: return "SoundSpace Musicompress";
    case 0x2000: return "Dolby AC-3 (SPDIF)";
    case 0x2001: return "DTS";
    default:     return nullptr;
    }
    // clang-format on
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchedWavAudioFormat)
};

} // namespace juce
