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

#include "../JucePlugin.h"

namespace Pedalboard {

/**
 * A dummy plugin that buffers audio data internally, used to test Pedalboard's
 * automatic delay compensation.
 */
class DelayLine : public JucePlugin<juce::dsp::DelayLine<
                      float, juce::dsp::DelayLineInterpolationTypes::None>> {
public:
  virtual ~DelayLine(){};

  virtual void reset() override {
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

inline void init_delay_line(py::module &m) {
  py::class_<DelayLine, Plugin>(
      m, "DelayLine",
      "A dummy plugin that delays input audio for the given number of samples "
      "before passing it back to the output. Used internally to test "
      "Pedalboard's automatic latency compensation. Probably not useful as a "
      "real effect.")
      .def(py::init([](int samples) {
             auto dl = new DelayLine();
             dl->getDSP().setMaximumDelayInSamples(samples);
             dl->getDSP().setDelay(samples);
             return dl;
           }),
           py::arg("samples") = 44100);
}
}; // namespace Pedalboard
