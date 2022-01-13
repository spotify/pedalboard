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
      // This part might be the same across all rubberband plugins?
      if (rbPtr)
      {
        // Is this the right way to get the next input block?
        // Do I have to shift something?
        // size_t sampleFrames = 0; // How do I compute this? Is there a value somewhere

        auto inBlock = context.getInputBlock();
        auto outBlock = context.getOutputBlock();

        // TODO: Is num samples the total number of frames or n_samples_per_channel * n_channels
        auto len = inBlock.getNumSamples();
        auto numChans = inBlock.getNumChannels();

        jassert(len == outBlock.getNumSamples());
        jassert(numChans == outBlock.getNumChannels());

        // Have to find way to get all channel data?
        const float *inChannels[numChans];
        float *outChannels[numChans];
        for (size_t i = 0; i < numChans; i++)
        {
          inChannels[i] = inBlock.getChannelPointer(i);
          outChannels[i] = outBlock.getChannelPointer(i);
          std::cout << "input channel " << i << " = " << inChannels[i] << "\n";
          std::cout << "output channel " << i << " = " << outChannels[i] << "\n";
        }

        // Rubberband expects all channel data with one float array per channel
        processSamples(inChannels, outChannels, len, false);
      }
    }

    void processSamples(const float *const *inBlock, float *const *outBlock, size_t samples, bool final)
    {
      rbPtr->process(inBlock, samples, false); // Impact of not setting final to true?
      std::cout << rbPtr->getSamplesRequired() << "\n";
      size_t samplesRetrieved = 0;
      size_t samplesAvailable = 0;
      int count = 0;
      while (count < 10) // is this correct to hang here for each block?
      {
        samplesAvailable = rbPtr->available();
        std::cout << samplesAvailable << "\n";
        if (samplesAvailable)
        {
          samplesRetrieved += rbPtr->retrieve(outBlock, samplesAvailable);
        }
        count++;
      }
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