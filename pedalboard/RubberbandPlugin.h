#include "../vendors/rubberband/rubberband/RubberBandStretcher.h"
#include "Plugin.h"

using namespace RubberBand;

namespace Pedalboard
{
  class RubberbandPlugin : public Plugin
  /*
Base class for rubberband plugins.
*/
  {
  public:
    virtual ~RubberbandPlugin(){};

    void process(
        const juce::dsp::ProcessContextReplacing<float> &context) override final
    {
      if (rbPtr)
      {
        auto inBlock = context.getInputBlock();
        auto outBlock = context.getOutputBlock();

        auto len = inBlock.getNumSamples();
        auto numChannels = inBlock.getNumChannels();

        jassert(len == outBlock.getNumSamples());
        jassert(numChannels == outBlock.getNumChannels());

        const float *inChannels[numChannels];
        float *outChannels[numChannels];
        for (size_t i = 0; i < numChannels; i++)
        {
          inChannels[i] = inBlock.getChannelPointer(i);
          outChannels[i] = outBlock.getChannelPointer(i);
        }

        // Rubberband expects all channel data with one float array per channel
        processSamples(inChannels, outChannels, len, numChannels);
      }
    }

    void reset() override final
    {
      if (rbPtr)
      {
        rbPtr->reset();
      }
    }

  private:
    void processSamples(const float *const *inBlock, float **outBlock,
                        size_t samples, size_t numChannels)
    {
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
      if (missingSamples > 0)
      {
        for (size_t c = 0; c < numChannels; c++)
        {
          // Clear the start of the buffer so that we start
          // the buffer with silence:
          for (size_t i = 0; i < missingSamples; i++)
          {
            outBlock[c][i] = 0.0;
          }

          // Move the output buffer pointer forward so that
          // RubberBandStretcher::retrieve(...) places its
          // output at the end of the buffer:
          outBlock[c] += missingSamples;
        }
      }

      // Pull the next audio data out of Rubberband:
      rbPtr->retrieve(outBlock, samplesToPull);
    }

  protected:
    std::unique_ptr<RubberBandStretcher> rbPtr;
  };
}; // namespace Pedalboard
