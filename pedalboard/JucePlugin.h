/*
 * pedalboard
 * Copyright 2021 Spotify AB
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
    if (lastSpec.sampleRate != spec.sampleRate ||
        lastSpec.maximumBlockSize < spec.maximumBlockSize ||
        spec.numChannels != lastSpec.numChannels) {
      dspBlock.prepare(spec);
      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override {
    dspBlock.process(context);
    return context.getOutputBlock().getNumSamples();
  }

  void reset() override { dspBlock.reset(); }

  DSPType &getDSP() { return dspBlock; };

private:
  DSPType dspBlock;
};
} // namespace Pedalboard
