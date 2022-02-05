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
#include "../Plugin.h"
#include "../plugins/AddLatency.h"
#include <mutex>

namespace Pedalboard {

/**
 * A test plugin used to verify the behaviour of the ResamplingPlugin wrapper.
 */
template <typename SampleType> class Passthrough : public Plugin {
public:
  virtual ~Passthrough(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {}

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    return context.getInputBlock().getNumSamples();
  }

  virtual void reset() {}
};

/**
 * A template class that wraps a Pedalboard plugin and resamples
 * the audio to the provided sample rate. The wrapped plugin receives
 * resampled audio and its sampleRate and maximumBlockSize parameters
 * are adjusted accordingly.
 */
template <typename T = Passthrough<float>, typename SampleType = float>
class Resample : public Plugin {
public:
  virtual ~Resample(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (specChanged) {
      reset();

      nativeToTargetResamplers.resize(spec.numChannels);
      targetToNativeResamplers.resize(spec.numChannels);

      resamplerRatio = spec.sampleRate / targetSampleRate;
      inverseResamplerRatio = targetSampleRate / spec.sampleRate;

      const juce::dsp::ProcessSpec subSpec = {
          .numChannels = spec.numChannels,
          .sampleRate = targetSampleRate,
          .maximumBlockSize = static_cast<unsigned int>(spec.maximumBlockSize *
                                                        resamplerRatio)};
      plugin.prepare(subSpec);

      int maximumBlockSizeInSampleRate = spec.maximumBlockSize / resamplerRatio;

      // Store the remainder of the input: any samples that weren't consumed in
      // one pushSamples() call but would be consumable in the next one.
      inputReservoir.setSize(spec.numChannels,
                             2 * ((int)std::ceil(resamplerRatio) +
                                  (int)std::ceil(inverseResamplerRatio)) +
                                 spec.maximumBlockSize);

      inStreamLatency = 0;

      // Add the resamplers' latencies so the output is properly aligned:
      inStreamLatency += std::round(
          nativeToTargetResamplers[0].getBaseLatency() * resamplerRatio +
          targetToNativeResamplers[0].getBaseLatency());

      resampledBuffer.setSize(spec.numChannels,
                              maximumBlockSizeInSampleRate +
                                  (inStreamLatency / resamplerRatio));
      outputBuffer.setSize(spec.numChannels,
                           spec.maximumBlockSize * 3 + inStreamLatency);

      lastSpec = spec;
    }
  }

  int process(const juce::dsp::ProcessContextReplacing<SampleType> &context)
      override final {
    auto ioBlock = context.getOutputBlock();

    float expectedResampledSamples = ioBlock.getNumSamples() / resamplerRatio;

    if (spaceAvailableInResampledBuffer() < expectedResampledSamples) {
      throw std::runtime_error(
          "More samples were provided than can be buffered! This is an "
          "internal Pedalboard error and should be reported. Buffer had " +
          std::to_string(processedSamplesInResampledBuffer +
                         cleanSamplesInResampledBuffer) +
          "/" + std::to_string(resampledBuffer.getNumSamples()) +
          " samples at target sample rate, but was provided " +
          std::to_string(expectedResampledSamples) + ".");
    }

    int samplesUsed = 0;
    if (samplesInInputReservoir) {
      // Copy the input samples into the input reservoir and use that as the
      // resampler's input:
      expectedResampledSamples +=
          (float)samplesInInputReservoir / resamplerRatio;

      for (int c = 0; c < ioBlock.getNumChannels(); c++) {
        inputReservoir.copyFrom(c, samplesInInputReservoir,
                                ioBlock.getChannelPointer(c),
                                ioBlock.getNumSamples());
        SampleType *resampledBufferPointer =
            resampledBuffer.getWritePointer(c) +
            processedSamplesInResampledBuffer + cleanSamplesInResampledBuffer;
        samplesUsed = nativeToTargetResamplers[c].process(
            resamplerRatio, inputReservoir.getReadPointer(c),
            resampledBufferPointer, expectedResampledSamples);
      }

      if (samplesUsed < ioBlock.getNumSamples() + samplesInInputReservoir) {
        // Take the missing samples and put them at the start of the input
        // reservoir for next time:
        int unusedInputSampleCount =
            (ioBlock.getNumSamples() + samplesInInputReservoir) - samplesUsed;

        for (int c = 0; c < ioBlock.getNumChannels(); c++) {
          inputReservoir.copyFrom(
              c, 0, inputReservoir.getReadPointer(c) + samplesUsed,
              unusedInputSampleCount);
        }

        samplesInInputReservoir = unusedInputSampleCount;
      } else {
        samplesInInputReservoir = 0;
      }
    } else {
      for (int c = 0; c < ioBlock.getNumChannels(); c++) {
        SampleType *resampledBufferPointer =
            resampledBuffer.getWritePointer(c) +
            processedSamplesInResampledBuffer + cleanSamplesInResampledBuffer;
        samplesUsed = nativeToTargetResamplers[c].process(
            resamplerRatio, ioBlock.getChannelPointer(c),
            resampledBufferPointer, (int)expectedResampledSamples);
      }

      if (samplesUsed < ioBlock.getNumSamples()) {
        // Take the missing samples and put them at the start of the input
        // reservoir for next time:
        int unusedInputSampleCount = ioBlock.getNumSamples() - samplesUsed;
        for (int c = 0; c < ioBlock.getNumChannels(); c++) {
          inputReservoir.copyFrom(c, 0,
                                  ioBlock.getChannelPointer(c) + samplesUsed,
                                  unusedInputSampleCount);
        }
        samplesInInputReservoir = unusedInputSampleCount;
      }
    }

    cleanSamplesInResampledBuffer += (int)expectedResampledSamples;

    // Pass resampledBuffer to the plugin:
    juce::dsp::AudioBlock<SampleType> resampledBlock(resampledBuffer);
    juce::dsp::AudioBlock<SampleType> subBlock = resampledBlock.getSubBlock(
        processedSamplesInResampledBuffer, cleanSamplesInResampledBuffer);
    if (cleanSamplesInResampledBuffer) {
      // TODO: Check that samplesInResampledBuffer is not greater than the
      // plugin's maximumBlockSize!
      juce::dsp::ProcessContextReplacing<SampleType> subContext(subBlock);
      int resampledSamplesOutput = plugin.process(subContext);

      if (resampledSamplesOutput < cleanSamplesInResampledBuffer) {
        // Move processed samples to the left of the buffer:
        int offset = cleanSamplesInResampledBuffer - resampledSamplesOutput;

        for (int c = 0; c < ioBlock.getNumChannels(); c++) {
          // Move the contents of the resampled block to the left:
          std::memmove((char *)resampledBuffer.getWritePointer(c) +
                           processedSamplesInResampledBuffer,
                       (char *)(resampledBuffer.getWritePointer(c) +
                                processedSamplesInResampledBuffer + offset),
                       (resampledSamplesOutput) * sizeof(SampleType));
        }
      }
      cleanSamplesInResampledBuffer = 0;
      processedSamplesInResampledBuffer += resampledSamplesOutput;
    }

    // Resample back to the intended sample rate:
    int expectedOutputSamples =
        processedSamplesInResampledBuffer * resamplerRatio;

    int samplesConsumed = 0;

    if (spaceAvailableInOutputBuffer() < expectedOutputSamples) {
      throw std::runtime_error(
          "More samples were provided than can be buffered! This is an "
          "internal Pedalboard error and should be reported. Buffer had " +
          std::to_string(samplesInOutputBuffer) + "/" +
          std::to_string(outputBuffer.getNumSamples()) +
          " samples at native sample rate, but was provided " +
          std::to_string(expectedOutputSamples) + ".");
    }

    for (int c = 0; c < ioBlock.getNumChannels(); c++) {
      float *outputBufferPointer =
          outputBuffer.getWritePointer(c) + samplesInOutputBuffer;
      samplesConsumed = targetToNativeResamplers[c].process(
          inverseResamplerRatio, resampledBuffer.getReadPointer(c),
          outputBufferPointer, expectedOutputSamples);
    }

    samplesInOutputBuffer += expectedOutputSamples;

    int samplesRemainingInResampledBuffer = processedSamplesInResampledBuffer +
                                            cleanSamplesInResampledBuffer -
                                            samplesConsumed;
    if (samplesRemainingInResampledBuffer > 0) {
      for (int c = 0; c < ioBlock.getNumChannels(); c++) {
        // Move the contents of the resampled block to the left:
        std::memmove(
            (char *)resampledBuffer.getWritePointer(c),
            (char *)(resampledBuffer.getWritePointer(c) + samplesConsumed),
            (samplesRemainingInResampledBuffer) * sizeof(SampleType));
      }
    }

    processedSamplesInResampledBuffer -= samplesConsumed;

    // Copy from output buffer to output block:
    int samplesToOutput =
        std::min(ioBlock.getNumSamples(), (unsigned long)samplesInOutputBuffer);
    ioBlock.copyFrom(outputBuffer, 0, ioBlock.getNumSamples() - samplesToOutput,
                     samplesToOutput);

    int samplesRemainingInOutputBuffer =
        samplesInOutputBuffer - samplesToOutput;
    if (samplesRemainingInOutputBuffer > 0) {
      for (int c = 0; c < ioBlock.getNumChannels(); c++) {
        // Move the contents of the resampled block to the left:
        std::memmove(
            (char *)outputBuffer.getWritePointer(c),
            (char *)(outputBuffer.getWritePointer(c) + samplesToOutput),
            (samplesRemainingInOutputBuffer) * sizeof(SampleType));
      }
    }
    samplesInOutputBuffer -= samplesToOutput;

    samplesProduced += samplesToOutput;
    int samplesToReturn = std::min((long)(samplesProduced - inStreamLatency),
                                   (long)samplesToOutput);
    if (samplesToReturn < 0)
      samplesToReturn = 0;

    return samplesToReturn;
  }

  void setTargetSampleRate(float newSampleRate) {
    targetSampleRate = newSampleRate;
  }

  float getTargetSampleRate() const { return targetSampleRate; }

  T &getNestedPlugin() { return plugin; }

  virtual void reset() override final {
    nativeToTargetResamplers.clear();
    targetToNativeResamplers.clear();

    resampledBuffer.clear();
    outputBuffer.clear();
    inputReservoir.clear();

    cleanSamplesInResampledBuffer = 0;
    processedSamplesInResampledBuffer = 0;
    samplesInOutputBuffer = 0;
    samplesInInputReservoir = 0;

    samplesProduced = 0;
    inStreamLatency = 0;
  }

  virtual int getLatencyHint() override { return inStreamLatency; }

private:
  T plugin;
  float targetSampleRate = 44100.0f;

  double resamplerRatio = 1.0;
  double inverseResamplerRatio = 1.0;

  juce::AudioBuffer<SampleType> inputReservoir;
  int samplesInInputReservoir = 0;

  std::vector<juce::Interpolators::WindowedSinc> nativeToTargetResamplers;
  juce::AudioBuffer<SampleType> resampledBuffer;
  int cleanSamplesInResampledBuffer = 0;
  int processedSamplesInResampledBuffer = 0;
  std::vector<juce::Interpolators::WindowedSinc> targetToNativeResamplers;

  juce::AudioBuffer<SampleType> outputBuffer;
  int samplesInOutputBuffer = 0;

  int samplesProduced = 0;
  int inStreamLatency = 0;

  int spaceAvailableInResampledBuffer() const {
    return resampledBuffer.getNumSamples() -
           std::max(cleanSamplesInResampledBuffer,
                    processedSamplesInResampledBuffer);
  }

  int spaceAvailableInOutputBuffer() const {
    return outputBuffer.getNumSamples() - samplesInOutputBuffer;
  }
};

inline void init_resampling_test_plugin(py::module &m) {
  py::class_<Resample<AddLatency, float>, Plugin>(m, "Resample")
      .def(py::init([](float targetSampleRate) {
             auto plugin = std::make_unique<Resample<AddLatency, float>>();
             plugin->setTargetSampleRate(targetSampleRate);

             // Set a delay on this test plugin:
             plugin->getNestedPlugin().getDSP().setMaximumDelayInSamples(1024);
             plugin->getNestedPlugin().getDSP().setDelay(1024);
             return plugin;
           }),
           py::arg("target_sample_rate") = 8000.0)
      .def("__repr__", [](const Resample<AddLatency, float> &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.Resample";
        ss << " target_sample_rate=" << plugin.getTargetSampleRate();
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard