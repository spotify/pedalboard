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
#include "../juce_overrides/juce_FastWindowedSincInterpolators.h"
#include "../plugins/AddLatency.h"

namespace Pedalboard {

#include <variant>

/**
 * The various levels of resampler quality available from JUCE.
 * More could be added here, but these should cover the vast
 * majority of use cases.
 */
enum class ResamplingQuality {
  // These types are the legacy JUCE versions, which all
  // introduce aliasing when used for downsampling:
  ZeroOrderHold = 0,
  Linear = 1,
  CatmullRom = 2,
  Lagrange = 3,
  WindowedSinc = 4,
  // These resamplers are faster than the default WindowedSinc
  // counterpart, and all properly handle downsampling without
  // aliasing. They also include significant speedups for the cases
  // in which the speed ratio is a nice fraction like 2/3 or 3/2.
  // The higher the number used as the suffix
  WindowedSinc256 = 5,
  WindowedSinc128 = 6,
  WindowedSinc64 = 7, // This is the new default as of Pedalboard v0.9.14
  WindowedSinc32 = 8,
  WindowedSinc16 = 9,
  WindowedSinc8 = 10,
};

/**
 * A wrapper class that allows changing the quality of a resampler,
 * as the JUCE GenericInterpolator implementations are each separate classes.
 */
class VariableQualityResampler {
public:
  void setQuality(const ResamplingQuality newQuality) {
    switch (newQuality) {
    case ResamplingQuality::ZeroOrderHold:
      interpolator = juce::Interpolators::ZeroOrderHold();
      break;
    case ResamplingQuality::Linear:
      interpolator = juce::Interpolators::Linear();
      break;
    case ResamplingQuality::CatmullRom:
      interpolator = juce::Interpolators::CatmullRom();
      break;
    case ResamplingQuality::Lagrange:
      interpolator = juce::Interpolators::Lagrange();
      break;
    case ResamplingQuality::WindowedSinc:
      interpolator = juce::Interpolators::WindowedSinc();
      break;
    case ResamplingQuality::WindowedSinc256:
      interpolator = juce::FastInterpolators::WindowedSinc256();
      break;
    case ResamplingQuality::WindowedSinc128:
      interpolator = juce::FastInterpolators::WindowedSinc128();
      break;
    case ResamplingQuality::WindowedSinc64:
      interpolator = juce::FastInterpolators::WindowedSinc64();
      break;
    case ResamplingQuality::WindowedSinc32:
      interpolator = juce::FastInterpolators::WindowedSinc32();
      break;
    case ResamplingQuality::WindowedSinc16:
      interpolator = juce::FastInterpolators::WindowedSinc16();
      break;
    case ResamplingQuality::WindowedSinc8:
      interpolator = juce::FastInterpolators::WindowedSinc8();
      break;
    default:
      throw std::domain_error("Unknown resampler quality received!");
    }
  }

  ResamplingQuality getQuality() const {
    return (ResamplingQuality)interpolator.index();
  }

  float getBaseLatency() const {
    // Unfortunately, std::visit cannot be used here due to macOS version
    // issues: https://stackoverflow.com/q/52310835/679081
    if (auto *i =
            std::get_if<juce::Interpolators::ZeroOrderHold>(&interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Linear>(&interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::Interpolators::CatmullRom>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Lagrange>(&interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::Interpolators::WindowedSinc>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc256>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc128>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc64>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc32>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc16>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc8>(
                   &interpolator)) {
      return i->getBaseLatency();
    } else {
      throw std::runtime_error("Unknown resampler quality!");
    }
  }

  void reset() noexcept {
    // Unfortunately, std::visit cannot be used here due to macOS version
    // issues: https://stackoverflow.com/q/52310835/679081
    if (auto *i =
            std::get_if<juce::Interpolators::ZeroOrderHold>(&interpolator)) {
      i->reset();
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Linear>(&interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::Interpolators::CatmullRom>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Lagrange>(&interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::Interpolators::WindowedSinc>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc256>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc128>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc64>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc32>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc16>(
                   &interpolator)) {
      i->reset();
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc8>(
                   &interpolator)) {
      i->reset();
    } else {
      throw std::runtime_error("Unknown resampler quality!");
    }
  }

  int process(double speedRatio, const float *inputSamples,
              float *outputSamples, int numOutputSamplesToProduce) noexcept {
    // Unfortunately, std::visit cannot be used here due to macOS version
    // issues: https://stackoverflow.com/q/52310835/679081
    if (auto *i =
            std::get_if<juce::Interpolators::ZeroOrderHold>(&interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Linear>(&interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::Interpolators::CatmullRom>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i =
                   std::get_if<juce::Interpolators::Lagrange>(&interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::Interpolators::WindowedSinc>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc256>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc128>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc64>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc32>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc16>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else if (auto *i = std::get_if<juce::FastInterpolators::WindowedSinc8>(
                   &interpolator)) {
      return i->process(speedRatio, inputSamples, outputSamples,
                        numOutputSamplesToProduce);
    } else {
      throw std::runtime_error("Unknown resampler quality!");
    }
  }

private:
  std::variant<juce::Interpolators::ZeroOrderHold, juce::Interpolators::Linear,
               juce::Interpolators::CatmullRom, juce::Interpolators::Lagrange,
               juce::Interpolators::WindowedSinc,
               juce::FastInterpolators::WindowedSinc256,
               juce::FastInterpolators::WindowedSinc128,
               juce::FastInterpolators::WindowedSinc64,
               juce::FastInterpolators::WindowedSinc32,
               juce::FastInterpolators::WindowedSinc16,
               juce::FastInterpolators::WindowedSinc8>
      interpolator;
};

/**
 * A test plugin used to verify the behaviour of the ResamplingPlugin wrapper.
 */
template <typename SampleType> class Passthrough : public Plugin {
public:
  virtual ~Passthrough() {};

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
template <typename T = Passthrough<float>, typename SampleType = float,
          int DefaultSampleRate = 8000>
class Resample : public Plugin {
public:
  virtual ~Resample() {};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (specChanged || nativeToTargetResamplers.empty()) {
      reset();

      nativeToTargetResamplers.resize(spec.numChannels);
      targetToNativeResamplers.resize(spec.numChannels);

      resamplerRatio = spec.sampleRate / targetSampleRate;
      inverseResamplerRatio = targetSampleRate / spec.sampleRate;

      for (int i = 0; i < spec.numChannels; i++) {
        nativeToTargetResamplers[i].setQuality(quality);
        nativeToTargetResamplers[i].reset();
        targetToNativeResamplers[i].setQuality(quality);
        targetToNativeResamplers[i].reset();
      }

      maximumBlockSizeInTargetSampleRate =
          std::ceil(spec.maximumBlockSize / resamplerRatio);

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
                              ((maximumBlockSizeInTargetSampleRate + 1) * 3) +
                                  (inStreamLatency / resamplerRatio));
      outputBuffer.setSize(
          spec.numChannels,
          (int)std::ceil(resampledBuffer.getNumSamples() * resamplerRatio) +
              spec.maximumBlockSize);

      lastSpec = spec;
    }

    juce::dsp::ProcessSpec subSpec;
    subSpec.numChannels = spec.numChannels;
    subSpec.sampleRate = targetSampleRate;
    subSpec.maximumBlockSize = maximumBlockSizeInTargetSampleRate;
    plugin.prepare(subSpec);
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

    unsigned long samplesUsed = 0;
    if (samplesInInputReservoir) {
      // Copy the input samples into the input reservoir and use that as the
      // resampler's input:
      expectedResampledSamples +=
          (float)samplesInInputReservoir / resamplerRatio;

      for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
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

        juce::dsp::AudioBlock<SampleType> inputReservoirBlock(inputReservoir);
        inputReservoirBlock.move(samplesUsed, 0, unusedInputSampleCount);
        samplesInInputReservoir = unusedInputSampleCount;
      } else {
        samplesInInputReservoir = 0;
      }
    } else {
      for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
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
        for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
          inputReservoir.copyFrom(c, 0,
                                  ioBlock.getChannelPointer(c) + samplesUsed,
                                  unusedInputSampleCount);
        }
        samplesInInputReservoir = unusedInputSampleCount;
      }
    }

    cleanSamplesInResampledBuffer += (int)expectedResampledSamples;

    // Pass resampledBuffer to the plugin, in chunks:
    juce::dsp::AudioBlock<SampleType> resampledBlock(resampledBuffer);

    // Only pass in the maximumBlockSize (in target sample rate) that the
    // sub-plugin expects:
    while (cleanSamplesInResampledBuffer > 0) {
      int cleanSamplesToProcess =
          std::min((int)maximumBlockSizeInTargetSampleRate,
                   cleanSamplesInResampledBuffer);

      juce::dsp::AudioBlock<SampleType> subBlock = resampledBlock.getSubBlock(
          processedSamplesInResampledBuffer, cleanSamplesToProcess);
      juce::dsp::ProcessContextReplacing<SampleType> subContext(subBlock);

      int resampledSamplesOutput = plugin.process(subContext);

      if (resampledSamplesOutput < cleanSamplesToProcess) {
        // Move all remaining samples to the left of the buffer:
        int offset = cleanSamplesToProcess - resampledSamplesOutput;

        for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
          // Move the contents of the resampled block to the left:
          std::memmove(
              (char *)resampledBuffer.getWritePointer(c) +
                  processedSamplesInResampledBuffer,
              (char *)(resampledBuffer.getWritePointer(c) +
                       processedSamplesInResampledBuffer + offset),
              (resampledSamplesOutput + cleanSamplesInResampledBuffer) *
                  sizeof(SampleType));
        }
      }
      processedSamplesInResampledBuffer += resampledSamplesOutput;
      cleanSamplesInResampledBuffer -= cleanSamplesToProcess;
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

    for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
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
      for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
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
        std::min((int)ioBlock.getNumSamples(), (int)samplesInOutputBuffer);
    ioBlock.copyFrom(outputBuffer, 0, ioBlock.getNumSamples() - samplesToOutput,
                     samplesToOutput);

    int samplesRemainingInOutputBuffer =
        samplesInOutputBuffer - samplesToOutput;
    if (samplesRemainingInOutputBuffer > 0) {
      for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
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

  SampleType getTargetSampleRate() const { return targetSampleRate; }
  void setTargetSampleRate(const SampleType value) {
    if (value <= 0.0) {
      throw std::range_error("Target sample rate must be greater than 0Hz.");
    }
    targetSampleRate = value;
  };

  ResamplingQuality getQuality() const { return quality; }
  void setQuality(const ResamplingQuality value) {
    quality = value;
    reset();
  };

  T &getNestedPlugin() { return plugin; }

  virtual void reset() override final {
    plugin.reset();

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
    maximumBlockSizeInTargetSampleRate = 0;
  }

  virtual int getLatencyHint() override {
    return inStreamLatency + (plugin.getLatencyHint() * resamplerRatio);
  }

private:
  T plugin;
  float targetSampleRate = (float)DefaultSampleRate;
  ResamplingQuality quality = ResamplingQuality::WindowedSinc;

  double resamplerRatio = 1.0;
  double inverseResamplerRatio = 1.0;

  juce::AudioBuffer<SampleType> inputReservoir;
  int samplesInInputReservoir = 0;

  std::vector<VariableQualityResampler> nativeToTargetResamplers;
  juce::AudioBuffer<SampleType> resampledBuffer;
  int cleanSamplesInResampledBuffer = 0;
  int processedSamplesInResampledBuffer = 0;
  std::vector<VariableQualityResampler> targetToNativeResamplers;

  juce::AudioBuffer<SampleType> outputBuffer;
  int samplesInOutputBuffer = 0;

  int samplesProduced = 0;
  int inStreamLatency = 0;
  unsigned int maximumBlockSizeInTargetSampleRate = 0;

  int spaceAvailableInResampledBuffer() const {
    return resampledBuffer.getNumSamples() -
           std::max(cleanSamplesInResampledBuffer,
                    processedSamplesInResampledBuffer);
  }

  int spaceAvailableInOutputBuffer() const {
    return outputBuffer.getNumSamples() - samplesInOutputBuffer;
  }
};

inline void init_resample(py::module &m) {
  py::class_<Resample<Passthrough<float>, float>, Plugin,
             std::shared_ptr<Resample<Passthrough<float>, float>>>
      resample(
          m, "Resample",
          "A plugin that downsamples the input audio to the given sample rate, "
          "then upsamples it back to the original sample rate. Various quality "
          "settings will produce audible distortion and aliasing effects.");

  py::enum_<ResamplingQuality>(
      resample, "Quality", "Indicates a specific resampling algorithm to use.")
      .value("ZeroOrderHold", ResamplingQuality::ZeroOrderHold,
             "The lowest quality and fastest resampling method, with lots of "
             "audible artifacts.\n\nZero-order hold resampling chooses the "
             "next value to use based on the last value, without any "
             "interpolation. Think of it like nearest-neighbor resampling.")
      .value(
          "Linear", ResamplingQuality::Linear,
          "A resampling method slightly less noisy than the simplest "
          "method.\n\nLinear resampling takes the average of the two nearest "
          "values to the desired sample, which is reasonably good for "
          "downsampling.")
      .value("CatmullRom", ResamplingQuality::CatmullRom,
             "A moderately good-sounding resampling method which is fast to "
             "run. Slightly slower than Linear resampling, but slightly higher "
             "quality.")
      .value("Lagrange", ResamplingQuality::Lagrange,
             "A moderately good-sounding resampling method which is slow to "
             "run. Slower than CatmullRom resampling, but slightly higher "
             "quality.")
      .value("WindowedSinc", ResamplingQuality::WindowedSinc,
             "A very high quality (and the slowest) resampling method, with no "
             "audible artifacts.\n\nThis resampler applies a windowed sinc "
             "filter design with 100 zero-crossings of the sinc function to "
             "approimate an ideal brick-wall low-pass filter.")
      .value("WindowedSinc256", ResamplingQuality::WindowedSinc256,
             "Higher quality than WindowedSinc, with no audible "
             "artifacts.\n\nThis resampler applies a windowed sinc filter "
             "with 256 zero-crossings to approimate an ideal brick-wall "
             "low-pass filter. This filter is faster than WindowedSinc due to "
             "an optimized implementation and has a steeper frequency response "
             "(i.e.: fewer artifacts).")
      .value("WindowedSinc128", ResamplingQuality::WindowedSinc128,
             "Higher quality than WindowedSinc, with no audible "
             "artifacts.\n\nThis resampler applies a windowed sinc filter "
             "with 128 zero-crossings to approimate an ideal brick-wall "
             "low-pass filter. This filter is faster than WindowedSinc due to "
             "an optimized implementation and has a steeper frequency response "
             "(i.e.: fewer artifacts).")
      .value(
          "WindowedSinc64", ResamplingQuality::WindowedSinc64,
          "Roughly equal quality to WindowedSinc, but faster, with no audible "
          "artifacts.\n\nThis resampler applies a windowed sinc filter "
          "with 64 zero-crossings to approimate an ideal brick-wall "
          "low-pass filter. This filter is about 2-3x faster than WindowedSinc "
          "due to an optimized implementation and has a nearly identical "
          "frequency"
          "response (i.e.: fewer artifacts).")
      .value("WindowedSinc32", ResamplingQuality::WindowedSinc32,
             "A faster version of WindowedSinc with slightly more artifacts.")
      .value("WindowedSinc16", ResamplingQuality::WindowedSinc16,
             "An even faster version of WindowedSinc with many more artifacts.")
      .value("WindowedSinc8", ResamplingQuality::WindowedSinc8,
             "A very fast version of WindowedSinc with many artifacts.")
      .export_values();

  resample
      .def(py::init([](float targetSampleRate, ResamplingQuality quality) {
             auto resample =
                 std::make_unique<Resample<Passthrough<float>, float>>();
             resample->setTargetSampleRate(targetSampleRate);
             resample->setQuality(quality);
             return resample;
           }),
           py::arg("target_sample_rate") = 8000.0,
           py::arg("quality") = ResamplingQuality::WindowedSinc)
      .def("__repr__",
           [](const Resample<Passthrough<float>, float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Resample";
             ss << " target_sample_rate=" << plugin.getTargetSampleRate();
             ss << " quality=";
             switch (plugin.getQuality()) {
             case ResamplingQuality::ZeroOrderHold:
               ss << "ZeroOrderHold";
               break;
             case ResamplingQuality::Linear:
               ss << "Linear";
               break;
             case ResamplingQuality::CatmullRom:
               ss << "CatmullRom";
               break;
             case ResamplingQuality::Lagrange:
               ss << "Lagrange";
               break;
             case ResamplingQuality::WindowedSinc:
               ss << "WindowedSinc";
               break;
             case ResamplingQuality::WindowedSinc256:
               ss << "WindowedSinc256";
               break;
             case ResamplingQuality::WindowedSinc128:
               ss << "WindowedSinc128";
               break;
             case ResamplingQuality::WindowedSinc64:
               ss << "WindowedSinc64";
               break;
             case ResamplingQuality::WindowedSinc32:
               ss << "WindowedSinc32";
               break;
             case ResamplingQuality::WindowedSinc16:
               ss << "WindowedSinc16";
               break;
             case ResamplingQuality::WindowedSinc8:
               ss << "WindowedSinc8";
               break;
             default:
               ss << "unknown";
               break;
             }
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property(
          "target_sample_rate",
          &Resample<Passthrough<float>, float>::getTargetSampleRate,
          &Resample<Passthrough<float>, float>::setTargetSampleRate,
          "The sample rate to resample the input audio to. This value may be a "
          "floating-point number, in which case a floating-point sampling rate "
          "will be used. Note that the output of this plugin will still be at "
          "the original sample rate; this is merely the sample rate used for "
          "quality reduction.")
      .def_property("quality", &Resample<Passthrough<float>, float>::getQuality,
                    &Resample<Passthrough<float>, float>::setQuality,
                    "The resampling algorithm used to resample the audio.");
}

/**
 * An internal test plugin that does nothing but add latency to the resampled
 * signal.
 */
inline void init_resample_with_latency(py::module &m) {
  py::class_<Resample<AddLatency, float>, Plugin,
             std::shared_ptr<Resample<AddLatency, float>>>(
      m, "ResampleWithLatency")
      .def(py::init([](float targetSampleRate, int internalLatency,
                       ResamplingQuality quality) {
             auto plugin = std::make_unique<Resample<AddLatency, float>>();
             plugin->setTargetSampleRate(targetSampleRate);
             plugin->getNestedPlugin().getDSP().setMaximumDelayInSamples(
                 internalLatency);
             plugin->getNestedPlugin().getDSP().setDelay(internalLatency);
             plugin->setQuality(quality);
             return plugin;
           }),
           py::arg("target_sample_rate") = 8000.0,
           py::arg("internal_latency") = 1024,
           py::arg("quality") = ResamplingQuality::WindowedSinc)
      .def("__repr__",
           [](Resample<AddLatency, float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.ResampleWithLatency";
             ss << " target_sample_rate=" << plugin.getTargetSampleRate();
             ss << " internal_latency="
                << plugin.getNestedPlugin().getDSP().getDelay();
             ss << " quality=";
             switch (plugin.getQuality()) {
             case ResamplingQuality::ZeroOrderHold:
               ss << "ZeroOrderHold";
               break;
             case ResamplingQuality::Linear:
               ss << "Linear";
               break;
             case ResamplingQuality::CatmullRom:
               ss << "CatmullRom";
               break;
             case ResamplingQuality::Lagrange:
               ss << "Lagrange";
               break;
             case ResamplingQuality::WindowedSinc:
               ss << "WindowedSinc";
               break;
             case ResamplingQuality::WindowedSinc256:
               ss << "WindowedSinc256";
               break;
             case ResamplingQuality::WindowedSinc128:
               ss << "WindowedSinc128";
               break;
             case ResamplingQuality::WindowedSinc64:
               ss << "WindowedSinc64";
               break;
             case ResamplingQuality::WindowedSinc32:
               ss << "WindowedSinc32";
               break;
             case ResamplingQuality::WindowedSinc16:
               ss << "WindowedSinc16";
               break;
             case ResamplingQuality::WindowedSinc8:
               ss << "WindowedSinc8";
               break;
             default:
               ss << "unknown";
               break;
             }
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("target_sample_rate",
                    &Resample<AddLatency, float>::getTargetSampleRate,
                    &Resample<AddLatency, float>::setTargetSampleRate)
      .def_property("quality", &Resample<AddLatency, float>::getQuality,
                    &Resample<AddLatency, float>::setQuality);
}

} // namespace Pedalboard