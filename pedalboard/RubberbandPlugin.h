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

#include "../vendors/rubberband/rubberband/RubberBandStretcher.h"
#include "Plugin.h"

using namespace RubberBand;

namespace Pedalboard {

/*
 * Base class for all Rubber Band-derived plugins.
 */
class RubberbandPlugin : public Plugin {
public:
  virtual ~RubberbandPlugin(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       spec.numChannels != lastSpec.numChannels;

    if (!rbPtr || specChanged) {
      auto stretcherOptions = RubberBandStretcher::OptionProcessRealTime |
                              RubberBandStretcher::OptionThreadingNever |
                              RubberBandStretcher::OptionChannelsTogether |
                              RubberBandStretcher::OptionPitchHighQuality;
      rbPtr = std::make_unique<RubberBandStretcher>(
          spec.sampleRate, spec.numChannels, stretcherOptions);
      rbPtr->setMaxProcessSize(spec.maximumBlockSize);

      lastSpec = spec;
      reset();
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    if (rbPtr) {
      auto inBlock = context.getInputBlock();
      auto outBlock = context.getOutputBlock();

      auto len = inBlock.getNumSamples();
      auto numChannels = inBlock.getNumChannels();

      jassert(len == outBlock.getNumSamples());
      jassert(numChannels == outBlock.getNumChannels());

      const float **inChannels =
          (const float **)alloca(numChannels * sizeof(float *));
      float **outChannels = (float **)alloca(numChannels * sizeof(float *));

      for (size_t i = 0; i < numChannels; i++) {
        inChannels[i] = inBlock.getChannelPointer(i);
        outChannels[i] = outBlock.getChannelPointer(i);
      }

      // Rubberband expects all channel data with one float array per channel
      return processSamples(inChannels, outChannels, len, numChannels);
    }
    return 0;
  }

  void reset() override final {
    if (rbPtr) {
      rbPtr->reset();
    }
  }

  RubberBandStretcher &getStretcher() { return *rbPtr; }

  virtual int getLatencyHint() override {
    if (!rbPtr)
      return 0;

    initialSamplesRequired =
        std::max(initialSamplesRequired,
                 (int)(rbPtr->getSamplesRequired() + rbPtr->getLatency() +
                       lastSpec.maximumBlockSize));

    return initialSamplesRequired;
  }

private:
  int processSamples(const float *const *inBlock, float **outBlock,
                     size_t samples, size_t numChannels) {
    // Push all of the input samples into RubberBand:
    rbPtr->process(inBlock, samples, false);

    // Figure out how many samples RubberBand is ready to give to us:
    int availableSamples = rbPtr->available();

    // ...but only actually ask Rubberband for at most the number of samples we
    // can handle:
    int samplesToPull = samples;
    if (samplesToPull > availableSamples)
      samplesToPull = availableSamples;

    // If we don't have enough samples to fill a full buffer,
    // right-align the samples that we do have (i..e: start with silence).
    int missingSamples = samples - availableSamples;
    if (missingSamples > 0) {
      for (size_t c = 0; c < numChannels; c++) {
        // Clear the start of the buffer so that we start
        // the buffer with silence:
        std::fill_n(outBlock[c], missingSamples, 0.0);

        // Move the output buffer pointer forward so that
        // RubberBandStretcher::retrieve(...) places its
        // output at the end of the buffer:
        outBlock[c] += missingSamples;
      }
    }

    // Pull the next audio data out of Rubberband:
    return rbPtr->retrieve(outBlock, samplesToPull);
  }

protected:
  std::unique_ptr<RubberBandStretcher> rbPtr;
  int initialSamplesRequired = 0;
};
}; // namespace Pedalboard
