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

template <int K> static forcedinline bool isExactRatio(double value) {
  return value == 0.5 || value == 2 || value == 0.25 || value == 4 ||
         value == 0.125 || value == 8 || value == 0.0625 || value == 16;
}

/**
 * @brief The original JUCE resampler code accumulates floating point error in a
 * very specific way. To remain consistent with that, this class does the same.
 *
 */
struct Position {
  long long numUsed = 0;
  long long integerPart = 0;
  // This looks weird, right? Well, again, this is to match the original code
  // with all of its floating-point quirks.
  double fractionalPart = 1.0;

  Position(long long _integerStartValue) : integerPart(_integerStartValue) {}

  double getTotalPosition() const {
    // printf("getTotalPosition: fractionalPart=%.12f, integerPart=%d\n",
    //        fractionalPart, integerPart);
    return integerPart + fractionalPart;
  }

  int getCeiling() const {
    return (fractionalPart > 0) ? integerPart + 1 : integerPart;
  }

  int getCeilingMinus(double offset) const {
    int toReturn = integerPart + 1;
    double fractionalPartCopy = fractionalPart - offset;
    while (fractionalPartCopy < 0 || fractionalPartCopy == 0) {
      toReturn--;
      fractionalPartCopy += 1.0;
    }
    return toReturn;
  }

  void incrementInteger(long long amount) { integerPart += amount; }

  template <int K>
  forcedinline void incrementBatch(double ratio,
                                   std::array<float, K> &fractionalParts,
                                   std::array<int, K> &integerParts) {

    int integerPartStart = integerPart;
    int _integerPart = integerPart;
    double _fractionalPart = fractionalPart;

    // For certain ratios that we know to be absolutely exact in floating point,
    // we can speed things up by using closed form methods and integer math,
    // which can be vectorized by the compiler much more effectively:
    if (isExactRatio<K>(ratio)) {
      if (_fractionalPart >= 1.0) {
        _integerPart += (int)_fractionalPart;
        _fractionalPart -= (int)_fractionalPart;
      }
#pragma clang loop vectorize(enable) interleave(enable)
      for (int i = 0; i < K; i++) {
        fractionalParts[i] = _fractionalPart + (ratio * i) -
                             (int)(_fractionalPart + (ratio * i));
        integerParts[i] = _integerPart + (i * ratio);
      }
      _fractionalPart = fractionalParts[K - 1] + ratio;
      _integerPart = integerParts[K - 1];
    } else {
#pragma clang loop vectorize(enable) interleave(enable)
      for (int i = 0; i < K; i++) {
        while (_fractionalPart >= 1.0) {
          _fractionalPart -= 1.0;
          _integerPart++;
        }

        fractionalParts[i] = _fractionalPart;
        integerParts[i] = _integerPart;

        _fractionalPart += ratio;
      }
    }

    integerPart = _integerPart;
    fractionalPart = _fractionalPart;
    numUsed += _integerPart - integerPartStart;
  }

  void decrementInteger(long long amount) { integerPart -= amount; }

  void resetNumUsed() { numUsed = 0; }
};

template <typename trait, unsigned long K>
static forcedinline void
computeValuesAtOffsets(const float *const __restrict inputs, Position &position,
                       const double posOffset,
                       float *const __restrict outputs) noexcept {
  std::array<float, K> results;
  std::array<float, K> fractionalPositions;
  std::array<int, K> integerPositions;
  position.incrementBatch<K>(posOffset, fractionalPositions, integerPositions);

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    results[i] = trait::valueAtOffset(inputs, fractionalPositions[i],
                                      integerPositions[i]);
  }

#pragma clang loop vectorize(enable) interleave(enable)
  for (unsigned long i = 0; i < K; i++) {
    outputs[i] = results[i];
  }
}

template <typename trait>
static forcedinline void
computeValuesAtOffsets(const float *const __restrict inputs, Position &position,
                       const double posOffset, float *__restrict outputs,
                       int numOutputSamples) noexcept {
  while (numOutputSamples >= 1024) {
    computeValuesAtOffsets<trait, 1024>(inputs, position, posOffset, outputs);
    outputs += 1024;
    numOutputSamples -= 1024;
  }
  while (numOutputSamples >= 512) {
    computeValuesAtOffsets<trait, 512>(inputs, position, posOffset, outputs);
    outputs += 512;
    numOutputSamples -= 512;
  }
  while (numOutputSamples >= 256) {
    computeValuesAtOffsets<trait, 256>(inputs, position, posOffset, outputs);
    outputs += 256;
    numOutputSamples -= 256;
  }
  while (numOutputSamples >= 128) {
    computeValuesAtOffsets<trait, 128>(inputs, position, posOffset, outputs);
    outputs += 128;
    numOutputSamples -= 128;
  }
  while (numOutputSamples >= 64) {
    computeValuesAtOffsets<trait, 64>(inputs, position, posOffset, outputs);
    outputs += 64;
    numOutputSamples -= 64;
  }
  while (numOutputSamples >= 32) {
    computeValuesAtOffsets<trait, 32>(inputs, position, posOffset, outputs);
    outputs += 32;
    numOutputSamples -= 32;
  }
  while (numOutputSamples >= 32) {
    computeValuesAtOffsets<trait, 32>(inputs, position, posOffset, outputs);
    outputs += 32;
    numOutputSamples -= 32;
  }
  while (numOutputSamples >= 16) {
    computeValuesAtOffsets<trait, 16>(inputs, position, posOffset, outputs);
    outputs += 16;
    numOutputSamples -= 16;
  }
  while (numOutputSamples >= 8) {
    computeValuesAtOffsets<trait, 8>(inputs, position, posOffset, outputs);
    outputs += 8;
    numOutputSamples -= 8;
  }
  while (numOutputSamples >= 4) {
    computeValuesAtOffsets<trait, 4>(inputs, position, posOffset, outputs);
    outputs += 4;
    numOutputSamples -= 4;
  }
  while (numOutputSamples >= 4) {
    computeValuesAtOffsets<trait, 4>(inputs, position, posOffset, outputs);
    outputs += 4;
    numOutputSamples -= 4;
  }
  while (numOutputSamples >= 2) {
    computeValuesAtOffsets<trait, 2>(inputs, position, posOffset, outputs);
    outputs += 2;
    numOutputSamples -= 2;
  }
  if (numOutputSamples >= 1) {
    computeValuesAtOffsets<trait, 1>(inputs, position, posOffset, outputs);
    outputs++;
    numOutputSamples--;
  }
}

/**
    An interpolator base class for resampling streams of floats.

    Note that the resamplers are stateful, so when there's a break in the
   continuity of the input stream you're feeding it, you should call reset()
   before feeding it any new data. And like with any other stateful filter, if
   you're resampling multiple channels, make sure each one uses its own
   interpolator object.

    @see LagrangeInterpolator, CatmullRomInterpolator,
   WindowedSincInterpolator, LinearInterpolator, ZeroOrderHoldInterpolator

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

      Call this when there's a break in the continuity of the input data
     stream.
  */
  void reset() noexcept {
    subSamplePos = Position(memorySize - 1);
    std::fill(std::begin(lastInputSamples), std::end(lastInputSamples), 0.0f);
  }

  /** Resamples a stream of samples.

      @param speedRatio                   the number of input samples to use
     for each output sample
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
    // printf("\n\n[new       ]\e[0;32m Outputting %d samples with speed "
    //        "ratio %f and pos %.20f (%d + %.20f)\e[0m\n",
    //        numOutputSamplesToProduce, speedRatio,
    //        subSamplePos.getTotalPosition(), subSamplePos.integerPart,
    //        subSamplePos.fractionalPart);
    auto pos = subSamplePos;
    int numUsed = 0;

    int numInputSamples = std::ceil(numOutputSamplesToProduce * speedRatio);

    // The number of samples to copy from the input buffer into a temporary
    // stack buffer. Once we read past memorySize * 2 samples, we can switch
    // to reading the input buffer directly to avoid having to copy all of
    // the input into a temporary buffer.
    int numInputSamplesToBuffer = std::min(numInputSamples, memorySize * 2);
    int inputBufferSwitchThreshold = memorySize * 2;
    int inputBufferSize =
        std::max(numInputSamplesToBuffer + memorySize, pos.getCeiling());
    std::vector<float> vec(inputBufferSize);
    float *inputBuf = vec.data();

    // Copy all of lastInputSamples in:
    std::memcpy(inputBuf, lastInputSamples, memorySize * sizeof(float));
    std::memcpy(inputBuf + memorySize, input,
                numInputSamplesToBuffer * sizeof(float));

    // printf("numInputSamples is %d, copied into inputBuf = [",
    // numInputSamples); for (int i = 0; i < inputBufferSize; i++) {
    //   if (i == inputBufferSize - 1)
    //     printf("%f]\n\n", inputBuf[i]);
    //   else
    //     printf("%f, ", inputBuf[i]);
    // }

    if ((numInputSamples + memorySize) > inputBufferSwitchThreshold) {
      // Compute the first samples from the input buffer alone,
      // rounding down to figure out how many full output samples
      // we can compute:
      int numOutputSamplesThatCanBeProducedFromBuffer = 0;
      while (
          numOutputSamplesThatCanBeProducedFromBuffer <
              numOutputSamplesToProduce &&
          pos.getTotalPosition() +
                  (numOutputSamplesThatCanBeProducedFromBuffer * speedRatio) <
              memorySize * 2) {
        numOutputSamplesThatCanBeProducedFromBuffer++;
      }

      computeValuesAtOffsets<InterpolatorTraits>(
          inputBuf, pos, speedRatio, output,
          numOutputSamplesThatCanBeProducedFromBuffer);
      int remainingOutputSamples = numOutputSamplesToProduce -
                                   numOutputSamplesThatCanBeProducedFromBuffer;
      pos.decrementInteger(memorySize);
      computeValuesAtOffsets<InterpolatorTraits>(
          input, pos, speedRatio,
          output + numOutputSamplesThatCanBeProducedFromBuffer,
          remainingOutputSamples);
      pos.incrementInteger(memorySize);
    } else {
      computeValuesAtOffsets<InterpolatorTraits>(
          inputBuf, pos, speedRatio, output, numOutputSamplesToProduce);
    }

    // The following calculation is "more correct" but does not match the
    // original code: pos = subSamplePos + (numOutputSamplesToProduce *
    // speedRatio) -
    //       numInputSamples;

    // Note that this index is from the start of the lastInputSamples buffer,
    // virtually concatenated with the input buffer.
    // int lastIndexUsed =
    //     (int)std::ceil(subSamplePos.getTotalPosition() +
    //                    (numOutputSamplesToProduce * speedRatio));
    int lastIndexUsed = std::max(0, pos.getCeilingMinus(speedRatio));
    // printf("[new       ] lastIndexUsed = %d\n", lastIndexUsed);
    // printf("[new       ] pos = %d + %.20f\n", pos.integerPart,
    //        pos.fractionalPart);

    // numUsed = lastIndexUsed - memorySize - 1;
    numUsed = pos.numUsed;
    // printf("[new       ] numUsed = %d\n", numUsed);
    pos.decrementInteger(numUsed);
    pos.resetNumUsed();
    // printf("[new       ] pos = %d + %.20f\n", pos.integerPart,
    //        pos.fractionalPart);

    while (pos.getTotalPosition() <= (memorySize - 1)) {
      // In the original code, the real position in the lastInputSamples
      // buffer is given by (pos + indexBuffer) % memorySize. However, we
      // don't have indexBuffer in this code (as we're not using a ring
      // buffer). Thus, pos must always end pointing at the last sample in
      // lastInputSamples or later.
      pos.incrementInteger(1);
      // printf("[new       ] pos = %d + %.20f\n", pos.integerPart,
      //        pos.fractionalPart);
    }

    if ((lastIndexUsed - memorySize) >= inputBufferSwitchThreshold) {
      std::memcpy(lastInputSamples, input + lastIndexUsed - memorySize * 2,
                  memorySize * sizeof(float));
    } else if (lastIndexUsed > memorySize) {
      int startOfCopy = std::min(inputBufferSize, lastIndexUsed) - memorySize;
      printf("copying out of inputBuf from index %d = [", startOfCopy);
      for (int i = 0; i < inputBufferSize; i++) {
        if (i == inputBufferSize - 1)
          printf("%f]\n\n", inputBuf[i]);
        else
          printf("%f, ", inputBuf[i]);
      }
      std::memcpy(lastInputSamples, inputBuf + startOfCopy,
                  memorySize * sizeof(float));
    }

    subSamplePos = pos;
    printf(
        "new Exiting with numUsed=%d, subSamplePos=%.20f, lastIndexUsed = % d, "
        "lastInputSamples = [",
        numUsed, subSamplePos.getTotalPosition(), lastIndexUsed);
    for (int i = 0; i < memorySize; i++) {
      if (i == memorySize - 1)
        printf("%f]\n\n", lastInputSamples[i]);
      else
        printf("%f, ", lastInputSamples[i]);
    }
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];

  // This sub-sample position is indexed from the start of lastInputSamples,
  // which is initialized with zeros.
  Position subSamplePos = Position(memorySize);

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
  struct LagrangeTraits {
    static constexpr float algorithmicLatency = 2.0f;

    static float valueAtOffset(const float *, float, int) noexcept;
  };

  struct CatmullRomTraits {
    //==============================================================================
    static constexpr float algorithmicLatency = 2.0f;

    static forcedinline float
    valueAtOffset(const float *const __restrict inputs, const float frac,
                  const int index) noexcept {
      auto y0 = inputs[index - 3];
      auto y1 = inputs[index - 2];
      auto y2 = inputs[index - 1];
      auto y3 = inputs[index];

      auto halfY0 = 0.5f * y0;
      auto halfY3 = 0.5f * y3;

      return y1 + frac * ((0.5f * y2 - halfY0) +
                          (frac * (((y0 + 2.0f * y2) - (halfY3 + 2.5f * y1)) +
                                   (frac * ((halfY3 + 1.5f * y1) -
                                            (halfY0 + 1.5f * y2))))));
    }
  };

  struct LinearTraits {
    static constexpr float algorithmicLatency = 1.0f;

    static forcedinline float
    valueAtOffset(const float *const __restrict inputs, const float frac,
                  const int index) noexcept {
      auto y0 = inputs[index - 1];
      auto y1 = inputs[index];

      auto result = y1 * frac + y0 * (1.0f - frac);
      // printf("{\"type\": \"new\", \"y0\": %.12f, \"y1\": %.12f, \"frac\": "
      //        "%.12f, \"index\": %d, \"result\": %.12f}\n",
      //        y0, y1, frac, index, result);
      return result;
    }
  };

  struct ZeroOrderHoldTraits {
    static constexpr float algorithmicLatency = 0.0f;

    static forcedinline float
    valueAtOffset(const float *const __restrict inputs, const float frac,
                  const int index) noexcept {
      return inputs[index];
    }
  };

public:
  using Lagrange = SIMDGenericInterpolator<LagrangeTraits, 5>;
  using CatmullRom = SIMDGenericInterpolator<CatmullRomTraits, 4>;
  using Linear = SIMDGenericInterpolator<LinearTraits, 2>;
  using ZeroOrderHold = SIMDGenericInterpolator<ZeroOrderHoldTraits, 1>;
};

} // namespace juce
