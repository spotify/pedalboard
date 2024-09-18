#include "../JuceHeader.h"

inline double sinc(const double x) {
  if (x == 0)
    return 1;
  return sin(juce::MathConstants<double>::pi * x) /
         (juce::MathConstants<double>::pi * x);
}

inline double besselI0(int K, double x) {
  // Modified Bessel function of order 0.
  double sum = 0;
  for (int k = 0; k < K; k++) {
    sum += pow(pow(x, 2) / 4, k) / pow(tgamma(k + 1), 2);
  }
  return sum;
}

template <int NumZeros, int Precision>
std::vector<float> calculateSincTable(double rolloff, double kaiserBeta,
                                      int besselPrecision) {
  const int n = Precision * NumZeros;

  // Make the right hand side of the kaiser window with the provided beta:
  std::vector<float> kaiserWindow(n + 1);
  double besselI0OfBeta = besselI0(besselPrecision, kaiserBeta);
  for (int i = 0; i <= n; i++) {
    double alpha = (2 * n) / 2.0;
    double x = (i - alpha) / alpha;
    kaiserWindow[i] =
        besselI0(besselPrecision, kaiserBeta * sqrt(1 - (x * x))) /
        besselI0OfBeta;
  }

  std::vector<float> sinc_win(n + 1 // for the 0th element (magnitude 1)
                              + 2 // for the two extra zero elements at the end
  );
  for (int i = 0; i <= n; i++) {
    double linspace = NumZeros * ((double)i / (double)n);
    sinc_win[i] = rolloff * sinc(linspace * rolloff) * kaiserWindow[n - i];
  }
  return sinc_win;
}

template <int NumZeros, int Precision>
const std::vector<float> &getSincTable() {
  float rolloff = 0.990;
  float kaiserBeta = 8.00009;
  int besselPrecision = 10;

  if constexpr (NumZeros == 8) {
    kaiserBeta = 19.9989;
    besselPrecision = 32;
  } else if constexpr (NumZeros == 16) {
    kaiserBeta = 8.00113;
    besselPrecision = 16;
  } else if constexpr (NumZeros == 32) {
    kaiserBeta = 8.0001;
    besselPrecision = 16;
  } else if constexpr (NumZeros == 64) {
    kaiserBeta = 8.00264;
    besselPrecision = 16;
  } else if constexpr (NumZeros == 128) {
    kaiserBeta = 8.00013;
    besselPrecision = 16;
  }

  static std::vector<float> sinc_win = calculateSincTable<NumZeros, Precision>(
      rolloff, kaiserBeta, besselPrecision);
  return sinc_win;
}

namespace juce {

template <int _NumCrossings, int _DistanceBetweenCrossings>
struct FastWindowedSincTraits {
  static constexpr int NumCrossings = _NumCrossings;
  static constexpr int DistanceBetweenCrossings = _DistanceBetweenCrossings;

  static constexpr int LookupTableSize =
      NumCrossings * DistanceBetweenCrossings +
      1    // for the 0th element (magnitude 1)
      + 2; // for the two extra zero elements at the end
  ;
  static constexpr int BufferSize = NumCrossings * 4;

  static constexpr float algorithmicLatency =
      static_cast<float>(BufferSize / 2);

  static inline float windowedSinc(const float *lookupTable, float firstFrac,
                                   int index) noexcept {
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
  subsampleSincFilter(const float *lookupTable, float offset,
                      double speedRatio) noexcept {
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
      sincValues[i] = windowedSinc(lookupTable, fracs[i], ints[i]);
    }

    return sincValues;
  }

  // This works with a pre-computed sinc table:
  static float
  valueAtOffset(const float *const __restrict inputs, int indexBuffer,
                double speedRatio,
                const std::array<float, BufferSize> &sincValues) noexcept {
    double effectiveSpeedRatio = std::max(speedRatio, 1.0);

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
  FastWindowedSincInterpolator() noexcept {
    lookupTable = &getSincTable<InterpolatorTraits::NumCrossings,
                                InterpolatorTraits::DistanceBetweenCrossings>();
    reset();
  }

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

  int process(double speedRatio, const float *inputSamples,
              float *outputSamples, int numOutputSamplesToProduce) noexcept {
    return interpolate(speedRatio, inputSamples, outputSamples,
                       numOutputSamplesToProduce);
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

    const float *lookupTablePointer = lookupTable->data();

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
                InterpolatorTraits::subsampleSincFilter(lookupTablePointer,
                                                        pair.first, speedRatio);
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
          auto sincValues = InterpolatorTraits::subsampleSincFilter(
              lookupTablePointer, (float)pos, speedRatio);
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

        auto sincValues = InterpolatorTraits::subsampleSincFilter(
            lookupTablePointer, pos, speedRatio);
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

  const std::vector<float> *lookupTable;

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