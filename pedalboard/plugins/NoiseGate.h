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
class NoiseGate : public JucePlugin<juce::dsp::NoiseGate<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Ratio, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Attack, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_noisegate(py::module &m) {

  py::class_<NoiseGate<float>, Plugin, std::shared_ptr<NoiseGate<float>>>(
      m, "NoiseGate",
      "A simple noise gate with standard threshold, ratio, attack time and "
      "release time controls. Can be used as an expander if the ratio is low.")
      .def(py::init([](float thresholddB, float ratio, float attackMs,
                       float releaseMs) {
             auto plugin = std::make_unique<NoiseGate<float>>();
             plugin->setThreshold(thresholddB);
             plugin->setRatio(ratio);
             plugin->setAttack(attackMs);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           py::arg("threshold_db") = -100.0, py::arg("ratio") = 10,
           py::arg("attack_ms") = 1.0, py::arg("release_ms") = 100.0)
      .def("__repr__",
           [](const NoiseGate<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.NoiseGate";
             ss << " threshold_db=" << plugin.getThreshold();
             ss << " ratio=" << plugin.getRatio();
             ss << " attack_ms=" << plugin.getAttack();
             ss << " release_ms=" << plugin.getRelease();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("threshold_db", &NoiseGate<float>::getThreshold,
                    &NoiseGate<float>::setThreshold)
      .def_property("ratio", &NoiseGate<float>::getRatio,
                    &NoiseGate<float>::setRatio)
      .def_property("attack_ms", &NoiseGate<float>::getAttack,
                    &NoiseGate<float>::setAttack)
      .def_property("release_ms", &NoiseGate<float>::getRelease,
                    &NoiseGate<float>::setRelease);
}
}; // namespace Pedalboard
