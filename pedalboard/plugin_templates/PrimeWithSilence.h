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

#include "../JuceHeader.h"
#include "../Plugin.h"
#include "../plugins/AddLatency.h"
#include <mutex>

namespace Pedalboard {

/**
 * A dummy plugin that buffers audio data internally, used to test Pedalboard's
 * automatic latency compensation.
 */
template <typename T, typename SampleType = float,
          int DefaultSilenceLengthSamples = 0>
class PrimeWithSilence
    : public JucePlugin<juce::dsp::DelayLine<
          SampleType, juce::dsp::DelayLineInterpolationTypes::None>> {
public:
  virtual ~PrimeWithSilence(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    JucePlugin<juce::dsp::DelayLine<
        SampleType,
        juce::dsp::DelayLineInterpolationTypes::None>>::prepare(spec);
    this->getDSP().setMaximumDelayInSamples(silenceLengthSamples);
    this->getDSP().setDelay(silenceLengthSamples);
    plugin.prepare(spec);
  }

  virtual void reset() override {
    JucePlugin<juce::dsp::DelayLine<
        SampleType, juce::dsp::DelayLineInterpolationTypes::None>>::reset();
    this->getDSP().reset();
    this->getDSP().setMaximumDelayInSamples(silenceLengthSamples);
    this->getDSP().setDelay(silenceLengthSamples);
    plugin.reset();
    samplesOutput = 0;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) override {
    this->getDSP().process(context);

    // Context now has a delayed signal in it:
    int samplesProcessed = plugin.process(context);
    samplesOutput += samplesProcessed;

    return std::max(
        0, std::min((int)samplesProcessed,
                    (int)samplesOutput - (int)this->getDSP().getDelay()));
  }

  virtual int getLatencyHint() override {
    return this->getDSP().getDelay() + getNestedPlugin().getLatencyHint();
  }

  T &getNestedPlugin() { return plugin; }

  void setSilenceLengthSamples(int newSilenceLengthSamples) {
    if (silenceLengthSamples != newSilenceLengthSamples) {
      this->getDSP().setMaximumDelayInSamples(newSilenceLengthSamples);
      this->getDSP().setDelay(newSilenceLengthSamples);
      silenceLengthSamples = newSilenceLengthSamples;

      reset();
    }
  }

  int getSilenceLengthSamples() const { return silenceLengthSamples; }

private:
  T plugin;
  int samplesOutput = 0;
  int silenceLengthSamples = DefaultSilenceLengthSamples;
};

/**
 * A test plugin used to verify the behaviour of the PrimingPlugin wrapper.
 */
class ExpectsToBePrimed : public AddLatency {
public:
  virtual ~ExpectsToBePrimed(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    getDSP().setMaximumDelayInSamples(10);
    getDSP().setDelay(10);

    AddLatency::prepare(spec);
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    auto inputBlock = context.getInputBlock();

    for (int i = 0; i < inputBlock.getNumSamples(); i++) {
      bool allChannelsSilent = true;

      for (int c = 0; c < inputBlock.getNumChannels(); c++) {
        if (inputBlock.getChannelPointer(c)[i] != 0) {
          allChannelsSilent = false;
        }
      }

      if (!allChannelsSilent) {
        // If we get here, we've got our first non-zero sample.
        if (seenSilentSamples < expectedSilentSamples) {
          throw std::runtime_error("Expected to see " +
                                   std::to_string(expectedSilentSamples) +
                                   " silent samples, but only saw " +
                                   std::to_string(seenSilentSamples) +
                                   " before first non-zero value.");
        }
        break;
      }

      seenSilentSamples++;
    }

    return AddLatency::process(context);
  }

  virtual void reset() {
    seenSilentSamples = 0;
    AddLatency::reset();
  }

  void setExpectedSilentSamples(int newExpectedSilentSamples) {
    expectedSilentSamples = newExpectedSilentSamples;
  }

private:
  int expectedSilentSamples = 0;
  int seenSilentSamples = 0;
};

class PrimeWithSilenceTestPlugin : public PrimeWithSilence<ExpectsToBePrimed> {
public:
  void setExpectedSilentSamples(int newExpectedSilentSamples) {
    setSilenceLengthSamples(newExpectedSilentSamples);
    getNestedPlugin().setExpectedSilentSamples(getSilenceLengthSamples());
  }

  int getExpectedSilentSamples() const { return getSilenceLengthSamples(); }

private:
  int expectedBlockSize = 0;
};

inline void init_prime_with_silence_test_plugin(py::module &m) {
  py::class_<PrimeWithSilenceTestPlugin, Plugin,
             std::shared_ptr<PrimeWithSilenceTestPlugin>>(
      m, "PrimeWithSilenceTestPlugin")
      .def(py::init([](int expectedSilentSamples) {
             auto plugin = std::make_unique<PrimeWithSilenceTestPlugin>();
             plugin->setExpectedSilentSamples(expectedSilentSamples);
             return plugin;
           }),
           py::arg("expected_silent_samples") = 160)
      .def("__repr__", [](const PrimeWithSilenceTestPlugin &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.PrimeWithSilenceTestPlugin";
        ss << " expected_silent_samples=" << plugin.getExpectedSilentSamples();
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard