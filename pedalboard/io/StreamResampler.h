/*
 * pedalboard
 * Copyright 2022 Spotify AB
 *
 * Licensed under the GNU Public License, Version 3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.gnu.org/licenses/gpl-3.0.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "../BufferUtils.h"
#include "../plugin_templates/Resample.h"
#include <mutex>

namespace Pedalboard {

template <typename SampleType = float> class StreamResampler {
public:
  StreamResampler(double sourceSampleRate, double targetSampleRate,
                  int numChannels, ResamplingQuality quality)
      : sourceSampleRate(sourceSampleRate), targetSampleRate(targetSampleRate),
        numChannels(numChannels), quality(quality) {
    overflowSamples.resize(numChannels);
    resamplers.resize(numChannels);

    for (int i = 0; i < numChannels; i++) {
      resamplers[i].setQuality(quality);
      resamplers[i].reset();
    }

    resamplerRatio = sourceSampleRate / targetSampleRate;
    inputLatency = resamplers[0].getBaseLatency();
    outputLatency = inputLatency / resamplerRatio;

    outputSamplesToSkip = outputLatency;
  }

  juce::AudioBuffer<SampleType>
  process(std::optional<juce::AudioBuffer<SampleType>> &_input,
          double maxSamplesToReturn = 1e40) {
    if (_input && _input->getNumChannels() != numChannels) {
      throw std::domain_error(
          "Expected " + std::to_string(numChannels) +
          "-channel input, but was provided a buffer with " +
          std::to_string(_input->getNumChannels()) + " channels and " +
          std::to_string(_input->getNumSamples()) + " samples.");
    }

    std::scoped_lock lock(mutex);

    juce::AudioBuffer<SampleType> input;
    bool isFlushing = false;
    int numNewInputSamples = 0;

    if (_input) {
      input = prependWith(*_input, overflowSamples);
      numNewInputSamples = input.getNumSamples();
    } else {
      isFlushing = true;
      int samplesToFlush = inputLatency;

      input = juce::AudioBuffer<float>(numChannels, samplesToFlush);
      inputSamplesBufferedInResampler = 0;
      input.clear();
      input = prependWith(input, overflowSamples);
    }

    double expectedResampledSamples =
        std::max(0.0, ((totalSamplesInput + input.getNumSamples()) *
                       targetSampleRate / sourceSampleRate) -
                          totalSamplesOutput);

    // TODO: Don't copy the entire input buffer multiple times here!
    juce::AudioBuffer<SampleType> output(input.getNumChannels(),
                                         (int)expectedResampledSamples);

    for (size_t c = 0; c < input.getNumChannels(); c++) {
      if (input.getNumSamples() > 0) {
        long long inputSamplesConsumed = resamplers[c].process(
            resamplerRatio, input.getReadPointer(c), output.getWritePointer(c),
            (int)expectedResampledSamples);

        if (c == 0) {
          if (!isFlushing) {
            totalSamplesInput += inputSamplesConsumed;
          }
          totalSamplesOutput += (int)expectedResampledSamples;
        }

        if (!isFlushing) {
          for (int i = inputSamplesConsumed; i < input.getNumSamples(); i++) {
            overflowSamples[c].push_back(input.getReadPointer(c)[i]);
          }
          if (c == 0) {
            inputSamplesBufferedInResampler += inputSamplesConsumed;
            if (inputSamplesBufferedInResampler > inputLatency) {
              inputSamplesBufferedInResampler = inputLatency;
            }
          }
        }
      }
    }

    // Chop off the first _n_ samples if necessary:
    if (outputSamplesToSkip > 0) {
      long long intOutputSamplesToSkip = (int)std::round(outputSamplesToSkip);
      if (intOutputSamplesToSkip) {
        outputSamplesToSkip -=
            std::min(intOutputSamplesToSkip, (long long)output.getNumSamples());

        int newNumOutputSamples =
            output.getNumSamples() - intOutputSamplesToSkip;
        if (newNumOutputSamples <= 0) {
          if (isFlushing) {
            reset_unlocked();
          }
          return juce::AudioBuffer<SampleType>(input.getNumChannels(), 0);
        }

        juce::AudioBuffer<SampleType> choppedOutput(input.getNumChannels(),
                                                    newNumOutputSamples);
        for (int c = 0; c < numChannels; c++) {
          choppedOutput.copyFrom(c, 0, output, c, intOutputSamplesToSkip,
                                 newNumOutputSamples);
        }

        if (isFlushing) {
          reset_unlocked();
        }
        return choppedOutput;
      }
    }

    if (isFlushing) {
      reset_unlocked();
    }

    return output;
  }

  void reset() {
    std::scoped_lock lock(mutex);
    reset_unlocked();
  }

  void reset_unlocked() {
    for (auto &r : resamplers) {
      r.reset();
    }

    inputSamplesBufferedInResampler = 0;
    outputSamplesToSkip = outputLatency;
    for (auto &overflowBuffer : overflowSamples) {
      overflowBuffer.clear();
    }

    totalSamplesInput = 0;
    totalSamplesOutput = 0;
  }

  int getNumChannels() const { return numChannels; }
  double getSourceSampleRate() const { return sourceSampleRate; }
  double getTargetSampleRate() const { return targetSampleRate; }
  ResamplingQuality getQuality() const { return quality; }

  double getInputLatency() const { return inputLatency; }
  double getOutputLatency() const { return outputLatency; }

  int getBufferedInputSamples() const {
    return inputSamplesBufferedInResampler;
  }

  // TODO: Rename me!
  int getOverflowSamples() const { return overflowSamples[0].size(); }

  /**
   * Advance the internal state of this resampler, as if the given
   * number of silent samples had been provided.
   *
   * Note that this method will only affect the sub-sample position stored by
   * the resampler, but will not clear all of the samples buffered internally.
   */
  long long advanceResamplerState(long long numOutputSamples) {
    double newSubSamplePos = 1.0;
    long long numOutputSamplesToProduce = numOutputSamples;

    long long numInputSamplesUsed = 0;

    static const bool USE_CONSTANT_TIME_CALCULATION = false;
    if (USE_CONSTANT_TIME_CALCULATION) {
      /**
       * NOTE(psobot): This calculation is faster than the below (as it runs in
       * constant time) _but_ due to floating point accumulation errors, this
       * produces slightly different output than the `while` loops below. This
       * would mean that users who call seek() on a resampled stream would end
       * up with a very slightly different copy of the stream. This
       * nondeterminism is not worth the speedup.
       *
       * The probable way to fix this is to rewrite juce::GenericInterpolator to
       * use two `long long` variables to track the subsample position, rather
       * than relying on a single `double` whose precision errors can accumulate
       * over time. That is not a refactor I want to start at 4:44pm on a
       * Friday.
       */

      double numInputSamplesNeeded =
          std::max(0LL, numOutputSamplesToProduce - 1) * resamplerRatio;
      numInputSamplesUsed = std::ceil(numInputSamplesNeeded);
      newSubSamplePos = numInputSamplesNeeded - numInputSamplesUsed + 1;

      if (numOutputSamplesToProduce) {
        while (newSubSamplePos >= 1.0) {
          numInputSamplesUsed++;
          newSubSamplePos -= 1.0;
        }
        newSubSamplePos += resamplerRatio;
      }
    } else {
      while (numOutputSamplesToProduce > 0) {
        while (newSubSamplePos >= 1.0) {
          numInputSamplesUsed++;
          newSubSamplePos -= 1.0;
        }

        newSubSamplePos += resamplerRatio;
        --numOutputSamplesToProduce;
      }
    }

    float zero = 0.0;
    for (auto &resampler : resamplers) {
      // This effectively sets the new subsample position:
      resampler.process(newSubSamplePos, &zero, &zero, 1);
    }

    totalSamplesOutput += numOutputSamples;
    totalSamplesInput += numInputSamplesUsed;

    return numInputSamplesUsed;
  }

  void setLastChannelLayout(ChannelLayout last) { lastChannelLayout = last; }

  std::optional<ChannelLayout> getLastChannelLayout() const {
    return lastChannelLayout;
  }

private:
  juce::AudioBuffer<SampleType>
  prependWith(juce::AudioBuffer<SampleType> &input,
              std::vector<std::vector<SampleType>> &toPrepend) {
    int prependSize = toPrepend[0].size();
    juce::AudioBuffer<SampleType> output(input.getNumChannels(),
                                         input.getNumSamples() + prependSize);

    for (int c = 0; c < input.getNumChannels(); c++) {
      SampleType *channelPointer = output.getWritePointer(c);
      for (int i = 0; i < prependSize; i++) {
        channelPointer[i] = toPrepend[c][i];
      }
      toPrepend[c].clear();

      output.copyFrom(c, prependSize, input, c, 0, input.getNumSamples());
    }

    return output;
  }

  double sourceSampleRate;
  double targetSampleRate;
  ResamplingQuality quality;
  std::vector<VariableQualityResampler> resamplers;

  double resamplerRatio = 1.0;
  std::vector<std::vector<SampleType>> overflowSamples;
  double inputLatency = 0;
  double outputLatency = 0;

  long long totalSamplesInput = 0;
  long long totalSamplesOutput = 0;

  int inputSamplesBufferedInResampler = 0;
  int numChannels = 1;
  double outputSamplesToSkip = 0.0;

  // A mutex to gate access to this processor, as its internals may not be
  // thread-safe.
  std::mutex mutex;

  std::optional<ChannelLayout> lastChannelLayout = {};
};

inline void init_stream_resampler(py::module &m) {
  py::class_<StreamResampler<float>, std::shared_ptr<StreamResampler<float>>>
      resampler(
          m, "StreamResampler",
          "A streaming resampler that can change the sample rate of multiple "
          "chunks of audio in series, while using constant memory.\n\nFor a "
          "resampling plug-in that can be used in :class:`Pedalboard` objects, "
          "see :class:`pedalboard.Resample`.\n\n*Introduced in v0.6.0.*");

  resampler.def(
      py::init([](float sourceSampleRate, float targetSampleRate,
                  int numChannels, ResamplingQuality quality) {
        return std::make_unique<StreamResampler<float>>(
            sourceSampleRate, targetSampleRate, numChannels, quality);
      }),
      py::arg("source_sample_rate"), py::arg("target_sample_rate"),
      py::arg("num_channels"),
      py::arg("quality") = ResamplingQuality::WindowedSinc32,
      "Create a new StreamResampler, capable of resampling a "
      "potentially-unbounded audio stream with a constant amount of memory. "
      "The source sample rate, target sample rate, quality, or number of "
      "channels cannot be changed once the resampler is instantiated.");

  resampler.def("__repr__", [](const StreamResampler<float> &resampler) {
    std::ostringstream ss;
    ss << "<pedalboard.io.StreamResampler";

    ss << " source_sample_rate=" << resampler.getSourceSampleRate();
    ss << " target_sample_rate=" << resampler.getTargetSampleRate();
    ss << " num_channels=" << resampler.getNumChannels();
    ss << " quality=";

    switch (resampler.getQuality()) {
    case ResamplingQuality::ZeroOrderHold:
      ss << "ZeroOrderHold";
      break;
    case ResamplingQuality::Linear:
      ss << "Linear";
      break;
    case ResamplingQuality::CatmullRom:
      ss << "CatmullRom";
      break;
    case ResamplingQuality::Lagrange:
      ss << "Lagrange";
      break;
    case ResamplingQuality::WindowedSinc:
      ss << "WindowedSinc";
      break;
    default:
      ss << "unknown";
      break;
    }

    ss << " at " << &resampler;
    ss << ">";
    return ss.str();
  });

  resampler.def(
      "process",
      [](StreamResampler<float> &resampler,
         std::optional<py::array_t<float, py::array::c_style>> input) {
        std::optional<juce::AudioBuffer<float>> inputBuffer;
        if (input) {
          std::optional<ChannelLayout> layout =
              resampler.getLastChannelLayout();
          if (!layout) {
            try {
              layout = detectChannelLayout(*input);
              resampler.setLastChannelLayout(*layout);
            } catch (...) {
              // Use the last cached layout.
            }
          }
          inputBuffer = convertPyArrayIntoJuceBuffer(*input, *layout);
        }

        juce::AudioBuffer<float> output;
        {
          py::gil_scoped_release release;
          output = resampler.process(inputBuffer);
        }

        return copyJuceBufferIntoPyArray(output,
                                         *resampler.getLastChannelLayout(), 0);
      },
      py::arg("input") = py::none(),
      "Resample a 32-bit floating-point audio buffer. The returned buffer may "
      "be smaller than the provided buffer depending on the quality method "
      "used. Call :meth:`process()` without any arguments to flush the "
      "internal buffers and return all remaining audio.");

  resampler.def("reset", &StreamResampler<float>::reset,
                "Used to reset the internal state of this resampler. Call this "
                "method when resampling a new audio stream to prevent audio "
                "from leaking between streams.");

  resampler.def_property_readonly("num_channels",
                                  &StreamResampler<float>::getNumChannels,
                                  "The number of channels expected to be "
                                  "passed in every call to :meth:`process()`.");
  resampler.def_property_readonly(
      "source_sample_rate", &StreamResampler<float>::getSourceSampleRate,
      "The source sample rate of the input audio that this resampler expects "
      "to be passed to :meth:`process()`.");

  resampler.def_property_readonly(
      "target_sample_rate", &StreamResampler<float>::getTargetSampleRate,
      "The sample rate of the audio that this resampler will return from "
      ":meth:`process()`.");

  resampler.def_property_readonly(
      "quality", &StreamResampler<float>::getQuality,
      "The resampling algorithm used by this resampler.");

  resampler.def_property_readonly(
      "input_latency", &StreamResampler<float>::getInputLatency,
      "The number of samples (in the input sample rate) that must be supplied "
      "before this resampler will begin returning output.");
}

} // namespace Pedalboard