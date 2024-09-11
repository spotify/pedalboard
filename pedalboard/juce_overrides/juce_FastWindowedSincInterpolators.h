#include "../JuceHeader.h"

namespace juce {

template <int NumCrossings, int DistanceBetweenCrossings>
struct FastWindowedSincTraits {
  static constexpr int LookupTableSize =
      NumCrossings * DistanceBetweenCrossings +
      1    // for the 0th element (magnitude 1)
      + 2; // for the two extra zero elements at the end
  ;
  static constexpr int BufferSize = NumCrossings * 4;

  static constexpr float algorithmicLatency =
      static_cast<float>(BufferSize / 2);

  static inline float windowedSinc(float firstFrac, int index) noexcept {
    auto value1 = lookupTable[index];
    auto value2 = lookupTable[index + 1];
    return value1 + (firstFrac * (value2 - value1));
  }

  /* use type punning instead of pointer arithmetic, to require proper
   * alignment
   */
  static inline float float2absf(float f) {
    /* optimizer will optimize away the `if` statement and the library call */
    if (sizeof(float) == sizeof(uint32_t)) {
      union {
        float f;
        uint32_t i;
      } u;
      u.f = f;
      u.i &= 0x7fffffff;
      return u.f;
    }
    return fabsf(f);
  }

  static std::array<float, BufferSize>
  subsampleSincFilter(float offset, double speedRatio) noexcept {
    double effectiveSpeedRatio = std::max(speedRatio, 1.0);
    const float sincStart = 1.0f - offset - ((float)BufferSize / 2.0f);
    float sincPositions[BufferSize];
#pragma clang loop vectorize(enable) interleave(enable)
#pragma unroll
    for (int i = 0; i < BufferSize; i++) {
      sincPositions[i] = float2absf(sincStart + i);
    }

    float fracs[BufferSize];
    int ints[BufferSize];

    float sincTableHop = DistanceBetweenCrossings / effectiveSpeedRatio;

#pragma clang loop vectorize(enable) interleave(enable)
#pragma unroll
    for (int i = 0; i < BufferSize; i++) {
      const float indexFloat = std::min((float)(LookupTableSize - 2),
                                        sincPositions[i] * sincTableHop);
      const int indexInt = static_cast<int>(indexFloat);
      ints[i] = indexInt;
      float firstFrac = indexFloat - indexInt;
      fracs[i] = firstFrac;
    }

    // Pre-compute sinc values and indices
    std::array<float, BufferSize> sincValues;

#pragma clang loop vectorize(enable) interleave(enable)
#pragma unroll
    for (int i = 0; i < BufferSize; i++) {
      sincValues[i] = windowedSinc(fracs[i], ints[i]);
    }

    return sincValues;
  }

  // This works with a pre-computed sinc table:
  static float
  valueAtOffset(const float *const __restrict inputs, int indexBuffer,
                double speedRatio,
                const std::array<float, BufferSize> &sincValues) noexcept {
    double effectiveSpeedRatio = std::max(speedRatio, 1.0);
    //     int inputIndices[BufferSize];
    // #pragma clang loop vectorize(enable) interleave(enable)
    //     for (int i = 0; i < BufferSize; i++) {
    //       // inputIndices[i] = (indexBuffer + i) % BufferSize;
    //       // Very neat trick: regular modular math is not vectorizable,
    //       // but floating-point multiplication and subtraction is!
    //       // So doing this in float32 space and then converting to int
    //       // is about 4x faster, as the compiler can emit SIMD instructions.
    //       // The result is identical as long as the range of the input
    //       // is within the range of the mantissa of a float32.
    //       const int d = indexBuffer + i;
    //       const int index =
    //           d - ((int)((float)(d) / (float)BufferSize) * BufferSize);
    //       inputIndices[i] = index;
    //     }

    // Copy from inputs in two contiguous chunks
    // ([indexBuffer, BufferSize), then [0, indexBuffer))
    // to minimize branching:
    float inputsBuffer[BufferSize];
    int firstChunkSize = BufferSize - indexBuffer;
    int secondChunkSize = BufferSize - firstChunkSize;
    std::memcpy(inputsBuffer, inputs + indexBuffer,
                firstChunkSize * sizeof(float));
    std::memcpy(inputsBuffer + firstChunkSize, inputs,
                secondChunkSize * sizeof(float));

    // Main computation loop
    float result = 0.0f;
#pragma clang loop vectorize(enable) interleave(enable)
#pragma unroll
    for (int i = 0; i < BufferSize; i++) {
      result += inputsBuffer[i] * sincValues[i];
    }

    return result / effectiveSpeedRatio;
  }

  static const float lookupTable[LookupTableSize];
};

/**
   An interpolator base class for resampling streams of floats using
   windowed-sinc interpolation while avoiding aliasing. This class differs from
   JUCE's built-in FastWindowedSincInterpolator in that it also passes the
   sampling ratio to the interpolator logic, allowing us to implement a low-pass
   filter simultaneously while the resampling occurs.
*/
template <class InterpolatorTraits> class FastWindowedSincInterpolator {
public:
  FastWindowedSincInterpolator() noexcept { reset(); }

  FastWindowedSincInterpolator(FastWindowedSincInterpolator &&) noexcept =
      default;
  FastWindowedSincInterpolator &
  operator=(FastWindowedSincInterpolator &&) noexcept = default;

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

private:
  //==============================================================================
  forcedinline void pushInterpolationSample(float newValue) noexcept {
    lastInputSamples[indexBuffer] = newValue;

    if (++indexBuffer == InterpolatorTraits::BufferSize)
      indexBuffer = 0;
  }

  //==============================================================================
  int interpolate(double speedRatio, const float *input, float *output,
                  int numOutputSamplesToProduce) noexcept {
    auto pos = subSamplePos;
    int numUsed = 0;

    // Pre-compute the sinc interpolation tables if possible:
    if (cachedSincValueTables.empty()) {
      static constexpr int MAX_CACHED_SINC_TABLES = 64;

      double tempPos = pos;
      std::map<double, int> offsetHistogram;
      for (int i = 0; i < MAX_CACHED_SINC_TABLES; i++) {
        while (tempPos >= 1.0) {
          tempPos -= 1.0;
        }
        offsetHistogram[tempPos]++;
        tempPos += speedRatio;
      }

      // Count the common offset values:
      for (auto &pair : offsetHistogram) {
        if (pair.second > 1) {
          if (cachedSincValueTables.find({speedRatio, pair.first}) ==
              cachedSincValueTables.end()) {
            // We have a common offset value, so we can precompute the sinc
            // table for this offset value:
            cachedSincValueTables[{speedRatio, pair.first}] =
                InterpolatorTraits::subsampleSincFilter(pair.first, speedRatio);
          }
        }
      }
    }

    if (!cachedSincValueTables.empty()) {
      while (numOutputSamplesToProduce > 0) {
        while (pos >= 1.0) {
          pushInterpolationSample(input[numUsed++]);
          pos -= 1.0;
        }

        // Check if we have a cached sinc table and create one if necessary:
        if (cachedSincValueTables.find({speedRatio, pos}) ==
            cachedSincValueTables.end()) {
          // This should not happen; we should have precomputed all sinc tables
          // above. Create a new sinc table on the stack:
          auto sincValues =
              InterpolatorTraits::subsampleSincFilter((float)pos, speedRatio);
          *output++ = InterpolatorTraits::valueAtOffset(
              lastInputSamples, indexBuffer, speedRatio, sincValues);
        } else {
          const auto &sincValues = cachedSincValueTables[{speedRatio, pos}];
          *output++ = InterpolatorTraits::valueAtOffset(
              lastInputSamples, indexBuffer, speedRatio, sincValues);
        }

        pos += speedRatio;
        --numOutputSamplesToProduce;
      }
    } else {
      while (numOutputSamplesToProduce > 0) {
        while (pos >= 1.0) {
          pushInterpolationSample(input[numUsed++]);
          pos -= 1.0;
        }

        auto sincValues =
            InterpolatorTraits::subsampleSincFilter(pos, speedRatio);
        *output++ = InterpolatorTraits::valueAtOffset(
            lastInputSamples, indexBuffer, speedRatio, sincValues);
        pos += speedRatio;
        --numOutputSamplesToProduce;
      }
    }

    subSamplePos = pos;
    return numUsed;
  }

  //==============================================================================
  float lastInputSamples[(size_t)InterpolatorTraits::BufferSize];
  double subSamplePos = 1.0;
  int indexBuffer = 0;

  // A mapping of (speedRatio, offset) to precomputed sinc position tables:
  std::map<std::pair<double, float>,
           std::array<float, InterpolatorTraits::BufferSize>>
      cachedSincValueTables;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FastWindowedSincInterpolator)
};

class FastInterpolators {
public:
#define WINDOWEDSINC(numCrossings, precision)                                  \
  FastWindowedSincInterpolator<FastWindowedSincTraits<numCrossings, precision>>
  using WindowedSinc256 = WINDOWEDSINC(256, 512);
  using WindowedSinc128 = WINDOWEDSINC(128, 512);
  using WindowedSinc64 = WINDOWEDSINC(64, 512);
  using WindowedSinc32 = WINDOWEDSINC(32, 512);
  using WindowedSinc16 = WINDOWEDSINC(16, 512);
  using WindowedSinc8 = WINDOWEDSINC(8, 512);
};

}; // namespace juce