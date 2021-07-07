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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"

namespace Pedalboard {
template <typename SampleType>
class HighpassFilter : public JucePlugin<juce::dsp::IIR::Filter<SampleType>> {
public:
  void setCutoffFrequencyHz(float f) noexcept { cutoffFrequencyHz = f; }
  float getCutoffFrequencyHz() const noexcept { return cutoffFrequencyHz; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    JucePlugin<juce::dsp::IIR::Filter<SampleType>>::prepare(spec);
    this->getDSP().coefficients =
        juce::dsp::IIR::Coefficients<SampleType>::makeFirstOrderHighPass(
            spec.sampleRate, cutoffFrequencyHz);
  }

private:
  float cutoffFrequencyHz;
};

inline void init_highpass(py::module &m) {
  py::class_<HighpassFilter<float>, Plugin>(
      m, "HighpassFilter",
      "Apply a first-order high-pass filter with a roll-off of 6dB/octave. "
      "The cutoff frequency will be attenuated by -3dB (i.e.: 0.707x as "
      "loud).")
      .def(py::init([](float cutoff_frequency_hz) {
             auto plugin = new HighpassFilter<float>();
             plugin->setCutoffFrequencyHz(cutoff_frequency_hz);
             return plugin;
           }),
           py::arg("cutoff_frequency_hz") = 50)
      .def("__repr__",
           [](const HighpassFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Highpass";
             ss << " cutoff_frequency_hz=" << plugin.getCutoffFrequencyHz();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("cutoff_frequency_hz",
                    &HighpassFilter<float>::getCutoffFrequencyHz,
                    &HighpassFilter<float>::setCutoffFrequencyHz);
}
}; // namespace Pedalboard
