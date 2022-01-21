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
    if (!rbPtr) {
      throw std::runtime_error("Rubber Band plugin failed to instantiate.");
    }

    auto ioBlock = context.getOutputBlock();
    auto numChannels = ioBlock.getNumChannels();

    if (numChannels > MAX_CHANNEL_COUNT) {
      throw std::runtime_error(
          "Pitch shifting or time stretching plugins support a maximum of " +
          std::to_string(MAX_CHANNEL_COUNT) + " channels.");
    }

    float *ioChannels[MAX_CHANNEL_COUNT] = {};

    // for (size_t i = 0; i < numChannels; i++) {
    //   ioChannels[i] = ioBlock.getChannelPointer(i);
    // }

    // Push all of the input samples we have into RubberBand:
    // rbPtr->process(ioChannels, ioBlock.getNumSamples(), false);
    // printf("Pushed %d samples into Rubber Band.\n", ioBlock.getNumSamples());
    // int availableSamples = rbPtr->available();

    int samplesProcessed = 0;
    int samplesWritten = 0;
    while (samplesProcessed < ioBlock.getNumSamples() || rbPtr->available()) {
      int samplesRequired = rbPtr->getSamplesRequired();
      int inputChunkLength = std::min(
          (int)(ioBlock.getNumSamples() - samplesProcessed), samplesRequired);

      for (size_t i = 0; i < numChannels; i++) {
        ioChannels[i] = ioBlock.getChannelPointer(i) + samplesProcessed;
      }

      rbPtr->process(ioChannels, inputChunkLength, false);
      samplesProcessed += inputChunkLength;

      int samplesAvailable = rbPtr->available();
      int freeSpace = ioBlock.getNumSamples() - samplesWritten;
      int outputChunkLength = std::min(freeSpace, samplesAvailable);

      // Avoid overwriting input that hasn't yet been passed to Rubber Band:
      if (samplesWritten + outputChunkLength > samplesProcessed) {
        outputChunkLength = samplesProcessed - samplesWritten;
      }

      for (size_t i = 0; i < numChannels; i++) {
        ioChannels[i] = ioBlock.getChannelPointer(i) + samplesWritten;
      }
      samplesWritten += rbPtr->retrieve(ioChannels, outputChunkLength);
      if (samplesWritten == ioBlock.getNumSamples())
        break;
    }

    if (samplesWritten > 0 && samplesWritten < ioBlock.getNumSamples()) {
      // Right-align the output samples in the buffer:
      int offset = ioBlock.getNumSamples() - samplesWritten;
      for (size_t i = 0; i < numChannels; i++) {
        float *channelBufferSource = ioBlock.getChannelPointer(i);
        float *channelBufferDestination = channelBufferSource + offset;
        std::memmove((char *)channelBufferDestination,
                     (char *)channelBufferSource,
                     sizeof(float) * samplesWritten);
      }
    }

    return samplesWritten;

    // Don't produce any output for this input if RubberBand isn't ready.
    // We can do this here because RubberBand buffers audio internally;
    // this might not be a safe technique to use with other plugins, as
    // their output sample buffers may overflow if passed more than
    // maximumBlockSize.

    // if (rbPtr->available() < (int) ioBlock.getNumSamples() + getLatency()) {
    //   printf("Need %d samples, but Rubber Band only had %d samples
    //   available.\n", ioBlock.getNumSamples() + getLatency(),
    //   rbPtr->available()); return 0;
    // } else {
    //   // Pull the next chunk of audio data out of RubberBand:
    //   int returned = rbPtr->retrieve(ioChannels, ioBlock.getNumSamples());
    //   printf("Sent %d samples into Rubber Band, and took %d samples back
    //   out.\n", ioBlock.getNumSamples(), returned); return returned;
    // }

    // // ...but only actually ask Rubberband for at most the number of samples
    // we
    // // can handle:
    // int samplesToPull = ioBlock.getNumSamples();
    // if (samplesToPull > availableSamples)
    //   samplesToPull = availableSamples;

    // // If we don't have enough samples to fill a full buffer,
    // // right-align the samples that we do have (i..e: start with silence).
    // int missingSamples = ioBlock.getNumSamples() - availableSamples;
    // if (missingSamples > 0) {
    //   for (size_t c = 0; c < numChannels; c++) {
    //     // Clear the start of the buffer so that we start
    //     // the buffer with silence:
    //     std::fill_n(ioChannels[c], missingSamples, 0.0);

    //     // Move the output buffer pointer forward so that
    //     // RubberBandStretcher::retrieve(...) places its
    //     // output at the end of the buffer:
    //     ioChannels[c] += missingSamples;
    //   }
    // }

    // // Pull the next audio data out of Rubberband:
    // int pulled = rbPtr->retrieve(ioChannels, samplesToPull);
    // printf("Pulled %d samples out of Rubber Band (%d were available).\n",
    // pulled, availableSamples); return pulled;
  }

  void reset() override final {
    if (rbPtr) {
      rbPtr->reset();
    }
    initialSamplesRequired = 0;
  }

  virtual int getLatencyHint() override {
    if (!rbPtr)
      return 0;

    initialSamplesRequired =
        std::max(initialSamplesRequired,
                 (int)(rbPtr->getSamplesRequired() + rbPtr->getLatency() + lastSpec.maximumBlockSize));

    return initialSamplesRequired;
  }

protected:
  std::unique_ptr<RubberBandStretcher> rbPtr;
  int initialSamplesRequired = 0;
  static constexpr int MAX_CHANNEL_COUNT = 8;
};
}; // namespace Pedalboard
