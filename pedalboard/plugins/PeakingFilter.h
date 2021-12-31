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
class PeakingFilter : public JucePlugin<juce::dsp::IIR::Filter<SampleType>> {
public:
  void setCentreFrequencyHz(float f) noexcept { centreFrequencyHz = f; }
  float getCentreFrequencyHz() const noexcept { return centreFrequencyHz; }
  void setQ(float f) noexcept { Q = f; }
  float getQ() const noexcept { return Q; }
  void setGainFactor(float f) noexcept { gainFactor = f; }
  float getGainFactor() const noexcept { return gainFactor; }


  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    JucePlugin<juce::dsp::IIR::Filter<SampleType>>::prepare(spec);
    this->getDSP().coefficients =
        juce::dsp::IIR::Coefficients<SampleType>::makePeakFilter(
            spec.sampleRate, centreFrequencyHz, Q, gainFactor);
  }

private:
  float centreFrequencyHz;
  float Q;
  float gainFactor;
};

inline void init_peaking(py::module &m) {
  py::class_<PeakingFilter<float>, Plugin>(
      m, "PeakingFilter",
      "Apply a Peaking filter."
      "The gain is a scale factor that the high frequencies are multiplied by, so values"
      "greater than 1.0 will boost the high frequencies, values less than 1.0 will"
      "attenuate them.  Centers the band around centreFrequencyHz")
      .def(py::init([](float centre_frequency_hz, float Q_val, float gainFactorBoost) {
             auto plugin = new PeakingFilter<float>();
             plugin->setCentreFrequencyHz(centre_frequency_hz);
             plugin->setQ(Q_val);
             plugin->setGainFactor(gainFactorBoost);
             return plugin;
           }),
           py::arg("centre_frequency_hz") = 50,
           py::arg("Q") = 0.0,
           py::arg("gain_factor") = .707)
      .def("__repr__",
           [](const PeakingFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.PeakFilter";
             ss << " centre_frequency_hz=" << plugin.getCentreFrequencyHz();
             ss << " Q=" << plugin.getQ();
             ss << " gain_factor=" << plugin.getGainFactor();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("centre_frequency_hz",
                    &PeakingFilter<float>::getCentreFrequencyHz,
                    &PeakingFilter<float>::setCentreFrequencyHz)
      .def_property("Q", 
                    &PeakingFilter<float>::getQ,
                    &PeakingFilter<float>::setQ)
      .def_property("gain_factor", 
                    &PeakingFilter<float>::getGainFactor,
                    &PeakingFilter<float>::setGainFactor);
}
}; // namespace Pedalboard
