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

#pragma once

#include "../JucePlugin.h"

namespace Pedalboard {

/**
 * A dummy plugin that buffers audio data internally, used to test Pedalboard's
 * automatic latency compensation.
 */
class AddLatency : public JucePlugin<juce::dsp::DelayLine<
                       float, juce::dsp::DelayLineInterpolationTypes::None>> {
public:
  virtual ~AddLatency(){};

  virtual void reset() override {
    JucePlugin<juce::dsp::DelayLine<
        float, juce::dsp::DelayLineInterpolationTypes::None>>::reset();
    getDSP().reset();
    samplesProvided = 0;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) override {
    getDSP().process(context);
    int blockSize = context.getInputBlock().getNumSamples();
    samplesProvided += blockSize;

    return std::min((int)blockSize,
                    std::max(0, (int)(samplesProvided - getDSP().getDelay())));
  }

  virtual int getLatencyHint() override { return getDSP().getDelay(); }

private:
  int samplesProvided = 0;
};

inline void init_add_latency(py::module &m) {
  py::class_<AddLatency, Plugin, std::shared_ptr<AddLatency>>(
      m, "AddLatency",
      "A dummy plugin that delays input audio for the given number of samples "
      "before passing it back to the output. Used internally to test "
      "Pedalboard's automatic latency compensation. Probably not useful as a "
      "real effect.")
      .def(py::init([](int samples) {
             auto al = std::make_unique<AddLatency>();
             al->getDSP().setMaximumDelayInSamples(samples);
             al->getDSP().setDelay(samples);
             return al;
           }),
           py::arg("samples") = 44100);
}
}; // namespace Pedalboard
