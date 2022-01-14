#include "./vendors/rubberband/rubberband/RubberBandStretcher.h"
#include "Plugin.h"

using namespace RubberBand;

namespace Pedalboard
{

  // TODO: Is this template needed?
  // template <typename DSPType>
  class RubberbandPlugin : public Plugin
  /*
  Base class for rubberband plugins.
  */
  {
  public:
    virtual ~RubberbandPlugin(){};

    // virtual void prepare(const juce::dsp::ProcessSpec &spec) = 0;

    void process(
        const juce::dsp::ProcessContextReplacing<float> &context) override final
    {
      if (rbPtr)
      {
        auto inBlock = context.getInputBlock();
        auto outBlock = context.getOutputBlock();

        // TODO: Is num samples the total number of frames or n_samples_per_channel * n_channels?
        auto len = inBlock.getNumSamples();
        auto numChannels = inBlock.getNumChannels();

        jassert(len == outBlock.getNumSamples());
        jassert(numChannels == outBlock.getNumChannels());

        // Have to find way to get all channel data?
        const float *inChannels[numChannels];
        float *outChannels[numChannels];
        for (size_t i = 0; i < numChannels; i++)
        {
          inChannels[i] = inBlock.getChannelPointer(i);
          outChannels[i] = outBlock.getChannelPointer(i);
        }

        // Rubberband expects all channel data with one float array per channel
        processSamples(inChannels, outChannels, len, numChans, false);
      }
    }

    void processSamples(const float *const *inBlock, float **outBlock, size_t samples, size_t numChannels, bool final)
    {
      // Push all of the input samples into RubberBand:
      rbPtr->process(inBlock, samples, false);
      
      // Figure out how many samples RubberBand is ready to give to us:
      int availableSamples = rbPtr->available();

      // ...but only actually ask Rubberband for at most the number of samples we can handle:
      int samplesToPull = samples;
      if (samplesToPull > availableSamples) samplesToPull = availableSamples;

      // If we don't have enough samples to fill a full buffer,
      // right-align the samples that we do have (i..e: start with silence).
      int missingSamples = samples - availableSamples;
      if (missingSamples > 0) {
        for (int c = 0; c < numChannels; c++) {
          // Clear the start of the buffer so that we start
          // the buffer with silence:
          for (int i = 0; i < missingSamples; i++) {
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

    void reset() override final
    {
      // dspBlock.reset();
      if (rbPtr)
      {
        rbPtr->reset();
      }
    }

    // RubberBandStretcher *getRb() { return rbPtr; }

  protected:
    // TODO: Do I need the DSP block?
    // DSPType dspBlock;
    RubberBandStretcher *rbPtr = nullptr;
  };
}; // pedalboard