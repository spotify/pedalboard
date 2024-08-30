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
  double reciprocal = 1.0 / value;
  return (value * K) == (double)(int)(value * K) &&
         (reciprocal * K) == (double)(int)(reciprocal * K);
}

/**
 * The original JUCE resampler code accumulates floating point error in a
 * very specific way. To remain consistent with that, this class does the same.
 */
template <bool MatchOriginalPrecision = true> struct Position {
  long long numUsed = 0;
  long long integerPart = 0;
  // This looks weird, right? Well, again, this is to match the original code
  // with all of its floating-point quirks.
  double fractionalPart = 1.0;

  Position(long long _integerStartValue) : integerPart(_integerStartValue) {}

  double getTotalPosition() const { return integerPart + fractionalPart; }

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
    if ((K >= 8 && isExactRatio<K>(ratio)) || !MatchOriginalPrecision) {
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

template <typename trait, unsigned long K, typename Pos>
static forcedinline void
computeValuesAtOffsets(const float *const __restrict inputs, Pos &position,
                       const double posOffset,
                       float *const __restrict outputs) noexcept {
  std::array<float, K> results;
  std::array<float, K> fractionalPositions;
  std::array<int, K> integerPositions;
  position.template incrementBatch<K>(posOffset, fractionalPositions,
                                      integerPositions);

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

template <typename trait, typename Pos>
static forcedinline void
computeValuesAtOffsets(const float *const __restrict inputs, Pos &position,
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
template <class InterpolatorTraits, int memorySize,
          typename Pos = Position<false>>
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
    subSamplePos = Pos(memorySize - 1);
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
    float inputBuf[memorySize * 3]; // up to inputBufferSize will be used

    // Copy all of lastInputSamples in:
    std::memcpy(inputBuf, lastInputSamples, memorySize * sizeof(float));
    std::memcpy(inputBuf + memorySize, input,
                numInputSamplesToBuffer * sizeof(float));

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

    numUsed = pos.numUsed;
    pos.decrementInteger(numUsed);
    pos.resetNumUsed();

    while (pos.getTotalPosition() <= (memorySize - 1)) {
      // In the original code, the real position in the lastInputSamples
      // buffer is given by (pos + indexBuffer) % memorySize. However, we
      // don't have indexBuffer in this code (as we're not using a ring
      // buffer). Thus, pos must always end pointing at the last sample in
      // lastInputSamples or later.
      pos.incrementInteger(1);
    }

    if (numUsed >= inputBufferSwitchThreshold) {
      std::memcpy(lastInputSamples,
                  input + numUsed + memorySize - memorySize * 2,
                  memorySize * sizeof(float));
    } else if (numUsed > 0) {
      int startOfCopy =
          std::min(inputBufferSize, numUsed + memorySize) - memorySize;
      std::memcpy(lastInputSamples, inputBuf + startOfCopy,
                  memorySize * sizeof(float));
    }

    subSamplePos = pos;
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)memorySize];

  // This sub-sample position is indexed from the start of lastInputSamples,
  // which is initialized with zeros.
  Pos subSamplePos = Pos(memorySize);

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

      return y1 * frac + y0 * (1.0f - frac);
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
  using LegacyPosition = Position<true>;
  using ExactPosition = Position<false>;

  // NOTE(psobot): These two interpolator types are very particular about
  // the order in which their input buffers are filled (i.e.: left-to-right
  // instead of right-to-left). This is not yet implemented in the above
  // SIMD code. To enable this, some of the indexing and buffer filling logic
  // will have to be rewritten to accommodate this.
  //
  // using WindowedSinc  = GenericInterpolator<WindowedSincTraits,  200>;
  // using Lagrange = SIMDGenericInterpolator<LagrangeTraits, 5>;

  // These variants are faster for all sampling ratios, but their outputs
  // are not byte-for-byte identical to the original JUCE interpolators:
  using CatmullRom =
      SIMDGenericInterpolator<CatmullRomTraits, 4, ExactPosition>;
  using Linear = SIMDGenericInterpolator<LinearTraits, 2, ExactPosition>;
  using ZeroOrderHold =
      SIMDGenericInterpolator<ZeroOrderHoldTraits, 1, ExactPosition>;

  // Variants that match the original precision of the JUCE interpolators,
  // but may be slower for non-exact sampling ratios:
  using LegacyCatmullRom =
      SIMDGenericInterpolator<CatmullRomTraits, 4, LegacyPosition>;
  using LegacyLinear = SIMDGenericInterpolator<LinearTraits, 2, LegacyPosition>;
  using LegacyZeroOrderHold =
      SIMDGenericInterpolator<ZeroOrderHoldTraits, 1, LegacyPosition>;
};

} // namespace juce
