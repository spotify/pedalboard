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

// #define debugprintf printf
#define debugprintf                                                            \
  if (false)                                                                   \
  printf

namespace juce {

template <typename trait, unsigned long K>
static forcedinline double
computeValuesAtOffsets(const float *const __restrict inputs,
                       const double startPos, const double posOffset,
                       float *const __restrict outputs) noexcept {
  std::array<float, K> results;
  std::array<float, K> positions;

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    positions[i] = (startPos + (i * posOffset));
  }

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    outputs[i] = trait::valueAtOffset(inputs, positions[i]);
  }

  // std::memcpy(outputs, results.data(), K * sizeof(float));
  return startPos + K * posOffset;
}

template <typename trait>
static forcedinline double
computeValuesAtOffsets(const float *const __restrict inputs, double startPos,
                       const double posOffset, float *__restrict outputs,
                       int numOutputSamples) noexcept {
  while (numOutputSamples > 128) {
    startPos = computeValuesAtOffsets<trait, 128>(inputs, startPos, posOffset,
                                                  outputs);
    outputs += 128;
    numOutputSamples -= 128;
  }
  while (numOutputSamples >= 64) {
    startPos =
        computeValuesAtOffsets<trait, 64>(inputs, startPos, posOffset, outputs);
    outputs += 64;
    numOutputSamples -= 64;
  }
  while (numOutputSamples >= 32) {
    startPos =
        computeValuesAtOffsets<trait, 32>(inputs, startPos, posOffset, outputs);
    outputs += 32;
    numOutputSamples -= 32;
  }
  while (numOutputSamples >= 32) {
    startPos =
        computeValuesAtOffsets<trait, 32>(inputs, startPos, posOffset, outputs);
    outputs += 32;
    numOutputSamples -= 32;
  }
  while (numOutputSamples >= 16) {
    startPos =
        computeValuesAtOffsets<trait, 16>(inputs, startPos, posOffset, outputs);
    outputs += 16;
    numOutputSamples -= 16;
  }
  while (numOutputSamples >= 8) {
    startPos =
        computeValuesAtOffsets<trait, 8>(inputs, startPos, posOffset, outputs);
    outputs += 8;
    numOutputSamples -= 8;
  }
  while (numOutputSamples >= 4) {
    startPos =
        computeValuesAtOffsets<trait, 4>(inputs, startPos, posOffset, outputs);
    outputs += 4;
    numOutputSamples -= 4;
  }
  while (numOutputSamples >= 4) {
    startPos =
        computeValuesAtOffsets<trait, 4>(inputs, startPos, posOffset, outputs);
    outputs += 4;
    numOutputSamples -= 4;
  }
  while (numOutputSamples >= 2) {
    startPos =
        computeValuesAtOffsets<trait, 2>(inputs, startPos, posOffset, outputs);
    outputs += 2;
    numOutputSamples -= 2;
  }
  if (numOutputSamples >= 1) {
    startPos =
        computeValuesAtOffsets<trait, 1>(inputs, startPos, posOffset, outputs);
    outputs++;
    numOutputSamples--;
  }
  return startPos;
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
    subSamplePos = memorySize;
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
    debugprintf("Generating %d output samples at speedRatio=%f with "
                "lastInputSamples: \t[",
                numOutputSamplesToProduce, speedRatio);
    for (int i = 0; i < memorySize; i++) {
      if (i == memorySize - 1)
        debugprintf("%f]\n", lastInputSamples[i]);
      else
        debugprintf("%f, ", lastInputSamples[i]);
    }
    auto pos = subSamplePos;
    int numUsed = 0;

    int numInputSamples = std::ceil(numOutputSamplesToProduce * speedRatio);

    int inputBufferSize =
        std::max(numInputSamples + memorySize, (int)std::ceil(pos));
    std::vector<float> vec(inputBufferSize);
    float *inputBuf = vec.data();
    std::memcpy(inputBuf, lastInputSamples, memorySize * sizeof(float));
    std::memcpy(inputBuf + memorySize, input, numInputSamples * sizeof(float));

    debugprintf(
        "Producing %d output samples from pos=%f with buffer with size %d "
        "(%d input samples, %d memory size): [",
        numOutputSamplesToProduce, pos, numInputSamples + memorySize,
        numInputSamples, memorySize);
    for (int i = 0; i < numInputSamples + memorySize; i++) {
      if (i == numInputSamples + memorySize - 1)
        debugprintf("%f]\n", inputBuf[i]);
      else
        debugprintf("%f, ", inputBuf[i]);
    }

    computeValuesAtOffsets<InterpolatorTraits>(
        inputBuf, pos, speedRatio, output, numOutputSamplesToProduce);

    pos = subSamplePos + (numOutputSamplesToProduce * speedRatio) -
          numInputSamples;

    int lastIndexUsed =
        (int)std::ceil(subSamplePos + (numOutputSamplesToProduce * speedRatio));

    numUsed = lastIndexUsed - memorySize;

    if (lastIndexUsed > inputBufferSize) {
      printf("lastIndexUsed (%d) > inputBufferSize (%d)\n", lastIndexUsed,
             inputBufferSize);
      throw std::runtime_error(
          "lastIndexUsed (" + std::to_string(lastIndexUsed) +
          ") > inputBufferSize (" + std::to_string(inputBufferSize) + ")");
    }

    while (pos <= (memorySize - 1)) {
      // In the original code, the real position in the lastInputSamples buffer
      // is given by (pos + indexBuffer) % memorySize. However, we don't have
      // indexBuffer in this code (as we're not using a ring buffer). Thus,
      // pos must always end pointing at the last sample in lastInputSamples
      // or later.
      pos += 1;
    }

    debugprintf(
        "Copying %d samples to lastInputSamples from inputBuf[%d:%d]: \t[",
        memorySize, lastIndexUsed - memorySize, lastIndexUsed);
    std::memcpy(lastInputSamples, inputBuf + lastIndexUsed - memorySize,
                memorySize * sizeof(float));
    for (int i = 0; i < memorySize; i++) {
      if (i == memorySize - 1)
        debugprintf("%f]\n", lastInputSamples[i]);
      else
        debugprintf("%f, ", lastInputSamples[i]);
    }
    subSamplePos = pos;
    debugprintf(
        "Exiting with numUsed=%d, subSamplePos=%f, lastInputSamples = [",
        numUsed, subSamplePos);
    for (int i = 0; i < memorySize; i++) {
      if (i == memorySize - 1)
        debugprintf("%f]\n\n", lastInputSamples[i]);
      else
        debugprintf("%f, ", lastInputSamples[i]);
    }
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];

  // This sub-sample position is indexed from the start of lastInputSamples,
  // which is initialized with zeros.
  double subSamplePos = memorySize;

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
                                            const float offset) noexcept {
      const float frac = offset - (int)offset;

      const int numCrossings = 100;
      const float floatCrossings = (float)numCrossings;
      float result = 0.0f;

      auto samplePosition = (int)(offset + 1 - 2 * algorithmicLatency);
      float firstFrac = 0.0f;
      float lastSincPosition = -1.0f;
      int index = 0, sign = -1;

      for (int i = -numCrossings; i <= numCrossings; ++i) {
        auto sincPosition = (1.0f - frac) + (float)i;

        if (i == -numCrossings || (sincPosition >= 0 && lastSincPosition < 0)) {
          auto indexFloat =
              (sincPosition >= 0.f ? sincPosition : -sincPosition) * 100.0f;
          auto indexFloored = std::floor(indexFloat);
          index = (int)indexFloored;
          firstFrac = indexFloat - indexFloored;
          sign = (sincPosition < 0 ? -1 : 1);
        }

        if (sincPosition == 0.0f) {
          debugprintf(
              "[new WinSin] Reading from inputs[%d] => \e[0;32m%f\e[0;0m\n",
              samplePosition, inputs[samplePosition]);
          result += inputs[samplePosition];
        } else if (sincPosition < floatCrossings &&
                   sincPosition > -floatCrossings) {
          debugprintf(
              "[new WinSin] Reading from inputs[%d] => \e[0;32m%f\e[0;0m\n",
              samplePosition, inputs[samplePosition]);
          result += inputs[samplePosition] * windowedSinc(firstFrac, index);
        }

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

    static float valueAtOffset(const float *, float) noexcept;
  };

  struct CatmullRomTraits {
    //==============================================================================
    static constexpr float algorithmicLatency = 2.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset) noexcept {
      const int index = (int)offset;
      const float frac = offset - (float)index;

      auto y0 = inputs[index - 3];
      auto y1 = inputs[index - 2];
      auto y2 = inputs[index - 1];
      auto y3 = inputs[index];

      auto halfY0 = 0.5f * y0;
      auto halfY3 = 0.5f * y3;

      auto output =
          y1 +
          frac * ((0.5f * y2 - halfY0) +
                  (frac *
                   (((y0 + 2.0f * y2) - (halfY3 + 2.5f * y1)) +
                    (frac * ((halfY3 + 1.5f * y1) - (halfY0 + 1.5f * y2))))));

      debugprintf("[new CatRom] Reading from sub-sample pos %f: y0=%f, "
                  "y1=%f, y2=%f, y3=%f => \e[0;32m%f\e[0;0m\n",
                  frac, y0, y1, y2, y3, output);
      return output;
    }
  };

  struct LinearTraits {
    static constexpr float algorithmicLatency = 1.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset) noexcept {
      const int index = (int)offset;
      const float frac = offset - (float)index;
      auto y0 = inputs[index - 1];
      auto y1 = inputs[index];

      auto res = y1 * frac + y0 * (1.0f - frac);
      debugprintf("[new Linear] Reading from sub-sample pos %f            : "
                  "y0=%f, y1=%f => \e[0;32m%f\e[0;0m\n",
                  offset, y0, y1, res);
      return res;
    }
  };

  struct ZeroOrderHoldTraits {
    static constexpr float algorithmicLatency = 0.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float offset) noexcept {
      return inputs[(int)offset];
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
