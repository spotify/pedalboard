/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include "../JuceHeader.h"

namespace juce {

/**
    An interpolator base class for resampling streams of floats.

    Note that the resamplers are stateful, so when there's a break in the
   continuity of the input stream you're feeding it, you should call reset()
   before feeding it any new data. And like with any other stateful filter, if
   you're resampling multiple channels, make sure each one uses its own
   interpolator object.

    @see LagrangeInterpolator, CatmullRomInterpolator, WindowedSincInterpolator,
         LinearInterpolator, ZeroOrderHoldInterpolator

    @tags{Audio}
*/
template <class InterpolatorTraits, int memorySize>
class JUCE_API SIMDGenericInterpolator {
public:
  SIMDGenericInterpolator() noexcept { reset(); }

  SIMDGenericInterpolator(SIMDGenericInterpolator &&) noexcept = default;
  SIMDGenericInterpolator &
  operator=(SIMDGenericInterpolator &&) noexcept = default;

  /** Returns the latency of the interpolation algorithm in isolation.

      In the context of resampling the total latency of a process using
      the interpolator is the base latency divided by the speed ratio.
  */
  static constexpr float getBaseLatency() noexcept {
    return InterpolatorTraits::algorithmicLatency;
  }

  /** Resets the state of the interpolator.

      Call this when there's a break in the continuity of the input data stream.
  */
  void reset() noexcept {
    indexBuffer = 0;
    subSamplePos = 1.0;
    std::fill(std::begin(lastInputSamples), std::end(lastInputSamples), 0.0f);
  }

  /** Resamples a stream of samples.

      @param speedRatio                   the number of input samples to use for
     each output sample
      @param inputSamples                 the source data to read from. This
     must contain at least (speedRatio * numOutputSamplesToProduce) samples.
      @param outputSamples                the buffer to write the results into
      @param numOutputSamplesToProduce    the number of output samples that
     should be created

      @returns the actual number of input samples that were used
  */
  int process(double speedRatio, const float *inputSamples,
              float *outputSamples, int numOutputSamplesToProduce) noexcept {
    return interpolate(speedRatio, inputSamples, outputSamples,
                       numOutputSamplesToProduce);
  }

private:
  //==============================================================================
  forcedinline void pushInterpolationSample(float newValue) noexcept {
    lastInputSamples[indexBuffer] = newValue;

    if (++indexBuffer == memorySize)
      indexBuffer = 0;
  }

  //==============================================================================
  int interpolate(double speedRatio, const float *input, float *output,
                  int numOutputSamplesToProduce) noexcept {
    auto pos = subSamplePos;
    int numUsed = 0;

    int numInputSamplesToConsume = numOutputSamplesToProduce / speedRatio;

    // To speed this up, we do a multi-step process to allow more vectorization
    // and cache locality:
    //  - Create a temporary stack array of 2x memorySize
    //  - Copy `memorySize` number of samples from `lastInputSamples`
    //    into the left half of the stack array (right-aligned)
    //    (note that lastInputSamples is a ring buffer, so we need to copy both
    //    before and after indexBuffer)
    //  - Copy min(memorySize, numInputSamplesToConsume) from the input
    //    into the right half of the stack array (left-aligned)
    //  - Run interpolation using the stack array as input
    //  - If we hit the right end of the stack array, then we know we're safe
    //    to continue processing the rest of the input buffer directly;
    //    so switch to a loop that iterates through `input` until we hit
    //    `numOutputSamplesToProduce`
    //  - Copy the last `memorySize` samples from `input` into
    //  `lastInputSamples`
    //  - Set indexBuffer to `memorySize`

    float workingMemory[2 * memorySize];
    std::memset(workingMemory, 0, 2 * memorySize * sizeof(float));
    std::memcpy(workingMemory + memorySize - indexBuffer, lastInputSamples,
                indexBuffer * sizeof(float));
    std::memcpy(workingMemory, lastInputSamples + indexBuffer,
                (memorySize - indexBuffer) * sizeof(float));
    int numSamplesToCopy = std::min(memorySize, numInputSamplesToConsume);

    // Copy the input into place:
    std::memcpy(workingMemory + memorySize, input,
                numSamplesToCopy * sizeof(float));

    bool usingWorkingMemory = true;
    int indexInBuffer = memorySize - indexBuffer;

    while (numOutputSamplesToProduce > 0) {
      while (pos >= 1.0) {
        indexInBuffer++;
        numUsed++;
        pos -= 1.0;

        if (indexInBuffer == memorySize * 2) {
          // Switch to using the input buffer directly:
          usingWorkingMemory = false;
          indexInBuffer -= memorySize;
        }
      }

      if (!usingWorkingMemory)
        break;

      const float *buffer = workingMemory;

      // All this loop unrolling looks kinda weird, but allows Clang to generate
      // optimized vector instructions in the cases where the resampling factor
      // is <=1/8, <=1/4, or <=1/2, which speeds things up by 15-20%.
      float pos0 = pos;
      float pos1 = pos0 + speedRatio;
      float pos2 = pos0 + speedRatio * 2;
      float pos3 = pos0 + speedRatio * 3;
      float pos4 = pos0 + speedRatio * 4;
      float pos5 = pos0 + speedRatio * 5;
      float pos6 = pos0 + speedRatio * 6;
      float pos7 = pos0 + speedRatio * 7;
      float pos8 = pos0 + speedRatio * 8;
      float pos9 = pos0 + speedRatio * 9;
      float pos10 = pos0 + speedRatio * 10;
      float pos11 = pos0 + speedRatio * 11;
      float pos12 = pos0 + speedRatio * 12;
      float pos13 = pos0 + speedRatio * 13;
      float pos14 = pos0 + speedRatio * 14;
      float pos15 = pos0 + speedRatio * 15;

      if (numOutputSamplesToProduce >= 16) {
        std::array<float, 16> positions = {
            pos0, pos1, pos2,  pos3,  pos4,  pos5,  pos6,  pos7,
            pos8, pos9, pos10, pos11, pos12, pos13, pos14, pos15};
        std::array<float, 16> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        output[4] = outputs[4];
        output[5] = outputs[5];
        output[6] = outputs[6];
        output[7] = outputs[7];
        output[8] = outputs[8];
        output[9] = outputs[9];
        output[10] = outputs[10];
        output[11] = outputs[11];
        output[12] = outputs[12];
        output[13] = outputs[13];
        output[14] = outputs[14];
        output[15] = outputs[15];
        numOutputSamplesToProduce -= 16;
        pos += speedRatio * 16;
        output += 16;
      } else if (numOutputSamplesToProduce >= 8) {
        std::array<float, 8> positions = {pos0, pos1, pos2, pos3,
                                          pos4, pos5, pos6, pos7};
        std::array<float, 8> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        output[4] = outputs[4];
        output[5] = outputs[5];
        output[6] = outputs[6];
        output[7] = outputs[7];
        numOutputSamplesToProduce -= 8;
        pos += speedRatio * 8;
        output += 8;
      } else if (numOutputSamplesToProduce >= 4) {
        const std::array<float, 4> positions = {pos0, pos1, pos2, pos3};
        std::array<float, 4> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        numOutputSamplesToProduce -= 4;
        pos += speedRatio * 4;
        output += 4;
      } else if (numOutputSamplesToProduce >= 2) {
        const std::array<float, 2> positions = {pos0, pos1};
        std::array<float, 2> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        numOutputSamplesToProduce -= 2;
        pos += speedRatio * 2;
        output += 2;
      } else {
        *output++ = InterpolatorTraits::valueAtOffset(buffer, (float)pos,
                                                      indexInBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      }
    }

    while (numOutputSamplesToProduce > 0) {
      while (pos >= 1.0) {
        indexInBuffer++;
        numUsed++;
        pos -= 1.0;
      }

      const float *buffer = (float *)input;

      // All this loop unrolling looks kinda weird, but allows Clang to generate
      // optimized vector instructions in the cases where the resampling factor
      // is <=1/8, <=1/4, or <=1/2, which speeds things up by 15-20%.
      float pos0 = pos;
      float pos1 = pos0 + speedRatio;
      float pos2 = pos0 + speedRatio * 2;
      float pos3 = pos0 + speedRatio * 3;
      float pos4 = pos0 + speedRatio * 4;
      float pos5 = pos0 + speedRatio * 5;
      float pos6 = pos0 + speedRatio * 6;
      float pos7 = pos0 + speedRatio * 7;
      float pos8 = pos0 + speedRatio * 8;
      float pos9 = pos0 + speedRatio * 9;
      float pos10 = pos0 + speedRatio * 10;
      float pos11 = pos0 + speedRatio * 11;
      float pos12 = pos0 + speedRatio * 12;
      float pos13 = pos0 + speedRatio * 13;
      float pos14 = pos0 + speedRatio * 14;
      float pos15 = pos0 + speedRatio * 15;

      if (numOutputSamplesToProduce >= 16) {
        std::array<float, 16> positions = {
            pos0, pos1, pos2,  pos3,  pos4,  pos5,  pos6,  pos7,
            pos8, pos9, pos10, pos11, pos12, pos13, pos14, pos15};
        std::array<float, 16> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        output[4] = outputs[4];
        output[5] = outputs[5];
        output[6] = outputs[6];
        output[7] = outputs[7];
        output[8] = outputs[8];
        output[9] = outputs[9];
        output[10] = outputs[10];
        output[11] = outputs[11];
        output[12] = outputs[12];
        output[13] = outputs[13];
        output[14] = outputs[14];
        output[15] = outputs[15];
        numOutputSamplesToProduce -= 16;
        pos += speedRatio * 16;
        output += 16;
      } else if (numOutputSamplesToProduce >= 8) {
        std::array<float, 8> positions = {pos0, pos1, pos2, pos3,
                                          pos4, pos5, pos6, pos7};
        std::array<float, 8> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        output[4] = outputs[4];
        output[5] = outputs[5];
        output[6] = outputs[6];
        output[7] = outputs[7];
        numOutputSamplesToProduce -= 8;
        pos += speedRatio * 8;
        output += 8;
      } else if (numOutputSamplesToProduce >= 4) {
        const std::array<float, 4> positions = {pos0, pos1, pos2, pos3};
        std::array<float, 4> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        numOutputSamplesToProduce -= 4;
        pos += speedRatio * 4;
        output += 4;
      } else if (numOutputSamplesToProduce >= 2) {
        const std::array<float, 2> positions = {pos0, pos1};
        std::array<float, 2> outputs = InterpolatorTraits::valuesAtOffsets(
            buffer, positions, indexInBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        numOutputSamplesToProduce -= 2;
        pos += speedRatio * 2;
        output += 2;
      } else {
        *output++ = InterpolatorTraits::valueAtOffset(buffer, (float)pos,
                                                      indexInBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      }
    }

    subSamplePos = pos;
    if (!usingWorkingMemory) {
      std::memcpy(lastInputSamples, input + numUsed - memorySize,
                  memorySize * sizeof(float));
      indexBuffer = 0;
    }
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];
  double subSamplePos = 1.0;
  int indexBuffer = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SIMDGenericInterpolator)
};

/**
    An interpolator base class for resampling streams of floats.

    Note that the resamplers are stateful, so when there's a break in the
   continuity of the input stream you're feeding it, you should call reset()
   before feeding it any new data. And like with any other stateful filter, if
   you're resampling multiple channels, make sure each one uses its own
   interpolator object.

    @see LagrangeInterpolator, CatmullRomInterpolator, WindowedSincInterpolator,
         LinearInterpolator, ZeroOrderHoldInterpolator

    @tags{Audio}
*/
template <class InterpolatorTraits, typename ratio, int memorySize>
class JUCE_API SIMDFixedRatioInterpolator {
public:
  SIMDFixedRatioInterpolator() noexcept { reset(); }

  SIMDFixedRatioInterpolator(SIMDFixedRatioInterpolator &&) noexcept = default;
  SIMDFixedRatioInterpolator &
  operator=(SIMDFixedRatioInterpolator &&) noexcept = default;

  /** Returns the latency of the interpolation algorithm in isolation.

      In the context of resampling the total latency of a process using
      the interpolator is the base latency divided by the speed ratio.
  */
  static constexpr float getBaseLatency() noexcept {
    return InterpolatorTraits::algorithmicLatency;
  }

  /** Resets the state of the interpolator.

      Call this when there's a break in the continuity of the input data stream.
  */
  void reset() noexcept {
    indexBuffer = 0;
    subsamplePosNumerator = ratio::den;
    std::fill(std::begin(lastInputSamples), std::end(lastInputSamples), 0.0f);
  }

  /** Resamples a stream of samples.

      @param speedRatio                   the number of input samples to use for
     each output sample
      @param inputSamples                 the source data to read from. This
     must contain at least (speedRatio * numOutputSamplesToProduce) samples.
      @param outputSamples                the buffer to write the results into
      @param numOutputSamplesToProduce    the number of output samples that
     should be created

      @returns the actual number of input samples that were used
  */
  int process(double speedRatio, const float *inputSamples,
              float *outputSamples, int numOutputSamplesToProduce) noexcept {
    // This should never happen:
    if (std::abs(speedRatio - (((float)ratio::num) / ((float)ratio::den))) >
        0.00001)
      throw new std::runtime_error(
          "Fixed ratio resampler used with the wrong speed ratio!");

    return interpolate(inputSamples, outputSamples, numOutputSamplesToProduce);
  }

private:
  //==============================================================================
  forcedinline void pushInterpolationSample(float newValue) noexcept {
    lastInputSamples[indexBuffer] = newValue;

    if (++indexBuffer == memorySize)
      indexBuffer = 0;
  }

  int interpolate(const float *input, float *output,
                  int numOutputSamplesToProduce) noexcept {
    auto pos = subsamplePosNumerator;
    int numUsed = 0;

    while (numOutputSamplesToProduce > 0) {
      while (pos >= ratio::den) {
        pushInterpolationSample(input[numUsed++]);
        pos -= ratio::den;
      }

      // All this loop unrolling looks kinda weird, but allows Clang to generate
      // optimized vector instructions in the cases where the resampling factor
      // is <=1/8, <=1/4, or <=1/2, which speeds things up by 15-20%.

      auto pos0 = pos;
      auto pos1 = pos0 + ratio::num;
      auto pos2 = pos0 + ratio::num * 2;
      auto pos3 = pos0 + ratio::num * 3;
      auto pos4 = pos0 + ratio::num * 4;
      auto pos5 = pos0 + ratio::num * 5;
      auto pos6 = pos0 + ratio::num * 6;
      auto pos7 = pos0 + ratio::num * 7;

      if (numOutputSamplesToProduce >= 8 && pos7 < ratio::den) {
        std::array<unsigned long, 8> positions = {pos0, pos1, pos2, pos3,
                                                  pos4, pos5, pos6, pos7};
        std::array<float, 8> outputs = InterpolatorTraits::valuesAtOffsets(
            lastInputSamples, positions, indexBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        output[4] = outputs[4];
        output[5] = outputs[5];
        output[6] = outputs[6];
        output[7] = outputs[7];
        numOutputSamplesToProduce -= 8;
        pos += ratio::den * 8;
        output += 8;
      } else if (numOutputSamplesToProduce >= 4 && pos3 < ratio::den) {
        const std::array<unsigned long, 4> positions = {pos0, pos1, pos2, pos3};
        std::array<float, 4> outputs = InterpolatorTraits::valuesAtOffsets(
            lastInputSamples, positions, indexBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        output[2] = outputs[2];
        output[3] = outputs[3];
        numOutputSamplesToProduce -= 4;
        pos += ratio::den * 4;
        output += 4;
      } else if (numOutputSamplesToProduce >= 2 && pos1 < ratio::den) {
        const std::array<unsigned long, 2> positions = {pos0, pos1};
        std::array<float, 2> outputs = InterpolatorTraits::valuesAtOffsets(
            lastInputSamples, positions, indexBuffer);
        output[0] = outputs[0];
        output[1] = outputs[1];
        numOutputSamplesToProduce -= 2;
        pos += ratio::den * 2;
        output += 2;
      } else {
        const std::array<unsigned long, 1> positions = {pos0};
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      positions, indexBuffer);
        pos += ratio::den;
        --numOutputSamplesToProduce;
      }
    }

    subsamplePosNumerator = pos;
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];
  unsigned long subsamplePosNumerator = ratio::den;
  int indexBuffer = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SIMDFixedRatioInterpolator)
};

/**
    A collection of different interpolators for resampling streams of floats.

    @see GenericInterpolator, WindowedSincInterpolator, LagrangeInterpolator,
         CatmullRomInterpolator, LinearInterpolator, ZeroOrderHoldInterpolator

    @tags{Audio}
*/
class SIMDInterpolators {
private:
  struct WindowedSincTraits {
    static constexpr float algorithmicLatency = 100.0f;

    static forcedinline float windowedSinc(float firstFrac,
                                           int index) noexcept {
      auto index2 = index + 1;
      auto frac = firstFrac;

      auto value1 = lookupTable[index];
      auto value2 = lookupTable[index2];

      return value1 + (frac * (value2 - value1));
    }

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int indexBuffer) noexcept {
      const int numCrossings = 100;
      const float floatCrossings = (float)numCrossings;
      float result = 0.0f;

      auto samplePosition = indexBuffer;
      float firstFrac = 0.0f;
      float lastSincPosition = -1.0f;
      int index = 0, sign = -1;

      for (int i = -numCrossings; i <= numCrossings; ++i) {
        auto sincPosition = (1.0f - offset) + (float)i;

        if (i == -numCrossings || (sincPosition >= 0 && lastSincPosition < 0)) {
          auto indexFloat =
              (sincPosition >= 0.f ? sincPosition : -sincPosition) * 100.0f;
          auto indexFloored = std::floor(indexFloat);
          index = (int)indexFloored;
          firstFrac = indexFloat - indexFloored;
          sign = (sincPosition < 0 ? -1 : 1);
        }

        if (sincPosition == 0.0f)
          result += inputs[samplePosition];
        else if (sincPosition < floatCrossings &&
                 sincPosition > -floatCrossings)
          result += inputs[samplePosition] * windowedSinc(firstFrac, index);

        if (++samplePosition == numCrossings * 2)
          samplePosition = 0;

        lastSincPosition = sincPosition;
        index += 100 * sign;
      }

      return result;
    }

    template <unsigned long K>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs, const std::array<float, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
#pragma clang loop vectorize(enable) interleave(enable)
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, pos[i], index);
      }
      return outputs;
    }

    template <unsigned long K, typename ratio = std::ratio<1, 1>>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs,
                    const std::array<unsigned long, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, (pos[i] / (float)ratio::den), index);
      }
      return outputs;
    }

    static const float lookupTable[10001];
  };

  struct LagrangeTraits {
    static constexpr float algorithmicLatency = 2.0f;

    static float valueAtOffset(const float *, float, int) noexcept;

    template <unsigned long K>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs, const std::array<float, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
#pragma clang loop vectorize(enable) interleave(enable)
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, pos[i], index);
      }
      return outputs;
    }

    template <unsigned long K, typename ratio = std::ratio<1, 1>>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs,
                    const std::array<unsigned long, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, (pos[i] / (float)ratio::den), index);
      }
      return outputs;
    }
  };

  struct CatmullRomTraits {
    //==============================================================================
    static constexpr float algorithmicLatency = 2.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int index) noexcept {
      auto y0 = inputs[index];
      if (++index == 4)
        index = 0;
      auto y1 = inputs[index];
      if (++index == 4)
        index = 0;
      auto y2 = inputs[index];
      if (++index == 4)
        index = 0;
      auto y3 = inputs[index];

      auto halfY0 = 0.5f * y0;
      auto halfY3 = 0.5f * y3;

      return y1 +
             offset *
                 ((0.5f * y2 - halfY0) +
                  (offset *
                   (((y0 + 2.0f * y2) - (halfY3 + 2.5f * y1)) +
                    (offset * ((halfY3 + 1.5f * y1) - (halfY0 + 1.5f * y2))))));
    }

    template <unsigned long K>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs, const std::array<float, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, pos[i], index);
      }
      return outputs;
    }

    template <unsigned long K, typename ratio = std::ratio<1, 1>>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs,
                    const std::array<unsigned long, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, (pos[i] / (float)ratio::den), index);
      }
      return outputs;
    }
  };

  struct LinearTraits {
    static constexpr float algorithmicLatency = 1.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int index) noexcept {
      auto y0 = inputs[index];
      auto y1 = inputs[index == 0 ? 1 : 0];

      return y1 * offset + y0 * (1.0f - offset);
    }

    template <unsigned long K>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs, const std::array<float, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
#pragma clang loop vectorize(enable) interleave(enable)
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, pos[i], index);
      }
      return outputs;
    }

    template <unsigned long K, typename ratio = std::ratio<1, 1>>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs,
                    const std::array<unsigned long, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, (pos[i] / (float)ratio::den), index);
      }
      return outputs;
    }
  };

  struct ZeroOrderHoldTraits {
    static constexpr float algorithmicLatency = 0.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float, int) noexcept {
      return inputs[0];
    }

    template <unsigned long K>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs, const std::array<float, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
#pragma clang loop vectorize(enable) interleave(enable)
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, pos[i], index);
      }
      return outputs;
    }

    template <unsigned long K, typename ratio = std::ratio<1, 1>>
    static forcedinline std::array<float, K>
    valuesAtOffsets(const float *const inputs,
                    const std::array<unsigned long, K> pos,
                    int index) noexcept {
      std::array<float, K> outputs;
      for (unsigned long i = 0; i < K; i++) {
        outputs[i] = valueAtOffset(inputs, (pos[i] / (float)ratio::den), index);
      }
      return outputs;
    }
  };

public:
  using WindowedSinc = SIMDGenericInterpolator<WindowedSincTraits, 200>;
  using Lagrange = SIMDGenericInterpolator<LagrangeTraits, 5>;
  using CatmullRom = SIMDGenericInterpolator<CatmullRomTraits, 4>;
  using Linear = SIMDGenericInterpolator<LinearTraits, 2>;
  using ZeroOrderHold = SIMDGenericInterpolator<ZeroOrderHoldTraits, 1>;
};

} // namespace juce
