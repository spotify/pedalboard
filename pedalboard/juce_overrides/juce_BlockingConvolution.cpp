#include "juce_BlockingConvolution.h"

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

/**
 * NOTE(psobot): this is copied nearly entirely from juce_Convolution.cpp.
 * The original implementation is threadsafe, but has race conditions, so for
 * pedalboard, the class below is used instead. Much of the original code is
 * copied to avoid drift if we upgrade JUCE.
 */

namespace juce {
namespace dsp {

//==============================================================================
//==============================================================================
struct ConvolutionEngine {
  ConvolutionEngine(const float *samples, size_t numSamples,
                    size_t maxBlockSize)
      : blockSize((size_t)nextPowerOfTwo((int)maxBlockSize)),
        fftSize(blockSize > 128 ? 2 * blockSize : 4 * blockSize),
        fftObject(std::make_unique<FFT>(roundToInt(std::log2(fftSize)))),
        numSegments(numSamples / (fftSize - blockSize) + 1u),
        numInputSegments((blockSize > 128 ? numSegments : 3 * numSegments)),
        bufferInput(1, static_cast<int>(fftSize)),
        bufferOutput(1, static_cast<int>(fftSize * 2)),
        bufferTempOutput(1, static_cast<int>(fftSize * 2)),
        bufferOverlap(1, static_cast<int>(fftSize)) {
    bufferOutput.clear();

    auto updateSegmentsIfNecessary =
        [this](size_t numSegmentsToUpdate,
               std::vector<AudioBuffer<float>> &segments) {
          if (numSegmentsToUpdate == 0 ||
              numSegmentsToUpdate != (size_t)segments.size() ||
              (size_t)segments[0].getNumSamples() != fftSize * 2) {
            segments.clear();

            for (size_t i = 0; i < numSegmentsToUpdate; ++i)
              segments.push_back({1, static_cast<int>(fftSize * 2)});
          }
        };

    updateSegmentsIfNecessary(numInputSegments, buffersInputSegments);
    updateSegmentsIfNecessary(numSegments, buffersImpulseSegments);

    auto FFTTempObject = std::make_unique<FFT>(roundToInt(std::log2(fftSize)));
    size_t currentPtr = 0;

    for (auto &buf : buffersImpulseSegments) {
      buf.clear();

      auto *impulseResponse = buf.getWritePointer(0);

      if (&buf == &buffersImpulseSegments.front())
        impulseResponse[0] = 1.0f;

      FloatVectorOperations::copy(
          impulseResponse, samples + currentPtr,
          static_cast<int>(jmin(fftSize - blockSize, numSamples - currentPtr)));

      FFTTempObject->performRealOnlyForwardTransform(impulseResponse);
      prepareForConvolution(impulseResponse);

      currentPtr += (fftSize - blockSize);
    }

    reset();
  }

  void reset() {
    bufferInput.clear();
    bufferOverlap.clear();
    bufferTempOutput.clear();
    bufferOutput.clear();

    for (auto &buf : buffersInputSegments)
      buf.clear();

    currentSegment = 0;
    inputDataPos = 0;
  }

  void processSamples(const float *input, float *output, size_t numSamples) {
    // Overlap-add, zero latency convolution algorithm with uniform partitioning
    size_t numSamplesProcessed = 0;

    auto indexStep = numInputSegments / numSegments;

    auto *inputData = bufferInput.getWritePointer(0);
    auto *outputTempData = bufferTempOutput.getWritePointer(0);
    auto *outputData = bufferOutput.getWritePointer(0);
    auto *overlapData = bufferOverlap.getWritePointer(0);

    while (numSamplesProcessed < numSamples) {
      const bool inputDataWasEmpty = (inputDataPos == 0);
      auto numSamplesToProcess =
          jmin(numSamples - numSamplesProcessed, blockSize - inputDataPos);

      FloatVectorOperations::copy(inputData + inputDataPos,
                                  input + numSamplesProcessed,
                                  static_cast<int>(numSamplesToProcess));

      auto *inputSegmentData =
          buffersInputSegments[currentSegment].getWritePointer(0);
      FloatVectorOperations::copy(inputSegmentData, inputData,
                                  static_cast<int>(fftSize));

      fftObject->performRealOnlyForwardTransform(inputSegmentData);
      prepareForConvolution(inputSegmentData);

      // Complex multiplication
      if (inputDataWasEmpty) {
        FloatVectorOperations::fill(outputTempData, 0,
                                    static_cast<int>(fftSize + 1));

        auto index = currentSegment;

        for (size_t i = 1; i < numSegments; ++i) {
          index += indexStep;

          if (index >= numInputSegments)
            index -= numInputSegments;

          convolutionProcessingAndAccumulate(
              buffersInputSegments[index].getWritePointer(0),
              buffersImpulseSegments[i].getWritePointer(0), outputTempData);
        }
      }

      FloatVectorOperations::copy(outputData, outputTempData,
                                  static_cast<int>(fftSize + 1));

      convolutionProcessingAndAccumulate(
          inputSegmentData, buffersImpulseSegments.front().getWritePointer(0),
          outputData);

      updateSymmetricFrequencyDomainData(outputData);
      fftObject->performRealOnlyInverseTransform(outputData);

      // Add overlap
      FloatVectorOperations::add(
          &output[numSamplesProcessed], &outputData[inputDataPos],
          &overlapData[inputDataPos], (int)numSamplesToProcess);

      // Input buffer full => Next block
      inputDataPos += numSamplesToProcess;

      if (inputDataPos == blockSize) {
        // Input buffer is empty again now
        FloatVectorOperations::fill(inputData, 0.0f, static_cast<int>(fftSize));

        inputDataPos = 0;

        // Extra step for segSize > blockSize
        FloatVectorOperations::add(&(outputData[blockSize]),
                                   &(overlapData[blockSize]),
                                   static_cast<int>(fftSize - 2 * blockSize));

        // Save the overlap
        FloatVectorOperations::copy(overlapData, &(outputData[blockSize]),
                                    static_cast<int>(fftSize - blockSize));

        currentSegment = (currentSegment > 0) ? (currentSegment - 1)
                                              : (numInputSegments - 1);
      }

      numSamplesProcessed += numSamplesToProcess;
    }
  }

  void processSamplesWithAddedLatency(const float *input, float *output,
                                      size_t numSamples) {
    // Overlap-add, zero latency convolution algorithm with uniform partitioning
    size_t numSamplesProcessed = 0;

    auto indexStep = numInputSegments / numSegments;

    auto *inputData = bufferInput.getWritePointer(0);
    auto *outputTempData = bufferTempOutput.getWritePointer(0);
    auto *outputData = bufferOutput.getWritePointer(0);
    auto *overlapData = bufferOverlap.getWritePointer(0);

    while (numSamplesProcessed < numSamples) {
      auto numSamplesToProcess =
          jmin(numSamples - numSamplesProcessed, blockSize - inputDataPos);

      FloatVectorOperations::copy(inputData + inputDataPos,
                                  input + numSamplesProcessed,
                                  static_cast<int>(numSamplesToProcess));

      FloatVectorOperations::copy(output + numSamplesProcessed,
                                  outputData + inputDataPos,
                                  static_cast<int>(numSamplesToProcess));

      numSamplesProcessed += numSamplesToProcess;
      inputDataPos += numSamplesToProcess;

      // processing itself when needed (with latency)
      if (inputDataPos == blockSize) {
        // Copy input data in input segment
        auto *inputSegmentData =
            buffersInputSegments[currentSegment].getWritePointer(0);
        FloatVectorOperations::copy(inputSegmentData, inputData,
                                    static_cast<int>(fftSize));

        fftObject->performRealOnlyForwardTransform(inputSegmentData);
        prepareForConvolution(inputSegmentData);

        // Complex multiplication
        FloatVectorOperations::fill(outputTempData, 0,
                                    static_cast<int>(fftSize + 1));

        auto index = currentSegment;

        for (size_t i = 1; i < numSegments; ++i) {
          index += indexStep;

          if (index >= numInputSegments)
            index -= numInputSegments;

          convolutionProcessingAndAccumulate(
              buffersInputSegments[index].getWritePointer(0),
              buffersImpulseSegments[i].getWritePointer(0), outputTempData);
        }

        FloatVectorOperations::copy(outputData, outputTempData,
                                    static_cast<int>(fftSize + 1));

        convolutionProcessingAndAccumulate(
            inputSegmentData, buffersImpulseSegments.front().getWritePointer(0),
            outputData);

        updateSymmetricFrequencyDomainData(outputData);
        fftObject->performRealOnlyInverseTransform(outputData);

        // Add overlap
        FloatVectorOperations::add(outputData, overlapData,
                                   static_cast<int>(blockSize));

        // Input buffer is empty again now
        FloatVectorOperations::fill(inputData, 0.0f, static_cast<int>(fftSize));

        // Extra step for segSize > blockSize
        FloatVectorOperations::add(&(outputData[blockSize]),
                                   &(overlapData[blockSize]),
                                   static_cast<int>(fftSize - 2 * blockSize));

        // Save the overlap
        FloatVectorOperations::copy(overlapData, &(outputData[blockSize]),
                                    static_cast<int>(fftSize - blockSize));

        currentSegment = (currentSegment > 0) ? (currentSegment - 1)
                                              : (numInputSegments - 1);

        inputDataPos = 0;
      }
    }
  }

  // After each FFT, this function is called to allow convolution to be
  // performed with only 4 SIMD functions calls.
  void prepareForConvolution(float *samples) noexcept {
    auto FFTSizeDiv2 = fftSize / 2;

    for (size_t i = 0; i < FFTSizeDiv2; i++)
      samples[i] = samples[i << 1];

    samples[FFTSizeDiv2] = 0;

    for (size_t i = 1; i < FFTSizeDiv2; i++)
      samples[i + FFTSizeDiv2] = -samples[((fftSize - i) << 1) + 1];
  }

  // Does the convolution operation itself only on half of the frequency domain
  // samples.
  void convolutionProcessingAndAccumulate(const float *input,
                                          const float *impulse, float *output) {
    auto FFTSizeDiv2 = fftSize / 2;

    FloatVectorOperations::addWithMultiply(output, input, impulse,
                                           static_cast<int>(FFTSizeDiv2));
    FloatVectorOperations::subtractWithMultiply(output, &(input[FFTSizeDiv2]),
                                                &(impulse[FFTSizeDiv2]),
                                                static_cast<int>(FFTSizeDiv2));

    FloatVectorOperations::addWithMultiply(&(output[FFTSizeDiv2]), input,
                                           &(impulse[FFTSizeDiv2]),
                                           static_cast<int>(FFTSizeDiv2));
    FloatVectorOperations::addWithMultiply(&(output[FFTSizeDiv2]),
                                           &(input[FFTSizeDiv2]), impulse,
                                           static_cast<int>(FFTSizeDiv2));

    output[fftSize] += input[fftSize] * impulse[fftSize];
  }

  // Undoes the re-organization of samples from the function
  // prepareForConvolution. Then takes the conjugate of the frequency domain
  // first half of samples to fill the second half, so that the inverse
  // transform will return real samples in the time domain.
  void updateSymmetricFrequencyDomainData(float *samples) noexcept {
    auto FFTSizeDiv2 = fftSize / 2;

    for (size_t i = 1; i < FFTSizeDiv2; i++) {
      samples[(fftSize - i) << 1] = samples[i];
      samples[((fftSize - i) << 1) + 1] = -samples[FFTSizeDiv2 + i];
    }

    samples[1] = 0.f;

    for (size_t i = 1; i < FFTSizeDiv2; i++) {
      samples[i << 1] = samples[(fftSize - i) << 1];
      samples[(i << 1) + 1] = -samples[((fftSize - i) << 1) + 1];
    }
  }

  //==============================================================================
  const size_t blockSize;
  const size_t fftSize;
  const std::unique_ptr<FFT> fftObject;
  const size_t numSegments;
  const size_t numInputSegments;
  size_t currentSegment = 0, inputDataPos = 0;

  AudioBuffer<float> bufferInput, bufferOutput, bufferTempOutput, bufferOverlap;
  std::vector<AudioBuffer<float>> buffersInputSegments, buffersImpulseSegments;
};

//==============================================================================
class MultichannelEngine {
public:
  MultichannelEngine(const AudioBuffer<float> &buf, int maxBlockSize,
                     int maxBufferSize, Convolution::NonUniform headSizeIn,
                     bool isZeroDelayIn)
      : tailBuffer(1, maxBlockSize), latency(isZeroDelayIn ? 0 : maxBufferSize),
        irSize(buf.getNumSamples()), blockSize(maxBlockSize),
        isZeroDelay(isZeroDelayIn) {
    constexpr auto numChannels = 2;

    const auto makeEngine = [&](int channel, int offset, int length,
                                uint32 thisBlockSize) {
      return std::make_unique<ConvolutionEngine>(
          buf.getReadPointer(jmin(buf.getNumChannels() - 1, channel), offset),
          length, static_cast<size_t>(thisBlockSize));
    };

    if (headSizeIn.headSizeInSamples == 0) {
      for (int i = 0; i < numChannels; ++i)
        head.emplace_back(makeEngine(i, 0, buf.getNumSamples(),
                                     static_cast<uint32>(maxBufferSize)));
    } else {
      const auto size = jmin(buf.getNumSamples(), headSizeIn.headSizeInSamples);

      for (int i = 0; i < numChannels; ++i)
        head.emplace_back(
            makeEngine(i, 0, size, static_cast<uint32>(maxBufferSize)));

      const auto tailBufferSize = static_cast<uint32>(
          headSizeIn.headSizeInSamples + (isZeroDelay ? 0 : maxBufferSize));

      if (size != buf.getNumSamples())
        for (int i = 0; i < numChannels; ++i)
          tail.emplace_back(
              makeEngine(i, size, buf.getNumSamples() - size, tailBufferSize));
    }
  }

  void reset() {
    for (const auto &e : head)
      e->reset();

    for (const auto &e : tail)
      e->reset();
  }

  void processSamples(const AudioBlock<const float> &input,
                      AudioBlock<float> &output) {
    const auto numChannels =
        jmin(head.size(), input.getNumChannels(), output.getNumChannels());
    const auto numSamples = jmin(input.getNumSamples(), output.getNumSamples());

    const AudioBlock<float> fullTailBlock(tailBuffer);
    const auto tailBlock = fullTailBlock.getSubBlock(0, (size_t)numSamples);

    const auto isUniform = tail.empty();

    for (size_t channel = 0; channel < numChannels; ++channel) {
      if (!isUniform)
        tail[channel]->processSamplesWithAddedLatency(
            input.getChannelPointer(channel), tailBlock.getChannelPointer(0),
            numSamples);

      if (isZeroDelay)
        head[channel]->processSamples(input.getChannelPointer(channel),
                                      output.getChannelPointer(channel),
                                      numSamples);
      else
        head[channel]->processSamplesWithAddedLatency(
            input.getChannelPointer(channel), output.getChannelPointer(channel),
            numSamples);

      if (!isUniform)
        output.getSingleChannelBlock(channel) += tailBlock;
    }

    const auto numOutputChannels = output.getNumChannels();

    for (auto i = numChannels; i < numOutputChannels; ++i)
      output.getSingleChannelBlock(i).copyFrom(output.getSingleChannelBlock(0));
  }

  int getIRSize() const noexcept { return irSize; }
  int getLatency() const noexcept { return latency; }
  int getBlockSize() const noexcept { return blockSize; }

private:
  std::vector<std::unique_ptr<ConvolutionEngine>> head, tail;
  AudioBuffer<float> tailBuffer;

  const int latency;
  const int irSize;
  const int blockSize;
  const bool isZeroDelay;
};

static AudioBuffer<float> fixNumChannels(const AudioBuffer<float> &buf,
                                         Convolution::Stereo stereo) {
  const auto numChannels =
      jmin(buf.getNumChannels(), stereo == Convolution::Stereo::yes ? 2 : 1);
  const auto numSamples = buf.getNumSamples();

  AudioBuffer<float> result(numChannels, buf.getNumSamples());

  for (auto channel = 0; channel != numChannels; ++channel)
    result.copyFrom(channel, 0, buf.getReadPointer(channel), numSamples);

  if (result.getNumSamples() == 0 || result.getNumChannels() == 0) {
    result.setSize(1, 1);
    result.setSample(0, 0, 1.0f);
  }

  return result;
}

static AudioBuffer<float> trimImpulseResponse(const AudioBuffer<float> &buf) {
  const auto thresholdTrim = Decibels::decibelsToGain(-80.0f);

  const auto numChannels = buf.getNumChannels();
  const auto numSamples = buf.getNumSamples();

  std::ptrdiff_t offsetBegin = numSamples;
  std::ptrdiff_t offsetEnd = numSamples;

  for (auto channel = 0; channel < numChannels; ++channel) {
    const auto indexAboveThreshold = [&](auto begin, auto end) {
      return std::distance(begin, std::find_if(begin, end, [&](float sample) {
                             return std::abs(sample) >= thresholdTrim;
                           }));
    };

    const auto channelBegin = buf.getReadPointer(channel);
    const auto channelEnd = channelBegin + numSamples;
    const auto itStart = indexAboveThreshold(channelBegin, channelEnd);
    const auto itEnd =
        indexAboveThreshold(std::make_reverse_iterator(channelEnd),
                            std::make_reverse_iterator(channelBegin));

    offsetBegin = jmin(offsetBegin, itStart);
    offsetEnd = jmin(offsetEnd, itEnd);
  }

  if (offsetBegin == numSamples) {
    auto result = AudioBuffer<float>(numChannels, 1);
    result.clear();
    return result;
  }

  const auto newLength =
      jmax(1, numSamples - static_cast<int>(offsetBegin + offsetEnd));

  AudioBuffer<float> result(numChannels, newLength);

  for (auto channel = 0; channel < numChannels; ++channel) {
    result.copyFrom(channel, 0,
                    buf.getReadPointer(channel, static_cast<int>(offsetBegin)),
                    result.getNumSamples());
  }

  return result;
}

static float calculateNormalisationFactor(float sumSquaredMagnitude) {
  if (sumSquaredMagnitude < 1e-8f)
    return 1.0f;

  return 0.125f / std::sqrt(sumSquaredMagnitude);
}

static void normaliseImpulseResponse(AudioBuffer<float> &buf) {
  const auto numChannels = buf.getNumChannels();
  const auto numSamples = buf.getNumSamples();
  const auto channelPtrs = buf.getArrayOfWritePointers();

  const auto maxSumSquaredMag = std::accumulate(
      channelPtrs, channelPtrs + numChannels, 0.0f,
      [numSamples](auto max, auto *channel) {
        return jmax(max, std::accumulate(channel, channel + numSamples, 0.0f,
                                         [](auto sum, auto samp) {
                                           return sum + (samp * samp);
                                         }));
      });

  const auto normalisationFactor =
      calculateNormalisationFactor(maxSumSquaredMag);

  std::for_each(channelPtrs, channelPtrs + numChannels,
                [normalisationFactor, numSamples](auto *channel) {
                  FloatVectorOperations::multiply(channel, normalisationFactor,
                                                  numSamples);
                });
}

static AudioBuffer<float> resampleImpulseResponse(const AudioBuffer<float> &buf,
                                                  const double srcSampleRate,
                                                  const double destSampleRate) {
  if (srcSampleRate == destSampleRate)
    return buf;

  const auto factorReading = srcSampleRate / destSampleRate;

  AudioBuffer<float> original = buf;
  MemoryAudioSource memorySource(original, false);
  ResamplingAudioSource resamplingSource(&memorySource, false,
                                         buf.getNumChannels());

  const auto finalSize =
      roundToInt(jmax(1.0, buf.getNumSamples() / factorReading));
  resamplingSource.setResamplingRatio(factorReading);
  resamplingSource.prepareToPlay(finalSize, srcSampleRate);

  AudioBuffer<float> result(buf.getNumChannels(), finalSize);
  resamplingSource.getNextAudioBlock({&result, 0, result.getNumSamples()});

  return result;
}

struct BufferWithSampleRate {
  BufferWithSampleRate() = default;

  BufferWithSampleRate(AudioBuffer<float> &&bufferIn, double sampleRateIn)
      : buffer(std::move(bufferIn)), sampleRate(sampleRateIn) {}

  AudioBuffer<float> buffer;
  double sampleRate = 0.0;
};

static BufferWithSampleRate
loadStreamToBuffer(std::unique_ptr<InputStream> stream, size_t maxLength) {
  AudioFormatManager manager;
  manager.registerBasicFormats();
  std::unique_ptr<AudioFormatReader> formatReader(
      manager.createReaderFor(std::move(stream)));

  if (formatReader == nullptr)
    return {};

  const auto fileLength = static_cast<size_t>(formatReader->lengthInSamples);
  const auto lengthToLoad =
      maxLength == 0 ? fileLength : jmin(maxLength, fileLength);

  BufferWithSampleRate result{
      {jlimit(1, 2, static_cast<int>(formatReader->numChannels)),
       static_cast<int>(lengthToLoad)},
      formatReader->sampleRate};

  formatReader->read(result.buffer.getArrayOfWritePointers(),
                     result.buffer.getNumChannels(), 0,
                     result.buffer.getNumSamples());

  return result;
}

// This class caches the data required to build a new convolution engine
// (in particular, impulse response data and a ProcessSpec).
// Calls to `setProcessSpec` and `setImpulseResponse` construct a
// new engine, which can be retrieved by calling `getEngine`.
class BlockingConvolutionEngineFactory {
public:
  BlockingConvolutionEngineFactory(Convolution::Latency requiredLatency,
                                   Convolution::NonUniform requiredHeadSize)
      : latency{(requiredLatency.latencyInSamples <= 0)
                    ? 0
                    : jmax(64,
                           nextPowerOfTwo(requiredLatency.latencyInSamples))},
        headSize{
            (requiredHeadSize.headSizeInSamples <= 0)
                ? 0
                : jmax(64, nextPowerOfTwo(requiredHeadSize.headSizeInSamples))},
        shouldBeZeroLatency(requiredLatency.latencyInSamples == 0) {}

  // It is safe to call this method simultaneously with other public
  // member functions.
  void setProcessSpec(const ProcessSpec &spec) {
    bool shouldRemakeEngine =
        processSpec.maximumBlockSize != spec.maximumBlockSize ||
        processSpec.sampleRate != spec.sampleRate ||
        processSpec.numChannels != spec.numChannels;
    processSpec = spec;

    if (shouldRemakeEngine) {
      engine = makeEngine();
    }
  }

  // It is safe to call this method simultaneously with other public
  // member functions.
  void setImpulseResponse(BufferWithSampleRate &&buf,
                          Convolution::Stereo stereo, Convolution::Trim trim,
                          Convolution::Normalise normalise) {
    wantsNormalise = normalise;
    originalSampleRate = buf.sampleRate;

    impulseResponse = [&] {
      auto corrected = fixNumChannels(buf.buffer, stereo);
      return trim == Convolution::Trim::yes ? trimImpulseResponse(corrected)
                                            : corrected;
    }();

    engine = makeEngine();
  }

  MultichannelEngine &getEngine() const {
    if (!engine) {
      throw std::runtime_error("Attempted to use Convolution without setting "
                               "an impulse response first.");
    }
    return *engine;
  }

private:
  std::unique_ptr<MultichannelEngine> makeEngine() {
    auto resampled = resampleImpulseResponse(
        impulseResponse, originalSampleRate, processSpec.sampleRate);

    if (wantsNormalise == Convolution::Normalise::yes)
      normaliseImpulseResponse(resampled);

    const auto currentLatency =
        jmax(processSpec.maximumBlockSize, (uint32)latency.latencyInSamples);
    const auto maxBufferSize =
        shouldBeZeroLatency ? static_cast<int>(processSpec.maximumBlockSize)
                            : nextPowerOfTwo(static_cast<int>(currentLatency));

    return std::make_unique<MultichannelEngine>(
        resampled, processSpec.maximumBlockSize, maxBufferSize, headSize,
        shouldBeZeroLatency);
  }

  static AudioBuffer<float> makeImpulseBuffer() {
    AudioBuffer<float> result(1, 1);
    result.setSample(0, 0, 1.0f);
    return result;
  }

  ProcessSpec processSpec{44100.0, 128, 2};
  AudioBuffer<float> impulseResponse = makeImpulseBuffer();
  double originalSampleRate = processSpec.sampleRate;
  Convolution::Normalise wantsNormalise = Convolution::Normalise::no;
  const Convolution::Latency latency;
  const Convolution::NonUniform headSize;
  const bool shouldBeZeroLatency;

  std::unique_ptr<MultichannelEngine> engine;
};

static void setImpulseResponse(BlockingConvolutionEngineFactory &factory,
                               const void *sourceData, size_t sourceDataSize,
                               Convolution::Stereo stereo,
                               Convolution::Trim trim, size_t size,
                               Convolution::Normalise normalise) {
  factory.setImpulseResponse(
      loadStreamToBuffer(std::make_unique<MemoryInputStream>(
                             sourceData, sourceDataSize, false),
                         size),
      stereo, trim, normalise);
}

static void setImpulseResponse(BlockingConvolutionEngineFactory &factory,
                               const File &fileImpulseResponse,
                               Convolution::Stereo stereo,
                               Convolution::Trim trim, size_t size,
                               Convolution::Normalise normalise) {
  factory.setImpulseResponse(
      loadStreamToBuffer(std::make_unique<FileInputStream>(fileImpulseResponse),
                         size),
      stereo, trim, normalise);
}

class BlockingConvolution::Impl {
public:
  Impl(Convolution::Latency requiredLatency,
       Convolution::NonUniform requiredHeadSize)
      : engineFactory(requiredLatency, requiredHeadSize) {}

  void reset() { engineFactory.getEngine().reset(); }

  void prepare(const ProcessSpec &spec) { engineFactory.setProcessSpec(spec); }

  void processSamples(const AudioBlock<const float> &input,
                      AudioBlock<float> &output) {
    engineFactory.getEngine().processSamples(input, output);
  }

  int getCurrentIRSize() const { return engineFactory.getEngine().getIRSize(); }

  int getLatency() const { return engineFactory.getEngine().getLatency(); }

  void loadImpulseResponse(AudioBuffer<float> &&buffer,
                           double originalSampleRate,
                           Convolution::Stereo stereo, Convolution::Trim trim,
                           Convolution::Normalise normalise) {
    engineFactory.setImpulseResponse({std::move(buffer), originalSampleRate},
                                     stereo, trim, normalise);
  }

  void loadImpulseResponse(const void *sourceData, size_t sourceDataSize,
                           Convolution::Stereo stereo, Convolution::Trim trim,
                           size_t size, Convolution::Normalise normalise) {

    setImpulseResponse(engineFactory, sourceData, sourceDataSize, stereo, trim,
                       size, normalise);
  }

  void loadImpulseResponse(const File &fileImpulseResponse,
                           Convolution::Stereo stereo, Convolution::Trim trim,
                           size_t size, Convolution::Normalise normalise) {
    setImpulseResponse(engineFactory, fileImpulseResponse, stereo, trim, size,
                       normalise);
  }

private:
  BlockingConvolutionEngineFactory engineFactory;
};

//==============================================================================
BlockingConvolution::BlockingConvolution()
    : BlockingConvolution(Convolution::Latency{0}) {}

BlockingConvolution::BlockingConvolution(
    const Convolution::Latency &requiredLatency)
    : BlockingConvolution(requiredLatency, {}) {}

BlockingConvolution::BlockingConvolution(
    const Convolution::NonUniform &nonUniform)
    : BlockingConvolution({}, nonUniform) {}

BlockingConvolution::BlockingConvolution(
    const Convolution::Latency &latency,
    const Convolution::NonUniform &nonUniform)
    : pimpl(std::make_unique<Impl>(latency, nonUniform)) {}

BlockingConvolution::~BlockingConvolution() noexcept = default;

void BlockingConvolution::loadImpulseResponse(
    const void *sourceData, size_t sourceDataSize, Convolution::Stereo stereo,
    Convolution::Trim trim, size_t size, Convolution::Normalise normalise) {
  pimpl->loadImpulseResponse(sourceData, sourceDataSize, stereo, trim, size,
                             normalise);
}

void BlockingConvolution::loadImpulseResponse(
    const File &fileImpulseResponse, Convolution::Stereo stereo,
    Convolution::Trim trim, size_t size, Convolution::Normalise normalise) {
  pimpl->loadImpulseResponse(fileImpulseResponse, stereo, trim, size,
                             normalise);
}

void BlockingConvolution::loadImpulseResponse(
    AudioBuffer<float> &&buffer, double originalSampleRate,
    Convolution::Stereo stereo, Convolution::Trim trim,
    Convolution::Normalise normalise) {
  pimpl->loadImpulseResponse(std::move(buffer), originalSampleRate, stereo,
                             trim, normalise);
}

void BlockingConvolution::prepare(const ProcessSpec &spec) {
  pimpl->prepare(spec);
  isActive = true;
}

void BlockingConvolution::reset() noexcept { pimpl->reset(); }

void BlockingConvolution::processSamples(const AudioBlock<const float> &input,
                                         AudioBlock<float> &output,
                                         bool isBypassed) noexcept {
  if (!isActive)
    return;

  jassert(input.getNumChannels() == output.getNumChannels());
  jassert(isPositiveAndBelow(
      input.getNumChannels(),
      static_cast<size_t>(3))); // only mono and stereo is supported

  pimpl->processSamples(input, output);
}

int BlockingConvolution::getCurrentIRSize() const {
  return pimpl->getCurrentIRSize();
}

int BlockingConvolution::getLatency() const { return pimpl->getLatency(); }

} // namespace dsp
} // namespace juce
