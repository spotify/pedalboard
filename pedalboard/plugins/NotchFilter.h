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
class NotchFilter : public JucePlugin<juce::dsp::IIR::Filter<SampleType>> {
public:
  void setCutoffFrequencyHz(float f) noexcept { cutoffFrequencyHz = f; }
  float getCutoffFrequencyHz() const noexcept { return cutoffFrequencyHz; }
  void setQ(float f) noexcept { Q = f; }
  float getQ() const noexcept { return Q; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    JucePlugin<juce::dsp::IIR::Filter<SampleType>>::prepare(spec);
    this->getDSP().coefficients =
        juce::dsp::IIR::Coefficients<SampleType>::makeNotch(
            spec.sampleRate, cutoffFrequencyHz, Q);
  }

private:
  float cutoffFrequencyHz;
  float Q;
};

inline void init_notch(py::module &m) {
  py::class_<NotchFilter<float>, Plugin>(
      m, "NotchFilter",
      "Create a notch filter with a variable Q, set around cutoff_frequency_hz.")
      .def(py::init([](float cutoff_frequency_hz, float Q_val) {
             auto plugin = new NotchFilter<float>();
             plugin->setCutoffFrequencyHz(cutoff_frequency_hz);
             plugin->setQ(Q_val);
             return plugin;
           }),
           py::arg("cutoff_frequency_hz") = 50,
           py::arg("q") = (juce::MathConstants<double>::sqrt2 / 2.0))
      .def("__repr__",
           [](const NotchFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.NotchFilter";
             ss << " cutoff_frequency_hz=" << plugin.getCutoffFrequencyHz();
             ss << " q=" << plugin.getQ();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
             })
      .def_property("cutoff_frequency_hz",
                    &NotchFilter<float>::getCutoffFrequencyHz,
                    &NotchFilter<float>::setCutoffFrequencyHz)
      .def_property("Q", 
                    &NotchFilter<float>::getQ,
                    &NotchFilter<float>::setQ);
}
}; // namespace Pedalboard
