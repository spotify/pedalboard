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

    virtual void prepare(const juce::dsp::ProcessSpec &spec) = 0;

    void process(
        const juce::dsp::ProcessContextReplacing<float> &context) override final
    {
      // This part might be the same across all rubberband plugins?
      if (rbPtr)
      {
        // Is this the right way to get the next input block?
        // Do I have to shift something?
        size_t sampleFrames = 0; // How do I compute this? Is there a value somewhere
        auto inputBlock = context.getInputBlock();
        // rbPtr->process(, sampleFrames, false); // Impact of not setting final to true?
        // size_t samplesRetrieved = 0;
        // size_t samplesAvailable = 0;
        // while (samplesRetrieved < inputBlock.getNumSamples()) // is this correct to hang here for each block?
        // {
        //   samplesAvailable = rbPtr->available();
        //   if (samplesAvailable)
        //   {
        //     samplesRetrieved += rbPtr->retrieve(context.getOutputBlock(), samplesAvailable);
        //   }
        // }
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
    // TODO: Do I need the DSP block? or are the buffers handled by RB?
    // DSPType dspBlock;
    RubberBandStretcher *rbPtr = nullptr;
  };
}; // pedalboard