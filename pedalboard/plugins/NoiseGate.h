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

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;

#include "../JucePlugin.h"

namespace Pedalboard {
template <typename SampleType>
class NoiseGate : public JucePlugin<juce::dsp::NoiseGate<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Ratio, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Attack, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_noisegate(nb::module_ &m) {

  nb::class_<NoiseGate<float>, Plugin>(
      m, "NoiseGate",
      "A simple noise gate with standard threshold, ratio, attack time and "
      "release time controls. Can be used as an expander if the ratio is low.")
      .def(nb::init([](float thresholddB, float ratio, float attackMs,
                       float releaseMs) {
             auto plugin = std::make_unique<NoiseGate<float>>();
             plugin->setThreshold(thresholddB);
             plugin->setRatio(ratio);
             plugin->setAttack(attackMs);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           nb::arg("threshold_db") = -100.0, nb::arg("ratio") = 10,
           nb::arg("attack_ms") = 1.0, nb::arg("release_ms") = 100.0)
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
      .def_prop_rw("threshold_db", &NoiseGate<float>::getThreshold,
                   &NoiseGate<float>::setThreshold)
      .def_prop_rw("ratio", &NoiseGate<float>::getRatio,
                   &NoiseGate<float>::setRatio)
      .def_prop_rw("attack_ms", &NoiseGate<float>::getAttack,
                   &NoiseGate<float>::setAttack)
      .def_prop_rw("release_ms", &NoiseGate<float>::getRelease,
                   &NoiseGate<float>::setRelease);
}
}; // namespace Pedalboard
