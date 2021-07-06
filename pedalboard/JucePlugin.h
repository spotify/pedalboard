#pragma once

#include "JuceHeader.h"

#include "Plugin.h"

// A macro to make it easier to add simple parameter validation to existing
// JUCE DSP types, without fully wrapping them in JUCE's `AudioParameter`
// machinery.
#define DEFINE_DSP_SETTER_AND_GETTER(type, CamelCaseParameterName,             \
                                     setterValidation)                         \
private:                                                                       \
  type _##CamelCaseParameterName;                                              \
                                                                               \
public:                                                                        \
  type get##CamelCaseParameterName() const {                                   \
    return _##CamelCaseParameterName;                                          \
  }                                                                            \
  void set##CamelCaseParameterName(const type value) {                         \
    {setterValidation};                                                        \
    _##CamelCaseParameterName = value;                                         \
    this->getDSP().set##CamelCaseParameterName(value);                         \
  }

namespace Pedalboard {
/**
 * A template class to adapt an arbitrary juce::dsp block to a Plugin.
 * Could technically be used with any type that provides prepare,
 * process, and reset methods.
 */
template <typename DSPType> class JucePlugin : public Plugin {
public:
  virtual ~JucePlugin(){};

  void prepare(const juce::dsp::ProcessSpec &spec) override {
    dspBlock.prepare(spec);
  }

  void process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    dspBlock.process(context);
  }

  void reset() override final { dspBlock.reset(); }

  DSPType &getDSP() { return dspBlock; };

private:
  DSPType dspBlock;
};
} // namespace Pedalboard
