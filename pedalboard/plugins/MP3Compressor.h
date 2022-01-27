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

#include "../Plugin.h"

extern "C" {
#include <lame.h>
}


namespace Pedalboard {

/*
 * A small C++ wrapper around the C-based LAME MP3 encoding functions.
 * Used mostly to avoid leaking memory.
 */
class EncoderWrapper {
public:
  EncoderWrapper() { lame = lame_init(); }
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
  lame_t lame;
};

/*
 * A small C++ wrapper around the C-based LAME MP3 decoding functions.
 * Used mostly to avoid leaking memory.
 */
class DecoderWrapper {
public:
  DecoderWrapper() { hip = hip_decode_init(); }
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
  hip_t hip;
};

class MP3Compressor : public Plugin {
public:
  virtual ~MP3Compressor(){};

  void setVBRQuality(float newLevel) {
    if (newLevel < 0 || newLevel > 10) {
      throw std::runtime_error("VBR quality must be greater than 0 and less "
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

      if (lame_set_in_samplerate(encoder.getContext(), spec.sampleRate) != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to set input sample rate.");
      }

      if (lame_set_out_samplerate(encoder.getContext(), spec.sampleRate) != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to set output sample rate.");
      }

      if (lame_set_num_channels(encoder.getContext(), spec.numChannels) != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to set number of channels.");
      }

      // TODO: handle when the output sample rate doesn't match one of the
      // expected rates from encoder.

      if (lame_set_VBR(encoder.getContext(), vbr_default) != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to set variable bit rate flag.");
      }

      if (lame_set_VBR_quality(encoder.getContext(), vbrLevel) != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to set variable bit rate quality to " +
            std::to_string(vbrLevel) + "!");
      }

      int ret = lame_init_params(encoder.getContext());
      if (ret != 0) {
        throw std::runtime_error(
            "MP3 encoder failed to initialize MP3 encoder! (error " +
            std::to_string(ret) + ")");
      }

      // Why + 528 + 1? Pulled directly from the libmp3lame code; not 100% sure.
      // Values have been confirmed empirically, however.
      encoderInStreamLatency =
          lame_get_encoder_delay(encoder.getContext()) + 528 + 1;

      int outputBufferSize = (16384 + spec.maximumBlockSize) * sizeof(short);
      for (int i = 0; i < 2; i++) {
        outputBuffers[i].setSize(outputBufferSize);
        outputBuffers[i].fillWith(0);
      }

      outputBufferLastSample = 0;

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {

    auto ioBlock = context.getOutputBlock();
    for (size_t i = 0; i < ioBlock.getNumSamples();
         i += MAX_MP3_FRAME_SIZE_SAMPLES) {
      jassert(ioBlock.getNumChannels() == 2);

      unsigned char mp3Buffer[MAX_MP3_FRAME_SIZE_BYTES];

      int frameSize =
          std::min(MAX_MP3_FRAME_SIZE_SAMPLES, ioBlock.getNumSamples() - i);

      int numBytesEncoded = lame_encode_buffer_ieee_float(
          encoder.getContext(), ioBlock.getChannelPointer(0) + i,
          ioBlock.getChannelPointer(1) + i, frameSize, mp3Buffer,
          MAX_MP3_FRAME_SIZE_BYTES);

      if (numBytesEncoded < 0) {
        throw std::runtime_error("MP3 encoder failed to encode with error " +
                                 std::to_string(numBytesEncoded) + ".");
      }

      samplesInEncodingBuffer += frameSize;

      // Decode frames from the buffer as soon as we get them:
      if (numBytesEncoded > 0) {
        int samplesDecoded = hip_decode(
            decoder.getContext(), mp3Buffer, numBytesEncoded,
            (short *)outputBuffers[0].getData() + outputBufferLastSample,
            (short *)outputBuffers[1].getData() + outputBufferLastSample);
        outputBufferLastSample += samplesDecoded;
        samplesInEncodingBuffer -= samplesDecoded;
        if (encodingLatency == 0) {
          encodingLatency = samplesInEncodingBuffer;
        }
      }
    }

    int samplesToOutput = std::min(ioBlock.getNumSamples(),
                                   (size_t)outputBufferLastSample);
    samplesProduced += samplesToOutput;
    int offsetInOutputBuffer = 0;
    if (samplesToOutput < ioBlock.getNumSamples()) {
      offsetInOutputBuffer = ioBlock.getNumSamples() - samplesToOutput;
    }

    for (int c = 0; c < ioBlock.getNumChannels(); c++) {
      juce::AudioDataConverters::convertInt16LEToFloat(
          (short *)outputBuffers[c].getData(),
          ioBlock.getChannelPointer(c) + offsetInOutputBuffer, samplesToOutput);
    }

    // Move the remaining content in the output buffer to the left hand side:
    int numSamplesRemaining =
        std::max(0L, outputBufferLastSample - samplesToOutput);
    if (numSamplesRemaining) {
      for (int c = 0; c < ioBlock.getNumChannels(); c++) {
        std::memmove((short *)outputBuffers[c].getData(),
                     (short *)outputBuffers[c].getData() + samplesToOutput,
                     numSamplesRemaining * sizeof(short));
      }
    }
    outputBufferLastSample = numSamplesRemaining;

    long usableSamplesProduced = std::max(
        0L, samplesProduced - (encodingLatency + encoderInStreamLatency));
    int samplesInBuffer = static_cast<int>(
        std::min(usableSamplesProduced, (long)samplesToOutput));
    return samplesInBuffer;
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();

    outputBuffers[0].fillWith(0);
    outputBuffers[1].fillWith(0);

    outputBufferLastSample = 0;
    samplesProduced = 0;
    samplesInEncodingBuffer = 0;
    encodingLatency = 0;
    encoderInStreamLatency = 0;
  }

protected:
  virtual int getLatencyHint() override {
    if (encodingLatency == 0) {
      // If we haven't started encoding yet, over-estimate
      // our latency bounds, as we'll almost always have a fairly large buffer.
      return encoderInStreamLatency + MAX_MP3_FRAME_SIZE_SAMPLES;
    }
    return encoderInStreamLatency + encodingLatency;
  }

private:
  float vbrLevel = 2.0;

  EncoderWrapper encoder;
  DecoderWrapper decoder;

  static constexpr size_t MAX_MP3_FRAME_SIZE_BYTES = 8192;
  static constexpr size_t MAX_MP3_FRAME_SIZE_SAMPLES = 1152;

  // This should really be an AudioBuffer<short>, but JUCE doesn't offer that.
  // TODO: This should be numChannels to support more than mono and stereo
  // audio!
  juce::MemoryBlock outputBuffers[2];
  long outputBufferLastSample = 0;
  long samplesProduced = 0;
  long samplesInEncodingBuffer = 0;

  // We have two latency numbers to consider here: the amount of latency between
  // supplying samples to LAME and getting samples back, and then the amount of
  // latency within the stream coming out of LAME itself.
  long encodingLatency = 0;
  long encoderInStreamLatency = 0;
};

inline void init_mp3_compressor(py::module &m) {
  py::class_<MP3Compressor, Plugin>(
      m, "MP3Compressor",
      "Apply an MP3 compressor to the audio to reduce its quality.")
      .def(py::init([](float vbr_quality) {
             auto plugin = new MP3Compressor();
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