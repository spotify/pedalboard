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
#include <gsm.h>
}

namespace Pedalboard {

/*
 * A small C++ wrapper around the C-based libgsm object.
 * Used mostly to avoid leaking memory.
 */
class GSMWrapper {
public:
  GSMWrapper() {}
  ~GSMWrapper() { reset(); }

  operator bool() const { return _gsm != nullptr; }

  void reset() {
    gsm_destroy(_gsm);
    _gsm = nullptr;
  }

  gsm getContext() {
    if (!_gsm)
      _gsm = gsm_create();
    return _gsm;
  }

private:
  gsm _gsm = nullptr;
};

class GSMCompressor : public Plugin {
public:
  virtual ~GSMCompressor(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (!encoder || specChanged) {
      reset();

      resamplerRatio = spec.sampleRate / GSM_SAMPLE_RATE;
      inverseResamplerRatio = GSM_SAMPLE_RATE / spec.sampleRate;

      gsmFrameSizeInNativeSampleRate = GSM_FRAME_SIZE_SAMPLES * resamplerRatio;
      int maximumBlockSizeInGSMSampleRate =
          spec.maximumBlockSize / resamplerRatio;

      // Store the remainder of the input: any samples that weren't consumed in
      // one pushSamples() call but would be consumable in the next one.
      inputReservoir.setSize(1, (int)std::ceil(resamplerRatio) +
                                    spec.maximumBlockSize);

      if (!encoder.getContext()) {
        throw std::runtime_error("Failed to initialize GSM encoder.");
      }
      if (!decoder.getContext()) {
        throw std::runtime_error("Failed to initialize GSM decoder.");
      }

      inStreamLatency = 0;

      // Add the resamplers' latencies so the output is properly aligned:
      inStreamLatency += nativeToGSMResampler.getBaseLatency() * resamplerRatio;
      inStreamLatency += gsmToNativeResampler.getBaseLatency() * resamplerRatio;

      resampledBuffer.setSize(1, maximumBlockSizeInGSMSampleRate +
                                     GSM_FRAME_SIZE_SAMPLES +
                                     (inStreamLatency / resamplerRatio));
      outputBuffer.setSize(1, spec.maximumBlockSize +
                                  gsmFrameSizeInNativeSampleRate +
                                  inStreamLatency);

      // Feed one GSM frame's worth of silence at the start so that we
      // can tolerate different buffer sizes without underrunning any buffers.
      std::vector<float> silence(gsmFrameSizeInNativeSampleRate);
      inStreamLatency += silence.size();
      pushSamples(silence.data(), silence.size());

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    auto ioBlock = context.getOutputBlock();

    // Mix all channels to mono first, if necessary; GSM (in reality) is
    // mono-only.
    if (ioBlock.getNumChannels() > 1) {
      float channelVolume = 1.0f / ioBlock.getNumChannels();
      for (int i = 0; i < ioBlock.getNumChannels(); i++) {
        ioBlock.getSingleChannelBlock(i) *= channelVolume;
      }

      // Copy all of the latter channels into the first channel,
      // which will be used for processing:
      auto firstChannel = ioBlock.getSingleChannelBlock(0);
      for (int i = 1; i < ioBlock.getNumChannels(); i++) {
        firstChannel += ioBlock.getSingleChannelBlock(i);
      }
    }

    // Actually do the GSM processing!
    pushSamples(ioBlock.getChannelPointer(0), ioBlock.getNumSamples());
    int samplesOutput =
        pullSamples(ioBlock.getChannelPointer(0), ioBlock.getNumSamples());

    // Copy the mono signal back out to all other channels:
    if (ioBlock.getNumChannels() > 1) {
      auto firstChannel = ioBlock.getSingleChannelBlock(0);
      for (int i = 1; i < ioBlock.getNumChannels(); i++) {
        ioBlock.getSingleChannelBlock(i).copyFrom(firstChannel);
      }
    }

    samplesProduced += samplesOutput;
    int samplesToReturn = std::min((long)(samplesProduced - inStreamLatency),
                                   (long)ioBlock.getNumSamples());
    if (samplesToReturn < 0)
      samplesToReturn = 0;

    return samplesToReturn;
  }

  /*
   * Return the number of samples needed for this
   * plugin to return a single GSM frame's worth of audio.
   */
  int spaceAvailableInResampledBuffer() const {
    return resampledBuffer.getNumSamples() - samplesInResampledBuffer;
  }

  int spaceAvailableInOutputBuffer() const {
    return outputBuffer.getNumSamples() - samplesInOutputBuffer;
  }

  /*
   * Push a certain number of input samples into the internal buffer(s)
   * of this plugin, as GSM coding processes audio 160 samples at a time.
   */
  void pushSamples(float *inputSamples, int numInputSamples) {
    float expectedOutputSamples = numInputSamples / resamplerRatio;

    if (spaceAvailableInResampledBuffer() < expectedOutputSamples) {
      throw std::runtime_error(
          "More samples were provided than can be buffered! This is an "
          "internal Pedalboard error and should be reported. Buffer had " +
          std::to_string(samplesInResampledBuffer) + "/" +
          std::to_string(resampledBuffer.getNumSamples()) +
          " samples at 8kHz, but was provided " +
          std::to_string(expectedOutputSamples) + ".");
    }

    float *resampledBufferPointer =
        resampledBuffer.getWritePointer(0) + samplesInResampledBuffer;

    int samplesUsed = 0;
    if (samplesInInputReservoir) {
      // Copy the input samples into the input reservoir and use that as the
      // resampler's input:
      expectedOutputSamples += (float)samplesInInputReservoir / resamplerRatio;
      inputReservoir.copyFrom(0, samplesInInputReservoir, inputSamples,
                              numInputSamples);
      samplesUsed = nativeToGSMResampler.process(
          resamplerRatio, inputReservoir.getReadPointer(0),
          resampledBufferPointer, expectedOutputSamples);

      if (samplesUsed < numInputSamples + samplesInInputReservoir) {
        // Take the missing samples and put them at the start of the input
        // reservoir for next time:
        int unusedInputSampleCount =
            (numInputSamples + samplesInInputReservoir) - samplesUsed;
        inputReservoir.copyFrom(0, 0,
                                inputReservoir.getReadPointer(0) + samplesUsed,
                                unusedInputSampleCount);
        samplesInInputReservoir = unusedInputSampleCount;
      } else {
        samplesInInputReservoir = 0;
      }
    } else {
      samplesUsed = nativeToGSMResampler.process(resamplerRatio, inputSamples,
                                                 resampledBufferPointer,
                                                 expectedOutputSamples);

      if (samplesUsed < numInputSamples) {
        // Take the missing samples and put them at the start of the input
        // reservoir for next time:
        int unusedInputSampleCount = numInputSamples - samplesUsed;
        inputReservoir.copyFrom(0, 0, inputSamples + samplesUsed,
                                unusedInputSampleCount);
        samplesInInputReservoir = unusedInputSampleCount;
      }
    }

    samplesInResampledBuffer += expectedOutputSamples;

    performEncodeAndDecode();
  }

  int pullSamples(float *outputSamples, int maxOutputSamples) {
    performEncodeAndDecode();

    // Copy the data out of outputBuffer and into the provided pointer, at
    // the right side of the buffer:
    int samplesToCopy = std::min(samplesInOutputBuffer, maxOutputSamples);
    int offsetInOutput = maxOutputSamples - samplesToCopy;
    juce::FloatVectorOperations::copy(outputSamples + offsetInOutput,
                                      outputBuffer.getWritePointer(0),
                                      samplesToCopy);
    samplesInOutputBuffer -= samplesToCopy;

    // Move remaining samples to the left side of the output buffer:
    std::memmove((char *)outputBuffer.getWritePointer(0),
                 (char *)(outputBuffer.getWritePointer(0) + samplesToCopy),
                 samplesInOutputBuffer * sizeof(float));

    performEncodeAndDecode();

    return samplesToCopy;
  }

  void performEncodeAndDecode() {
    while (samplesInResampledBuffer >= GSM_FRAME_SIZE_SAMPLES) {
      float *encodeBuffer = resampledBuffer.getWritePointer(0);

      // Convert samples to signed 16-bit integer first,
      // then pass to the GSM Encoder, then immediately back
      // around to the GSM decoder.
      short frame[GSM_FRAME_SIZE_SAMPLES];

      juce::AudioDataConverters::convertFloatToInt16LE(encodeBuffer, frame,
                                                       GSM_FRAME_SIZE_SAMPLES);

      // Actually do the GSM encoding/decoding:
      gsm_frame encodedFrame;

      gsm_encode(encoder.getContext(), frame, encodedFrame);
      if (gsm_decode(decoder.getContext(), encodedFrame, frame) < 0) {
        throw std::runtime_error("GSM decoder could not decode frame!");
      }

      if (spaceAvailableInOutputBuffer() < gsmFrameSizeInNativeSampleRate) {
        throw std::runtime_error(
            "Not enough space in output buffer to store a GSM frame! Needed " +
            std::to_string(gsmFrameSizeInNativeSampleRate) +
            " samples but only had " +
            std::to_string(spaceAvailableInOutputBuffer()) +
            " samples available. This is "
            "an internal Pedalboard error and should be reported.");
      }
      float *outputBufferPointer =
          outputBuffer.getWritePointer(0) + samplesInOutputBuffer;

      juce::AudioDataConverters::convertInt16LEToFloat(
          frame, gsmOutputFrame + samplesInGsmOutputFrame,
          GSM_FRAME_SIZE_SAMPLES);
      samplesInGsmOutputFrame += GSM_FRAME_SIZE_SAMPLES;

      // Resample back up to the native sample rate and store in outputBuffer,
      // using gsmOutputFrame as a temporary buffer to store up to 1 extra
      // sample to compensate for rounding errors:
      int expectedOutputSamples = samplesInGsmOutputFrame * resamplerRatio;
      int samplesConsumed = gsmToNativeResampler.process(
          inverseResamplerRatio, gsmOutputFrame, outputBufferPointer,
          expectedOutputSamples);
      std::memmove((char *)gsmOutputFrame,
                   (char *)(gsmOutputFrame + samplesConsumed),
                   (samplesInGsmOutputFrame - samplesConsumed) * sizeof(float));

      samplesInGsmOutputFrame -= samplesConsumed;
      samplesInOutputBuffer += expectedOutputSamples;

      // Now that we're done with this chunk of resampledBuffer, move its
      // contents to the left:
      int samplesRemainingInResampledBuffer =
          samplesInResampledBuffer - GSM_FRAME_SIZE_SAMPLES;
      std::memmove(
          (char *)resampledBuffer.getWritePointer(0),
          (char *)(resampledBuffer.getWritePointer(0) + GSM_FRAME_SIZE_SAMPLES),
          samplesRemainingInResampledBuffer * sizeof(float));
      samplesInResampledBuffer -= GSM_FRAME_SIZE_SAMPLES;
    }
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();
    nativeToGSMResampler.reset();
    gsmToNativeResampler.reset();

    resampledBuffer.clear();
    outputBuffer.clear();
    inputReservoir.clear();

    samplesInResampledBuffer = 0;
    samplesInOutputBuffer = 0;
    samplesInInputReservoir = 0;
    samplesInGsmOutputFrame = 0;

    samplesProduced = 0;
    inStreamLatency = 0;
  }

protected:
  virtual int getLatencyHint() override { return inStreamLatency; }

private:
  static constexpr size_t GSM_FRAME_SIZE_SAMPLES = 160;
  static constexpr float GSM_SAMPLE_RATE = 8000;

  double resamplerRatio = 1.0;
  double inverseResamplerRatio = 1.0;
  float gsmFrameSizeInNativeSampleRate;

  juce::AudioBuffer<float> inputReservoir;
  int samplesInInputReservoir = 0;

  juce::Interpolators::Lagrange nativeToGSMResampler;
  juce::AudioBuffer<float> resampledBuffer;
  int samplesInResampledBuffer = 0;

  GSMWrapper encoder;
  GSMWrapper decoder;

  juce::Interpolators::Lagrange gsmToNativeResampler;
  float gsmOutputFrame[GSM_FRAME_SIZE_SAMPLES + 1];
  int samplesInGsmOutputFrame = 0;

  juce::AudioBuffer<float> outputBuffer;
  int samplesInOutputBuffer = 0;

  int samplesProduced = 0;
  int inStreamLatency = 0;
};

inline void init_gsm_compressor(py::module &m) {
  py::class_<GSMCompressor, Plugin>(
      m, "GSMCompressor",
      "Apply an GSM compressor to emulate the sound of a GSM (\"2G\") cellular "
      "phone connection. This plugin internally resamples the input audio to "
      "8kHz.")
      .def(py::init([]() { return new GSMCompressor(); }))
      .def("__repr__", [](const GSMCompressor &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.GSMCompressor";
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

}; // namespace Pedalboard