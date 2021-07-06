#include "../JuceHeader.h"

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

namespace juce {
namespace dsp {

/**
    Performs stereo partitioned convolution of an input signal with an
    impulse response in the frequency domain, using the JUCE FFT class.

    This class provides some thread-safe functions to load impulse responses
    from audio files or memory on-the-fly without noticeable artefacts,
    performing resampling and trimming if necessary.

    The processing performed by this class is equivalent to the time domain
    convolution done in the FIRFilter class, with a FIRFilter::Coefficients
    object having the samples of the impulse response as its coefficients.
    However, in general it is more efficient to do frequency domain
    convolution when the size of the impulse response is 64 samples or
    greater.

    Note: The default operation of this class uses zero latency and a uniform
    partitioned algorithm. If the impulse response size is large, or if the
    algorithm is too CPU intensive, it is possible to use either a fixed
    latency version of the algorithm, or a simple non-uniform partitioned
    convolution algorithm.

    @see FIRFilter, FIRFilter::Coefficients, FFT

    @tags{DSP}
*/
class JUCE_API BlockingConvolution {
public:
  //==============================================================================
  /** Initialises an object for performing convolution in the frequency domain.
   */
  BlockingConvolution();

  /** Contains configuration information for a convolution with a fixed latency.
   */
  struct Latency {
    int latencyInSamples;
  };

  /** Initialises an object for performing convolution with a fixed latency.

      If the requested latency is zero, the actual latency will also be zero.
      For requested latencies greater than zero, the actual latency will
      always at least as large as the requested latency. Using a fixed
      non-zero latency can reduce the CPU consumption of the convolution
      algorithm.

      @param requiredLatency        the minimum latency
  */
  explicit BlockingConvolution(const Convolution::Latency &requiredLatency);

  /** Initialises an object for performing convolution in the frequency domain
      using a non-uniform partitioned algorithm.

      A requiredHeadSize of 256 samples or greater will improve the
      efficiency of the processing for IR sizes of 4096 samples or greater
      (recommended for reverberation IRs).

      @param requiredHeadSize       the head IR size for two stage non-uniform
                                    partitioned convolution
   */
  explicit BlockingConvolution(const Convolution::NonUniform &requiredHeadSize);

  ~BlockingConvolution() noexcept;

  //==============================================================================
  /** Must be called before loading any impulse response. This provides the
      maximumBufferSize and the sample rate required for any resampling.
  */
  void prepare(const ProcessSpec &);

  /** Resets the processing pipeline ready to start a new stream of data. */
  void reset() noexcept;

  /** Performs the filter operation on the given set of samples with optional
      stereo processing.
  */
  template <typename ProcessContext,
            std::enable_if_t<
                std::is_same<typename ProcessContext::SampleType, float>::value,
                int> = 0>
  void process(const ProcessContext &context) noexcept {
    processSamples(context.getInputBlock(), context.getOutputBlock(),
                   context.isBypassed);
  }

  //==============================================================================
  /** This function loads an impulse response audio file from memory, added in a
      JUCE project with the Projucer as binary data. It can load any of the
     audio formats registered in JUCE, and performs some resampling and
     pre-processing as well if needed.

      Note: Don't try to use this function on float samples, since the data is
      expected to be an audio file in its binary format. Be sure that the
     original data remains constant throughout the lifetime of the Convolution
     object, as the loading process will happen on a background thread once this
     function has returned.

      @param sourceData               the block of data to use as the stream's
     source
      @param sourceDataSize           the number of bytes in the source data
     block
      @param isStereo                 selects either stereo or mono
      @param requiresTrimming         optionally trim the start and the end of
     the impulse response
      @param size                     the expected size for the impulse response
     after loading, can be set to 0 to requesting the original impulse response
     size
      @param requiresNormalisation    optionally normalise the impulse response
     amplitude
  */
  void loadImpulseResponse(const void *sourceData, size_t sourceDataSize,
                           Convolution::Stereo isStereo,
                           Convolution::Trim requiresTrimming, size_t size,
                           Convolution::Normalise requiresNormalisation =
                               Convolution::Normalise::yes);

  /** This function loads an impulse response from an audio file. It can load
     any of the audio formats registered in JUCE, and performs some resampling
     and pre-processing as well if needed.

      @param fileImpulseResponse      the location of the audio file
      @param isStereo                 selects either stereo or mono
      @param requiresTrimming         optionally trim the start and the end of
     the impulse response
      @param size                     the expected size for the impulse response
     after loading, can be set to 0 to requesting the original impulse response
     size
      @param requiresNormalisation    optionally normalise the impulse response
     amplitude
  */
  void loadImpulseResponse(const File &fileImpulseResponse,
                           Convolution::Stereo isStereo,
                           Convolution::Trim requiresTrimming, size_t size,
                           Convolution::Normalise requiresNormalisation =
                               Convolution::Normalise::yes);

  /** This function loads an impulse response from an audio buffer.
      To avoid memory allocation on the audio thread, this function takes
      ownership of the buffer passed in.

      If calling this function during processing, make sure that the buffer is
      not allocated on the audio thread (be careful of accidental copies!).
      If you need to pass arbitrary/generated buffers it's recommended to
      create these buffers on a separate thread and to use some wait-free
      construct (a lock-free queue or a SpinLock/GenericScopedTryLock
     combination) to transfer ownership to the audio thread without allocating.

      @param buffer                   the AudioBuffer to use
      @param bufferSampleRate         the sampleRate of the data in the
     AudioBuffer
      @param isStereo                 selects either stereo or mono
      @param requiresTrimming         optionally trim the start and the end of
     the impulse response
      @param requiresNormalisation    optionally normalise the impulse response
     amplitude
  */
  void loadImpulseResponse(AudioBuffer<float> &&buffer, double bufferSampleRate,
                           Convolution::Stereo isStereo,
                           Convolution::Trim requiresTrimming,
                           Convolution::Normalise requiresNormalisation);

  /** This function returns the size of the current IR in samples. */
  int getCurrentIRSize() const;

  /** This function returns the current latency of the process in samples.

      Note: This is the latency of the convolution engine, not the latency
      associated with the current impulse response choice that has to be
      considered separately (linear phase filters, for example).
  */
  int getLatency() const;

private:
  //==============================================================================
  BlockingConvolution(const Convolution::Latency &,
                      const Convolution::NonUniform &);

  void processSamples(const AudioBlock<const float> &, AudioBlock<float> &,
                      bool isBypassed) noexcept;

  //==============================================================================
  class Impl;
  std::unique_ptr<Impl> pimpl;

  //==============================================================================
  bool isActive = false;

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockingConvolution)
};

} // namespace dsp
} // namespace juce
