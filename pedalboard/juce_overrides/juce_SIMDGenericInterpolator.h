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

#define debugprintf printf
// #define debugprintf                                                            \
//   if (false)                                                                   \
//   printf

namespace juce {

template <typename trait, unsigned long K>
static forcedinline void
computeValuesAtOffsets(const float *const __restrict inputs,
                       const float startPos, const float posOffset, int index,
                       float *const __restrict outputs) noexcept {
  std::array<float, K> results;
  std::array<float, K> positions;
  std::array<float, K> indices;

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    positions[i] =
        (startPos + (i * posOffset)) - (int)((startPos + (i * posOffset)));
    indices[i] = index + static_cast<int>(startPos + (i * posOffset));
  }

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    results[i] = trait::valueAtOffset(inputs, positions[i], indices[i]);
  }

  std::memcpy(outputs, results.data(), K * sizeof(float));
}

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
  int interpolate(double speedRatio, const float *input, float *output,
                  int numOutputSamplesToProduce) noexcept {
    debugprintf("\n\n[new       ]\e[0;32m Outputting %d samples with speed "
                "ratio %f and subSamplePos %f\e[0m\n",
                numOutputSamplesToProduce, speedRatio, subSamplePos);
    auto pos = subSamplePos;
    int numUsed = 0;

    int numInputSamplesToConsume =
        (int)std::ceil((float)numOutputSamplesToProduce * speedRatio);

    // To speed this up, we do a multi-step process to allow more vectorization
    // and cache locality for large buffers:
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
    //
    // Why not always do this in JUCE? Well, Pedalboard's use of this API
    // is a bit different; it usually has a large input buffer and is not
    // being called in real-time.

    debugprintf("[new       ] Starting with lastInputSamples: [");
    for (int i = 0; i < memorySize; i++) {
      if (i == memorySize - 1)
        debugprintf("%f]\n", lastInputSamples[i]);
      else
        debugprintf("%f, ", lastInputSamples[i]);
    }

    float workingMemory[2 * memorySize];
    std::memset(workingMemory, 0, 2 * memorySize * sizeof(float));
    std::memcpy(workingMemory, lastInputSamples, memorySize * sizeof(float));
    debugprintf(
        "[new       ] Copied lastInputSamples into temporary buffer: [");
    for (int i = 0; i < memorySize * 2; i++) {
      if (i == memorySize * 2 - 1)
        debugprintf("%f]\n", workingMemory[i]);
      else
        debugprintf("%f, ", workingMemory[i]);
    }

    int numSamplesToCopyFromInput =
        std::min(memorySize, numInputSamplesToConsume);

    // Copy the input into place:
    std::memcpy(workingMemory + memorySize, input,
                numSamplesToCopyFromInput * sizeof(float));

    debugprintf(
        "[new       ] Copied %d samples (of possible %d) from input into "
        "temporary buffer: [",
        numSamplesToCopyFromInput, numInputSamplesToConsume);
    for (int i = 0; i < memorySize * 2; i++) {
      if (i == memorySize * 2 - 1)
        debugprintf("%f]\n", workingMemory[i]);
      else
        debugprintf("%f, ", workingMemory[i]);
    }

    bool usingWorkingMemory = true;
    int indexInBuffer = memorySize - indexBuffer - 1;

    while (numOutputSamplesToProduce > 0) {
      while (pos >= 1.0) {
        indexInBuffer++;
        pos -= 1.0;

        if (indexInBuffer == memorySize + numSamplesToCopyFromInput) {
          // Switch to using the input buffer directly:
          usingWorkingMemory = false;
          debugprintf("[new       ] indexInBuffer (%d) == %d + %d, so "
                      "switching to using input buffer directly\n",
                      indexInBuffer, memorySize, numSamplesToCopyFromInput);
          indexInBuffer -= memorySize;
          debugprintf("[new       ] indexBuffer=%d, pos=%f, indexInBuffer=%d\n",
                      indexBuffer, pos, indexInBuffer);
          break;
        }

        numUsed++;
      }

      int numInputSamplesLeftInBuffer =
          (memorySize + numSamplesToCopyFromInput) - indexInBuffer;
      int numOutputSamplesToProduceThisIteration = std::min(
          numOutputSamplesToProduce,
          (int)((((float)numInputSamplesLeftInBuffer) - pos) / speedRatio));

      if (!usingWorkingMemory)
        break;

      debugprintf("[new       ] %d samples left in buffer from position %d, "
                  "speed ratio %f, producing %d samples this iteration.\n",
                  numInputSamplesLeftInBuffer, indexInBuffer, speedRatio,
                  numOutputSamplesToProduceThisIteration);

      debugprintf("[new       ] indexBuffer=%d, pos=%f, indexInBuffer=%d\n",
                  indexBuffer, pos, indexInBuffer);

      const float *buffer = workingMemory;
      debugprintf(
          "[new       ] Producing up to %d samples from sub-sample pos %f "
          "at index %d in temporary buffer: [",
          numOutputSamplesToProduceThisIteration, pos, indexInBuffer);
      for (int i = 0; i < memorySize * 2; i++) {
        if (i == memorySize * 2 - 1)
          debugprintf("%f]\n", buffer[i]);
        else
          debugprintf("%f, ", buffer[i]);
      }

      if (numOutputSamplesToProduceThisIteration >= 32) {
        computeValuesAtOffsets<InterpolatorTraits, 32UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 32;
        pos += speedRatio * 32;
        output += 32;
      } else if (numOutputSamplesToProduceThisIteration >= 16) {
        computeValuesAtOffsets<InterpolatorTraits, 16UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 16;
        pos += speedRatio * 16;
        output += 16;
      } else if (numOutputSamplesToProduceThisIteration >= 8) {
        computeValuesAtOffsets<InterpolatorTraits, 8UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
        numOutputSamplesToProduce -= 8;
        pos += speedRatio * 8;
        output += 8;
      } else if (numOutputSamplesToProduceThisIteration >= 4) {
        computeValuesAtOffsets<InterpolatorTraits, 4UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
        numOutputSamplesToProduce -= 4;
        pos += speedRatio * 4;
        output += 4;
      } else if (numOutputSamplesToProduceThisIteration >= 2) {
        computeValuesAtOffsets<InterpolatorTraits, 2UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
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

    debugprintf("[new       ] between loops: indexBuffer=%d, pos=%f, "
                "indexInBuffer=%d, numUsed=%d\n",
                indexBuffer, pos, indexInBuffer, numUsed);

    while (numOutputSamplesToProduce > 0) {
      int increment = static_cast<int>(std::floor(pos));
      indexInBuffer += increment;
      numUsed += increment;
      pos -= static_cast<float>(increment);

      const float *buffer = (float *)input;

      // All this loop unrolling looks kinda weird, but allows Clang to generate
      // optimized vector instructions in the cases where the resampling factor
      // is <=1/8, <=1/4, or <=1/2, which speeds things up by 15-20%.
      debugprintf(
          "[new       ] Producing up to %d samples from sub-sample pos %f "
          "at index %d in input buffer\n",
          numOutputSamplesToProduce, pos, indexInBuffer);
      if (numOutputSamplesToProduce >= 128) {
        computeValuesAtOffsets<InterpolatorTraits, 128UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 128;
        pos += speedRatio * 128;
        output += 128;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 64) {
        computeValuesAtOffsets<InterpolatorTraits, 64UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 64;
        pos += speedRatio * 64;
        output += 64;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 32) {
        computeValuesAtOffsets<InterpolatorTraits, 32UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 32;
        pos += speedRatio * 32;
        output += 32;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 16) {
        computeValuesAtOffsets<InterpolatorTraits, 16UL>(
            buffer, pos, speedRatio, indexInBuffer, output);
        numOutputSamplesToProduce -= 16;
        pos += speedRatio * 16;
        output += 16;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 8) {
        computeValuesAtOffsets<InterpolatorTraits, 8UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
        numOutputSamplesToProduce -= 8;
        pos += speedRatio * 8;
        output += 8;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 4) {
        computeValuesAtOffsets<InterpolatorTraits, 4UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
        numOutputSamplesToProduce -= 4;
        pos += speedRatio * 4;
        output += 4;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else if (numOutputSamplesToProduce >= 2) {
        computeValuesAtOffsets<InterpolatorTraits, 2UL>(buffer, pos, speedRatio,
                                                        indexInBuffer, output);
        numOutputSamplesToProduce -= 2;
        pos += speedRatio * 2;
        output += 2;
        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      } else {
        *output++ = InterpolatorTraits::valueAtOffset(buffer, (float)pos,
                                                      indexInBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;

        indexInBuffer += (int)pos;
        numUsed += (int)pos;
        pos -= (int)pos;
      }
    }

    debugprintf("[new       ] before bookkeeping: indexBuffer=%d, pos=%f, "
                "indexInBuffer=%d, numUsed=%d\n",
                indexBuffer, pos, indexInBuffer, numUsed);
    // int increment = static_cast<int>(std::floor(pos));
    // indexInBuffer += increment;
    // numUsed += static_cast<int>(std::floor(pos));
    // pos -= static_cast<float>(increment);

    // // Reset subsample position for next run:
    pos += 1;
    numUsed -= 1;

    debugprintf("[new       ] after bookkeeping: indexBuffer=%d, pos=%f, "
                "indexInBuffer=%d, numUsed=%d\n",
                indexBuffer, pos, indexInBuffer, numUsed);

    subSamplePos = pos;
    if (!usingWorkingMemory || indexInBuffer >= memorySize) {
      int lastSampleToCopy = numUsed;
      debugprintf("[new       ] Copying input into lastInputSamples from "
                  "lastSampleToCopy=%d "
                  "- memorySize=%d\n",
                  lastSampleToCopy, memorySize);
      std::memcpy(lastInputSamples, input + lastSampleToCopy - memorySize,
                  memorySize * sizeof(float));
      indexBuffer = indexInBuffer - lastSampleToCopy;

      debugprintf("[new       ] Filled lastInputSamples with %d samples: [",
                  memorySize);
      for (int i = 0; i < memorySize; i++) {
        if (i == memorySize - 1)
          debugprintf("%f]\n", lastInputSamples[i]);
        else
          debugprintf("%f, ", lastInputSamples[i]);
      }
    } else {
      indexBuffer = indexInBuffer;
    }
    debugprintf("[new       ] Used %d of the input samples. indexBuffer=%d\n",
                numUsed, indexBuffer);
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];
  double subSamplePos = 1.0;
  int indexBuffer = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SIMDGenericInterpolator)
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

    static const float lookupTable[10001];
  };

  struct LagrangeTraits {
    static constexpr float algorithmicLatency = 2.0f;

    static float valueAtOffset(const float *, float, int) noexcept;
  };

  struct CatmullRomTraits {
    //==============================================================================
    static constexpr float algorithmicLatency = 2.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int index) noexcept {
      auto y0 = inputs[index - 3];
      auto y1 = inputs[index - 2];
      auto y2 = inputs[index - 1];
      auto y3 = inputs[index];

      debugprintf(
          "[new CatRom] Reading from sub-sample pos %f at index %d: y0=%f, "
          "y1=%f, y2=%f, y3=%f\n",
          offset, index, y0, y1, y2, y3);

      auto halfY0 = 0.5f * y0;
      auto halfY3 = 0.5f * y3;

      return y1 +
             offset *
                 ((0.5f * y2 - halfY0) +
                  (offset *
                   (((y0 + 2.0f * y2) - (halfY3 + 2.5f * y1)) +
                    (offset * ((halfY3 + 1.5f * y1) - (halfY0 + 1.5f * y2))))));
    }
  };

  struct LinearTraits {
    static constexpr float algorithmicLatency = 1.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int index) noexcept {
      auto y0 = inputs[index - 1];
      auto y1 = inputs[index];

      auto res = y1 * offset + y0 * (1.0f - offset);
      debugprintf(
          "[new Linear] Reading from sub-sample pos %f at index %d: %f * %f "
          "+ %f * (1.0f - %f) = \e[0;32m%f\e[0;0m\n",
          offset, index, y1, offset, y0, offset, res);
      return res;
    }
  };

  struct ZeroOrderHoldTraits {
    static constexpr float algorithmicLatency = 0.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset,
                                            int index) noexcept {
      return inputs[index];
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
