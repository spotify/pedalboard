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
  EncoderWrapper() { }
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
  DecoderWrapper() { }
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

class Int16OutputBuffer {
public:
  void reset() {
    outputBuffers[0].fillWith(0);
    outputBuffers[1].fillWith(0);
    lastSample = 0;
  }

  short *getWritePointerAtEnd(int channel) {
    return (short *)outputBuffers[channel].getData() + lastSample;
  }
  
  int copyToRightSideOf(juce::dsp::AudioBlock<float> outputBlock, int offsetInThisBuffer = 0) {
    int samplesToOutput = std::min(outputBlock.getNumSamples(),
                                   (size_t)std::max(0L, lastSample - offsetInThisBuffer));

    if (samplesToOutput) {
      int offsetInOutputBuffer = 0;
      if (samplesToOutput < outputBlock.getNumSamples()) {
        offsetInOutputBuffer = outputBlock.getNumSamples() - samplesToOutput;
      }

      for (int c = 0; c < outputBlock.getNumChannels(); c++) {
        juce::AudioDataConverters::convertInt16LEToFloat(
            (short *)outputBuffers[c].getData() + offsetInThisBuffer,
            outputBlock.getChannelPointer(c) + offsetInOutputBuffer, samplesToOutput);
      }

      // Move the remaining content in the output buffer to the left hand side:
      int numSamplesRemaining =
          std::max(0L, lastSample - samplesToOutput);
      if (numSamplesRemaining) {
        for (int c = 0; c < outputBlock.getNumChannels(); c++) {
          std::memmove((short *)outputBuffers[c].getData(),
                      (short *)outputBuffers[c].getData() + samplesToOutput,
                      numSamplesRemaining * sizeof(short));
        }
      }
      lastSample = numSamplesRemaining;
    }

    return samplesToOutput;
  }

  void incrementSampleCountBy(int add) {
    lastSample += add;
  }

  void setSize(int samples) {
    for (int i = 0; i < 2; i++) {
      outputBuffers[i].ensureSize(sizeof(short) * samples);
      outputBuffers[i].fillWith(0);
    }
  }

private:
  juce::MemoryBlock outputBuffers[2];
  long lastSample = 0;
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
      encoderInStreamLatency = lame_get_encoder_delay(encoder.getContext()) + 528 + 1;
  
      // Constants copied from the LAME documentation:
      mp3Buffer.ensureSize((int) (1.25 * MAX_LAME_MP3_BUFFER_SIZE_SAMPLES) + 7200);
      outputBuffer.setSize(32768 + spec.maximumBlockSize * 2);

      // Feed in some silence at the start so that LAME buffers up enough samples
      // Without this, we underrun our output buffer at the end of the stream.
      int samplesToAdd = addedSilenceAtStart;

      float *silence = new float[samplesToAdd];
      // Note: this buffer will be discarded on the other end of LAME,
      // but in case there's any crossfade or leakage between frames,
      // we zero this out here.
      for (int i = 0; i < samplesToAdd; i++) silence[i] = 0;

      int numBytesEncoded = lame_encode_buffer_ieee_float(
        encoder.getContext(),
        silence,
        silence,
        samplesToAdd,
        (unsigned char *)mp3Buffer.getData(),
        mp3Buffer.getSize());

      delete[] silence;

      encoderInStreamLatency += samplesToAdd;
      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    auto ioBlock = context.getOutputBlock();

    for (int blockStart = 0; blockStart < ioBlock.getNumSamples(); blockStart += MAX_LAME_MP3_BUFFER_SIZE_SAMPLES) {
      int blockEnd = blockStart + MAX_LAME_MP3_BUFFER_SIZE_SAMPLES;
      if (blockEnd > ioBlock.getNumSamples()) blockEnd = ioBlock.getNumSamples();

      int blockSize = blockEnd - blockStart;
      int numBytesEncoded = lame_encode_buffer_ieee_float(
        encoder.getContext(),
        // If encoding in stereo, use both channels - otherwise, LAME
        // ignores the second channel argument here.
        ioBlock.getChannelPointer(0) + blockStart,
        ioBlock.getChannelPointer(ioBlock.getNumChannels() - 1) + blockStart,
        blockSize,
        (unsigned char *)mp3Buffer.getData(),
        mp3Buffer.getSize());

      samplesInEncodingBuffer += blockSize;

      if (numBytesEncoded == -1) {
        throw std::runtime_error("Ran out of MP3 buffer space! Try using a smaller buffer_size.");
      } else if (numBytesEncoded < 0) {
        throw std::runtime_error("MP3 encoder failed to encode with error " +
                                  std::to_string(numBytesEncoded) + ".");
      } else if (numBytesEncoded == 0 && lame_get_frameNum(encoder.getContext()) > 0) {
        numBytesEncoded = lame_encode_flush_nogap(encoder.getContext(), (unsigned char *)mp3Buffer.getData(), mp3Buffer.getSize());
      }

      // Decode frames from the buffer as soon as we get them:
      if (numBytesEncoded > 0) {
        // When parsing the first frame, hip_decode will fail to return anything. Get around this here:
        int numDecodes = 1;
        if (isFirstFrame) {
          numDecodes = 2;
        }

        for (int i = 0; i < numDecodes; i++) {
          int samplesDecoded = hip_decode(
              decoder.getContext(),
              (unsigned char *)mp3Buffer.getData(),
              numBytesEncoded,
              outputBuffer.getWritePointerAtEnd(0),
              outputBuffer.getWritePointerAtEnd(1));
          
          outputBuffer.incrementSampleCountBy(samplesDecoded);
          samplesInEncodingBuffer -= samplesDecoded;

          isFirstFrame = false;
        }
      }
    }

    int samplesOutput = outputBuffer.copyToRightSideOf(ioBlock);
    samplesProduced += samplesOutput;
    int samplesToReturn = std::min((long) samplesProduced - encodingLatency - encoderInStreamLatency, (long) ioBlock.getNumSamples());
    if (samplesToReturn < 0) samplesToReturn = 0;
    return samplesToReturn;
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();
    outputBuffer.reset();

    mp3Buffer.fillWith(0);

    samplesProduced = 0;
    samplesInEncodingBuffer = 0;
    encodingLatency = 1152;
    encoderInStreamLatency = 0;
    isFirstFrame = true;
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

  /**
   * The maximum number of samples to pass to LAME at once.
   * Determines roughly how big our output MP3 buffer has to be.
   */
  static constexpr size_t MAX_LAME_MP3_BUFFER_SIZE_SAMPLES = 32;
  static constexpr size_t MAX_MP3_FRAME_SIZE_SAMPLES = 1152;

  Int16OutputBuffer outputBuffer;
  long samplesProduced = 0;
  long samplesInEncodingBuffer = 0;

  juce::MemoryBlock mp3Buffer;

  // We have two latency numbers to consider here: the amount of latency between
  // supplying samples to LAME and getting samples back, and then the amount of
  // latency within the stream coming out of LAME itself.
  long encodingLatency = 1152;
  long encoderInStreamLatency = 0;

  // This is the number of samples we add at the start of the LAME stream to give
  // us enough of a "head start" to avoid underflowing our MP3 buffer when the
  // stream finishes.
  long addedSilenceAtStart = 1152;

  bool isFirstFrame = true;
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