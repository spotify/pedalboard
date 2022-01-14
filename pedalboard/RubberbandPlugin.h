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

          // Currently these point to the same array so data will be overwritten in the input channel
          std::cout << "input channel " << i << " = " << inChannels[i] << "\n";
          std::cout << "output channel " << i << " = " << outChannels[i] << "\n";
        }

        // Rubberband expects all channel data with one float array per channel
        std::cout << "Processing context with n samples " << len << "\n";
        processSamples(inChannels, outChannels, len, false);
      }
    }

    void processSamples(const float *const *inBlock, float *const *outBlock, size_t nframes, bool final)
    {
      // Impact of not setting final to true?
      // std::cout << rbPtr->getSamplesRequired() << "\n";
      rbPtr->process(inBlock, nframes, false);
      int samplesAvailable = 0;

      // TODO: Find out when to finish processing samples
      // Otherwise if we just call retrieve straight after and don't fetch anything, what do we do?
      // Do we just return all zeros i.e. a silence? Do we return the same as the input?
      size_t count = 0;
      while (count < 3)
      {
        samplesAvailable = rbPtr->available();
        std::cout << samplesAvailable << "\n";
        rbPtr->retrieve(outBlock, nframes);
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