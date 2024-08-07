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

#include "../Plugin.h"

extern "C" {
#include <lame.h>
#include <lame_overrides.h>
}

namespace Pedalboard {

/*
 * A small C++ wrapper around the C-based LAME MP3 encoding functions.
 * Used mostly to avoid leaking memory.
 */
class EncoderWrapper {
public:
  EncoderWrapper() {}
  ~EncoderWrapper() { reset(); }

  operator bool() const { return lame != nullptr; }

  void reset() {
    lame_close(lame);
    lame = nullptr;
  }

  lame_t getContext() {
    if (!lame)
      lame = lame_init();
    return lame;
  }

private:
  lame_t lame = nullptr;
};

/*
 * A small C++ wrapper around the C-based LAME MP3 decoding functions.
 * Used mostly to avoid leaking memory.
 */
class DecoderWrapper {
public:
  DecoderWrapper() {}
  ~DecoderWrapper() { reset(); }
  operator bool() const { return hip != nullptr; }

  void reset() {
    hip_decode_exit(hip);
    hip = nullptr;
  }

  hip_t getContext() {
    if (!hip)
      hip = hip_decode_init();
    return hip;
  }

private:
  hip_t hip = nullptr;
};

/**
 * A class analogous to juce::AudioBuffer, but supporting
 * signed Int16 audio data (as provided by LAME).
 */
class Int16OutputBuffer {
public:
  void reset() {
    outputBuffers[0].fillWith(0);
    outputBuffers[1].fillWith(0);
    lastSample = 0;
  }

  /**
   * Given a channel, return a write pointer that can be
   * used to write signed mono 16-bit integer audio.
   */
  short *getWritePointerAtEnd(int channel) {
    return (short *)outputBuffers[channel].getData() + lastSample;
  }

  /*
   * Copy the data in this buffer into the provided AudioBlock<float>.
   * Returns the number of samples copied.
   */
  int copyToRightSideOf(juce::dsp::AudioBlock<float> outputBlock) {
    int samplesToOutput =
        std::min((unsigned long)outputBlock.getNumSamples(), lastSample);

    if (samplesToOutput) {
      int offsetInOutputBuffer = 0;
      if (samplesToOutput < outputBlock.getNumSamples()) {
        offsetInOutputBuffer = outputBlock.getNumSamples() - samplesToOutput;
      }

      for (int c = 0; c < outputBlock.getNumChannels(); c++) {
        juce::AudioDataConverters::convertInt16LEToFloat(
            (short *)outputBuffers[c].getData(),
            outputBlock.getChannelPointer(c) + offsetInOutputBuffer,
            samplesToOutput);
      }

      // Move the remaining content in the output buffer to the left hand side:
      if (samplesToOutput < lastSample) {
        unsigned long numSamplesRemaining = lastSample - samplesToOutput;
        for (int c = 0; c < outputBlock.getNumChannels(); c++) {
          std::memmove((short *)outputBuffers[c].getData(),
                       (short *)outputBuffers[c].getData() + samplesToOutput,
                       numSamplesRemaining * sizeof(short));
        }
        lastSample = numSamplesRemaining;
      } else {
        lastSample = 0;
      }
    }

    return samplesToOutput;
  }

  void incrementSampleCountBy(int add) { lastSample += add; }

  int getNumSamples() const { return lastSample; }

  void setSize(int samples) {
    for (int i = 0; i < 2; i++) {
      outputBuffers[i].ensureSize(sizeof(short) * samples);
      outputBuffers[i].fillWith(0);
    }
  }

private:
  juce::MemoryBlock outputBuffers[2];
  unsigned long lastSample = 0;
};

class MP3Compressor : public Plugin {
public:
  virtual ~MP3Compressor(){};

  void setVBRQuality(float newLevel) {
    if (newLevel < 0 || newLevel > 10) {
      throw std::domain_error("VBR quality must be greater than 0 and less "
                              "than 10. (Higher numbers are lower quality.)");
    }

    vbrLevel = newLevel;
    encoder.reset();
  }

  float getVBRQuality() const { return vbrLevel; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (!encoder || specChanged) {
      reset();

      if (lame_set_in_samplerate(encoder.getContext(), spec.sampleRate) != 0 ||
          lame_set_out_samplerate(encoder.getContext(), spec.sampleRate) != 0) {
        // TODO: It would be possible to add a resampler here to support
        // arbitrary-sample-rate audio.
        throw std::domain_error(
            "MP3 only supports 32kHz, 44.1kHz, and 48kHz audio. (Was passed " +
            juce::String(spec.sampleRate / 1000, 1).toStdString() +
            "kHz audio.)");
      }

      if (lame_set_num_channels(encoder.getContext(), spec.numChannels) != 0) {
        // TODO: It would be possible to run multiple independent mono encoders.
        throw std::domain_error(
            "MP3Compressor only supports mono or stereo audio. (Was passed " +
            std::to_string(spec.numChannels) + "-channel audio.)");
      }

      if (lame_set_VBR(encoder.getContext(), vbr_default) != 0) {
        throw std::domain_error(
            "MP3 encoder failed to set variable bit rate flag.");
      }

      if (lame_set_VBR_quality(encoder.getContext(), vbrLevel) != 0) {
        throw std::domain_error(
            "MP3 encoder failed to set variable bit rate quality to " +
            std::to_string(vbrLevel) + "!");
      }

      int ret = lame_init_params(encoder.getContext());
      if (ret != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to initialize MP3 encoder! (error " +
            std::to_string(ret) + ")");
      }

      // Why + 528 + 1? Pulled directly from the libmp3lame code.
      // An explanation supposedly exists in the old mp3encoder mailing list
      // archive. These values have been confirmed empirically, however.
      encoderInStreamLatency =
          lame_get_encoder_delay(encoder.getContext()) + 528 + 1;

      // Why add this latency? Again, not 100% sure - this has just
      // been empirically observed at all sample rates. Good thing we have
      // tests.
      if (lame_get_in_samplerate(encoder.getContext()) >= 32000) {
        encoderInStreamLatency += 1152;
      } else {
        encoderInStreamLatency += 576;
      }

      // More constants copied from the LAME documentation, to ensure
      // we never overrun the mp3 buffer:
      mp3Buffer.ensureSize((1.25 * MAX_LAME_MP3_BUFFER_SIZE_SAMPLES) + 7200);

      // Feed in some silence at the start so that LAME buffers up enough
      // samples Without this, we underrun our output buffer at the end of the
      // stream.
      std::vector<short> silence(ADDED_SILENCE_SAMPLES_AT_START);
      // Use the integer version rather than the float version for a bit of
      // extra speed.
      mp3BufferBytesFilled = lame_encode_buffer(
          encoder.getContext(), silence.data(), silence.data(), silence.size(),
          (unsigned char *)mp3Buffer.getData(), mp3Buffer.getSize());

      if (mp3BufferBytesFilled < 0) {
        throw std::runtime_error(
            "Failed to prime MP3 encoder! This is an internal Pedalboard error "
            "and should be reported.");
      }

      encoderInStreamLatency += silence.size();

      // Allow us to buffer up to (expected latency + 1 block of audio)
      // between the output of LAME and data returned back to Pedalboard.
      outputBuffer.setSize(encoderInStreamLatency + spec.maximumBlockSize);

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    auto ioBlock = context.getOutputBlock();

    if (mp3BufferBytesFilled > 0) {
      int samplesDecoded = hip_decode_threadsafe(
          decoder.getContext(), (unsigned char *)mp3Buffer.getData(),
          mp3BufferBytesFilled, outputBuffer.getWritePointerAtEnd(0),
          outputBuffer.getWritePointerAtEnd(1));

      outputBuffer.incrementSampleCountBy(samplesDecoded);
      mp3BufferBytesFilled = 0;
    }

    for (int blockStart = 0; blockStart < ioBlock.getNumSamples();
         blockStart += MAX_LAME_MP3_BUFFER_SIZE_SAMPLES) {
      int blockEnd = blockStart + MAX_LAME_MP3_BUFFER_SIZE_SAMPLES;
      if (blockEnd > ioBlock.getNumSamples())
        blockEnd = ioBlock.getNumSamples();

      int blockSize = blockEnd - blockStart;
      mp3BufferBytesFilled = lame_encode_buffer_ieee_float(
          encoder.getContext(),
          // If encoding in stereo, use both channels - otherwise, LAME
          // ignores the second channel argument here.
          ioBlock.getChannelPointer(0) + blockStart,
          ioBlock.getChannelPointer(ioBlock.getNumChannels() - 1) + blockStart,
          blockSize, (unsigned char *)mp3Buffer.getData(), mp3Buffer.getSize());

      if (mp3BufferBytesFilled == -1) {
        throw std::runtime_error(
            "Ran out of MP3 buffer space! This is an internal Pedalboard error "
            "and should be reported.");
      } else if (mp3BufferBytesFilled < 0) {
        throw std::runtime_error("MP3 encoder failed to encode with error " +
                                 std::to_string(mp3BufferBytesFilled) + ".");
      } else if (mp3BufferBytesFilled == 0 &&
                 lame_get_frameNum(encoder.getContext()) > 0) {
        mp3BufferBytesFilled = lame_encode_flush_nogap(
            encoder.getContext(), (unsigned char *)mp3Buffer.getData(),
            mp3Buffer.getSize());
      }

      // Decode frames from the buffer as soon as we get them:
      if (mp3BufferBytesFilled > 0) {

        int samplesDecoded = hip_decode_threadsafe(
            decoder.getContext(), (unsigned char *)mp3Buffer.getData(),
            mp3BufferBytesFilled, outputBuffer.getWritePointerAtEnd(0),
            outputBuffer.getWritePointerAtEnd(1));
        mp3BufferBytesFilled = 0;

        outputBuffer.incrementSampleCountBy(samplesDecoded);
      }
    }

    int samplesOutput = outputBuffer.copyToRightSideOf(ioBlock);
    samplesProduced += samplesOutput;
    int samplesToReturn =
        std::min((long)(samplesProduced - encoderInStreamLatency),
                 (long)ioBlock.getNumSamples());
    if (samplesToReturn < 0)
      samplesToReturn = 0;
    return samplesToReturn;
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();
    outputBuffer.reset();

    mp3Buffer.fillWith(0);
    mp3BufferBytesFilled = 0;

    samplesProduced = 0;
    encoderInStreamLatency = 0;
  }

protected:
  virtual int getLatencyHint() override {
    return encoderInStreamLatency + MAX_MP3_FRAME_SIZE_SAMPLES;
  }

private:
  float vbrLevel = 2.0;

  EncoderWrapper encoder;
  DecoderWrapper decoder;

  // The maximum number of samples to pass to LAME at once.
  // Determines roughly how big our output MP3 buffer has to be.
  static constexpr size_t MAX_LAME_MP3_BUFFER_SIZE_SAMPLES = 32;
  static constexpr size_t MAX_MP3_FRAME_SIZE_SAMPLES = 1152;

  // This is the number of samples we add at the start of the LAME stream to
  // give us enough of a "head start" to avoid underflowing our MP3 buffer when
  // the stream finishes. This value, like many others, was determined
  // empirically.
  static constexpr long ADDED_SILENCE_SAMPLES_AT_START = 200;

  Int16OutputBuffer outputBuffer;
  long samplesProduced = 0;
  long encoderInStreamLatency = 0;

  // A memory block to use to temporarily hold MP3 frames (encoded bytes).
  juce::MemoryBlock mp3Buffer;
  int mp3BufferBytesFilled = 0;
};

inline void init_mp3_compressor(py::module &m) {
  py::class_<MP3Compressor, Plugin, std::shared_ptr<MP3Compressor>>(
      m, "MP3Compressor",
      "An MP3 compressor plugin that runs the LAME MP3 encoder in real-time to "
      "add compression artifacts to the audio stream.\n\nCurrently only "
      "supports variable bit-rate mode (VBR) and accepts a floating-point VBR "
      "quality value (between 0.0 and 10.0; lower is better).\n\nNote that the "
      "MP3 format only supports 8kHz, 11025Hz, 12kHz, 16kHz, 22050Hz, 24kHz, "
      "32kHz, 44.1kHz, and 48kHz audio; if an unsupported sample rate is "
      "provided, an exception will be thrown at processing time.")
      .def(py::init([](float vbr_quality) {
             auto plugin = std::make_unique<MP3Compressor>();
             plugin->setVBRQuality(vbr_quality);
             return plugin;
           }),
           py::arg("vbr_quality") = 2.0)
      .def("__repr__",
           [](const MP3Compressor &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.MP3Compressor";
             ss << " vbr_quality=" << plugin.getVBRQuality();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("vbr_quality", &MP3Compressor::getVBRQuality,
                    &MP3Compressor::setVBRQuality);
}

}; // namespace Pedalboard