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
#include "../plugins/MP3Compressor.h"

extern "C" {
#include <lame.h>
}

static const inline int MAXIMUM_MP3_FRAME_SIZE = 7200;

namespace Pedalboard {
class LameMP3AudioFormat : public juce::AudioFormat {
public:
  LameMP3AudioFormat() : juce::AudioFormat("MP3", ".mp3"){};
  ~LameMP3AudioFormat(){};

  juce::Array<int> getPossibleSampleRates() { return {32000, 44100, 48000}; }
  juce::Array<int> getPossibleBitDepths() { return {16}; }
  bool canDoStereo() { return true; }
  bool canDoMono() { return true; }
  bool isCompressed() { return true; }

  juce::StringArray getQualityOptions() {
    juce::StringArray opts(VBR_OPTIONS);

    for (int i = 0; i < juce::numElementsInArray(CBR_OPTIONS); ++i)
      opts.add(juce::String(CBR_OPTIONS[i]) + " kbps");

    return opts;
  }

  juce::AudioFormatReader *createReaderFor(juce::InputStream *,
                                           bool deleteStreamIfOpeningFails) {
    return nullptr;
  }

  juce::AudioFormatWriter *
  createWriterFor(juce::OutputStream *out, double sampleRateToUse,
                  unsigned int numberOfChannels, int bitsPerSample,
                  const juce::StringPairArray &metadataValues,
                  int qualityOptionIndex) {

    try {
      if (out != nullptr)
        return new Writer(out, sampleRateToUse, numberOfChannels,
                          qualityOptionIndex);
    } catch (...) {
      // TODO: Make WriteableAudioFile exception-safe so we can bubble up more
      // useful error messages.
    }

    return nullptr;
  }
  using juce::AudioFormat::createWriterFor;

  class Writer : public juce::AudioFormatWriter {
  public:
    Writer(juce::OutputStream *destStream, double sampleRate,
           unsigned int numberOfChannels, int qualityOptionIndex)
        : juce::AudioFormatWriter(nullptr, "MP3", sampleRate, numberOfChannels,
                                  16) {
      usesFloatingPointData = false;

      // Suppress all error logging from LAME directly to stdout
      lame_set_errorf(encoder.getContext(), nullptr);
      lame_set_debugf(encoder.getContext(), nullptr);
      lame_set_msgf(encoder.getContext(), nullptr);

      if (lame_set_in_samplerate(encoder.getContext(), sampleRate) != 0 ||
          lame_set_out_samplerate(encoder.getContext(), sampleRate) != 0) {
        throw std::domain_error(
            "MP3 only supports 32kHz, 44.1kHz, and 48kHz audio. (Was passed "
            "a sample rate of " +
            juce::String(sampleRate / 1000, 1).toStdString() + "kHz.)");
      }

      if (lame_set_num_channels(encoder.getContext(), numChannels) != 0) {
        throw std::domain_error(
            "MP3 only supports mono or stereo audio. (Was passed " +
            std::to_string(numChannels) + "-channel audio.)");
      }

      if (qualityOptionIndex < NUM_VBR_OPTIONS) {
        if (lame_set_VBR(encoder.getContext(), vbr_default) != 0) {
          throw std::domain_error(
              "MP3 encoder failed to set variable bit rate flag.");
        }

        // VBR options:
        if (lame_set_VBR_quality(encoder.getContext(), qualityOptionIndex) !=
            0) {
          throw std::domain_error(
              "MP3 encoder failed to set variable bit rate quality to " +
              std::to_string(qualityOptionIndex) + "!");
        }
      } else if (qualityOptionIndex <
                 (NUM_VBR_OPTIONS + juce::numElementsInArray(CBR_OPTIONS))) {
        if (lame_set_VBR(encoder.getContext(), vbr_off) != 0) {
          throw std::domain_error(
              "MP3 encoder failed to set constant bit rate flag.");
        }

        // CBR options:
        if (lame_set_brate(encoder.getContext(),
                           CBR_OPTIONS[qualityOptionIndex - NUM_VBR_OPTIONS]) !=
            0) {
          throw std::domain_error(
              "MP3 encoder failed to set constant bit rate quality to " +
              std::to_string(
                  CBR_OPTIONS[qualityOptionIndex - NUM_VBR_OPTIONS]) +
              "!");
        }
      } else {
        throw std::domain_error("Unsupported quality index!");
      }

      int ret = lame_init_params(encoder.getContext());
      if (ret != 0) {
        throw std::runtime_error("Failed to initialize MP3 encoder! (error " +
                                 std::to_string(ret) + ")");
      }

      // Assign this at the end, as the AudioFormatWriter destructor
      // automatically deletes this pointer, and we don't want that
      // to happen if our constructor fails.
      output = destStream;
    }

    virtual ~Writer() override {
      if (!output)
        return;

      std::vector<unsigned char> mp3buf(MAXIMUM_MP3_FRAME_SIZE);

      int bytesWritten =
          lame_encode_flush(encoder.getContext(), mp3buf.data(), mp3buf.size());
      if (bytesWritten > 0) {
        output->write(mp3buf.data(), bytesWritten);
      }

      writeVBRTag();
      flush();
    };

    virtual bool write(const int **samplesToWrite, int numSamples) {
      std::vector<unsigned char> encodedMp3Buffer(1.25 * numSamples +
                                                  MAXIMUM_MP3_FRAME_SIZE);

      std::vector<std::vector<short>> shortSamples(numChannels);
      for (int c = 0; c < numChannels; c++) {
        shortSamples[c].resize(numSamples);
        for (int i = 0; i < numSamples; i++) {
          shortSamples[c][i] = samplesToWrite[c][i] >> 16;
        }
      }

      int mp3BufferBytesFilled = lame_encode_buffer(
          encoder.getContext(), shortSamples[0].data(),
          numChannels == 1 ? nullptr : shortSamples[1].data(), numSamples,
          (unsigned char *)encodedMp3Buffer.data(), encodedMp3Buffer.size());

      if (mp3BufferBytesFilled < 0) {
        return false;
      }

      if (mp3BufferBytesFilled) {
        return output->write((unsigned char *)encodedMp3Buffer.data(),
                             mp3BufferBytesFilled);
      }

      return true;
    }

    virtual bool flush() {
      if (!output)
        return false;
      output->flush();
      return true;
    }

    void writeVBRTag() {
      /**
       * MP3 files in VBR mode are expected to have
       * a single frame after their ID3 tags indicating
       * information about how to seek through the
       * file effectively.
       *
       * The LAME encoder automatically
       * writes an empty frame at the start of the file
       * with the expectation that the encoder will
       * overwrite this frame once the file is done.
       *
       * LAME assumes we're writing to a file, which
       * is not true here - we're writing to a seekable
       * output stream, so we have to reimplement this
       * logic ourselves.
       */
      if (!output)
        return;

      size_t currentPosition = output->getPosition();

      uint8_t buffer[MAXIMUM_MP3_FRAME_SIZE];
      size_t frameTagSize =
          lame_get_lametag_frame(encoder.getContext(), buffer, sizeof(buffer));

      if (frameTagSize > sizeof(buffer)) {
        return;
      } else if (frameTagSize < 1) {
        return;
      }

      if (!output->setPosition(0)) {
        return;
      }

      if (!output->write(buffer, frameTagSize)) {
        return;
      }

      output->setPosition(currentPosition);
    }

  private:
    EncoderWrapper encoder;
  };

private:
  static inline const char *VBR_OPTIONS[] = {
      "V0 (best)", "V1", "V2", "V3", "V4 (normal)",
      "V5",        "V6", "V7", "V8", "V9 (smallest)",
      nullptr};
  static const int NUM_VBR_OPTIONS = 10;
  static inline const int CBR_OPTIONS[] = {32,  40,  48,  56,  64,  80,  96,
                                           112, 128, 160, 192, 224, 256, 320};
};

} // namespace Pedalboard
