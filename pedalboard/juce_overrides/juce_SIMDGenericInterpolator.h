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

  /** Resamples a stream of samples.

      @param speedRatio                   the number of input samples to use for
     each output sample
      @param inputSamples                 the source data to read from. This
     must contain at least (speedRatio * numOutputSamplesToProduce) samples.
      @param outputSamples                the buffer to write the results into
      @param numOutputSamplesToProduce    the number of output samples that
     should be created
      @param numInputSamplesAvailable     the number of available input samples.
     If it needs more samples than available, it either wraps back for
     wrapAround samples, or it feeds zeroes
      @param wrapAround                   if the stream exceeds available
     samples, it wraps back for wrapAround samples. If wrapAround is set to 0,
     it will feed zeroes.

      @returns the actual number of input samples that were used
  */
  int process(double speedRatio, const float *inputSamples,
              float *outputSamples, int numOutputSamplesToProduce,
              int numInputSamplesAvailable, int wrapAround) noexcept {
    return interpolate(speedRatio, inputSamples, outputSamples,
                       numOutputSamplesToProduce, numInputSamplesAvailable,
                       wrapAround);
  }

  /** Resamples a stream of samples, adding the results to the output data
      with a gain.

      @param speedRatio                   the number of input samples to use for
     each output sample
      @param inputSamples                 the source data to read from. This
     must contain at least (speedRatio * numOutputSamplesToProduce) samples.
      @param outputSamples                the buffer to write the results to -
     the result values will be added to any pre-existing data in this buffer
     after being multiplied by the gain factor
      @param numOutputSamplesToProduce    the number of output samples that
     should be created
      @param gain                         a gain factor to multiply the
     resulting samples by before adding them to the destination buffer

      @returns the actual number of input samples that were used
  */
  int processAdding(double speedRatio, const float *inputSamples,
                    float *outputSamples, int numOutputSamplesToProduce,
                    float gain) noexcept {
    return interpolateAdding(speedRatio, inputSamples, outputSamples,
                             numOutputSamplesToProduce, gain);
  }

  /** Resamples a stream of samples, adding the results to the output data
      with a gain.

      @param speedRatio                   the number of input samples to use for
     each output sample
      @param inputSamples                 the source data to read from. This
     must contain at least (speedRatio * numOutputSamplesToProduce) samples.
      @param outputSamples                the buffer to write the results to -
     the result values will be added to any pre-existing data in this buffer
     after being multiplied by the gain factor
      @param numOutputSamplesToProduce    the number of output samples that
     should be created
      @param numInputSamplesAvailable     the number of available input samples.
     If it needs more samples than available, it either wraps back for
     wrapAround samples, or it feeds zeroes
      @param wrapAround                   if the stream exceeds available
     samples, it wraps back for wrapAround samples. If wrapAround is set to 0,
     it will feed zeroes.
      @param gain                         a gain factor to multiply the
     resulting samples by before adding them to the destination buffer

      @returns the actual number of input samples that were used
  */
  int processAdding(double speedRatio, const float *inputSamples,
                    float *outputSamples, int numOutputSamplesToProduce,
                    int numInputSamplesAvailable, int wrapAround,
                    float gain) noexcept {
    return interpolateAdding(speedRatio, inputSamples, outputSamples,
                             numOutputSamplesToProduce,
                             numInputSamplesAvailable, wrapAround, gain);
  }

private:
  //==============================================================================
  forcedinline void pushInterpolationSample(float newValue) noexcept {
    lastInputSamples[indexBuffer] = newValue;

    if (++indexBuffer == memorySize)
      indexBuffer = 0;
  }

  forcedinline void
  pushInterpolationSamples(const float *input,
                           int numOutputSamplesToProduce) noexcept {
    if (numOutputSamplesToProduce >= memorySize) {
      const auto *const offsetInput =
          input + (numOutputSamplesToProduce - memorySize);

      for (int i = 0; i < memorySize; ++i)
        pushInterpolationSample(offsetInput[i]);
    } else {
      for (int i = 0; i < numOutputSamplesToProduce; ++i)
        pushInterpolationSample(input[i]);
    }
  }

  forcedinline void pushInterpolationSamples(const float *input,
                                             int numOutputSamplesToProduce,
                                             int numInputSamplesAvailable,
                                             int wrapAround) noexcept {
    if (numOutputSamplesToProduce >= memorySize) {
      if (numInputSamplesAvailable >= memorySize) {
        pushInterpolationSamples(input, numOutputSamplesToProduce);
      } else {
        pushInterpolationSamples(
            input +
                ((numOutputSamplesToProduce - numInputSamplesAvailable) - 1),
            numInputSamplesAvailable);

        if (wrapAround > 0) {
          numOutputSamplesToProduce -= wrapAround;

          pushInterpolationSamples(
              input + ((numOutputSamplesToProduce -
                        (memorySize - numInputSamplesAvailable)) -
                       1),
              memorySize - numInputSamplesAvailable);
        } else {
          for (int i = numInputSamplesAvailable; i < memorySize; ++i)
            pushInterpolationSample(0.0f);
        }
      }
    } else {
      if (numOutputSamplesToProduce > numInputSamplesAvailable) {
        for (int i = 0; i < numInputSamplesAvailable; ++i)
          pushInterpolationSample(input[i]);

        const auto extraSamples =
            numOutputSamplesToProduce - numInputSamplesAvailable;

        if (wrapAround > 0) {
          const auto *const offsetInput =
              input + (numInputSamplesAvailable - wrapAround);

          for (int i = 0; i < extraSamples; ++i)
            pushInterpolationSample(offsetInput[i]);
        } else {
          for (int i = 0; i < extraSamples; ++i)
            pushInterpolationSample(0.0f);
        }
      } else {
        for (int i = 0; i < numOutputSamplesToProduce; ++i)
          pushInterpolationSample(input[i]);
      }
    }
  }

  //==============================================================================
  int interpolate(double speedRatio, const float *input, float *output,
                  int numOutputSamplesToProduce) noexcept {
    auto pos = subSamplePos;
    int numUsed = 0;

    while (numOutputSamplesToProduce > 0) {
      while (pos >= 1.0) {
        pushInterpolationSample(input[numUsed++]);
        pos -= 1.0;
      }

      // `valueAtOffset` can be forcedinlined, so these seemingly-redundant
      // loops allow SIMD vectorization that gets us a 20-30% speedup:
      if ((pos + 3 * speedRatio) < 1.0) {
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      } else if ((pos + speedRatio) < 1.0) {
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      } else {
        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      }
    }

    subSamplePos = pos;
    return numUsed;
  }

  int interpolate(double speedRatio, const float *input, float *output,
                  int numOutputSamplesToProduce, int numInputSamplesAvailable,
                  int wrap) noexcept {
    auto originalIn = input;
    auto pos = subSamplePos;
    bool exceeded = false;

    if (speedRatio < 1.0) {
      for (int i = numOutputSamplesToProduce; --i >= 0;) {
        if (pos >= 1.0) {
          if (exceeded) {
            pushInterpolationSample(0.0f);
          } else {
            pushInterpolationSample(*input++);

            if (--numInputSamplesAvailable <= 0) {
              if (wrap > 0) {
                input -= wrap;
                numInputSamplesAvailable += wrap;
              } else {
                exceeded = true;
              }
            }
          }

          pos -= 1.0;
        }

        *output++ = InterpolatorTraits::valueAtOffset(lastInputSamples,
                                                      (float)pos, indexBuffer);
        pos += speedRatio;
      }
    } else {
      for (int i = numOutputSamplesToProduce; --i >= 0;) {
        while (pos < speedRatio) {
          if (exceeded) {
            pushInterpolationSample(0);
          } else {
            pushInterpolationSample(*input++);

            if (--numInputSamplesAvailable <= 0) {
              if (wrap > 0) {
                input -= wrap;
                numInputSamplesAvailable += wrap;
              } else {
                exceeded = true;
              }
            }
          }

          pos += 1.0;
        }

        pos -= speedRatio;
        *output++ = InterpolatorTraits::valueAtOffset(
            lastInputSamples, jmax(0.0f, 1.0f - (float)pos), indexBuffer);
      }
    }

    subSamplePos = pos;

    if (wrap == 0)
      return (int)(input - originalIn);

    return ((int)(input - originalIn) + wrap) % wrap;
  }

  int interpolateAdding(double speedRatio, const float *input, float *output,
                        int numOutputSamplesToProduce,
                        int numInputSamplesAvailable, int wrap,
                        float gain) noexcept {
    auto originalIn = input;
    auto pos = subSamplePos;
    bool exceeded = false;

    if (speedRatio < 1.0) {
      for (int i = numOutputSamplesToProduce; --i >= 0;) {
        if (pos >= 1.0) {
          if (exceeded) {
            pushInterpolationSample(0.0);
          } else {
            pushInterpolationSample(*input++);

            if (--numInputSamplesAvailable <= 0) {
              if (wrap > 0) {
                input -= wrap;
                numInputSamplesAvailable += wrap;
              } else {
                numInputSamplesAvailable = true;
              }
            }
          }

          pos -= 1.0;
        }

        *output++ += gain * InterpolatorTraits::valueAtOffset(
                                lastInputSamples, (float)pos, indexBuffer);
        pos += speedRatio;
      }
    } else {
      for (int i = numOutputSamplesToProduce; --i >= 0;) {
        while (pos < speedRatio) {
          if (exceeded) {
            pushInterpolationSample(0.0);
          } else {
            pushInterpolationSample(*input++);

            if (--numInputSamplesAvailable <= 0) {
              if (wrap > 0) {
                input -= wrap;
                numInputSamplesAvailable += wrap;
              } else {
                exceeded = true;
              }
            }
          }

          pos += 1.0;
        }

        pos -= speedRatio;
        *output++ += gain * InterpolatorTraits::valueAtOffset(
                                lastInputSamples, jmax(0.0f, 1.0f - (float)pos),
                                indexBuffer);
      }
    }

    subSamplePos = pos;

    if (wrap == 0)
      return (int)(input - originalIn);

    return ((int)(input - originalIn) + wrap) % wrap;
  }

  int interpolateAdding(double speedRatio, const float *input, float *output,
                        int numOutputSamplesToProduce, float gain) noexcept {
    auto pos = subSamplePos;
    int numUsed = 0;

    while (numOutputSamplesToProduce > 0) {
      while (pos >= 1.0) {
        pushInterpolationSample(input[numUsed++]);
        pos -= 1.0;
      }

      *output++ += gain * InterpolatorTraits::valueAtOffset(
                              lastInputSamples, (float)pos, indexBuffer);
      pos += speedRatio;
      --numOutputSamplesToProduce;
    }

    subSamplePos = pos;
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
  };

  struct ZeroOrderHoldTraits {
    static constexpr float algorithmicLatency = 0.0f;

    static forcedinline float valueAtOffset(const float *const inputs,
                                            const float, int) noexcept {
      return inputs[0];
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
