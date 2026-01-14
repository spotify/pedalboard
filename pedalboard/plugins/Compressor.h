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
class Compressor : public JucePlugin<juce::dsp::Compressor<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Ratio, {
    if (value < 1.0) {
      throw std::range_error("Compressor ratio must be a value >= 1.0.");
    }
  });
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Attack, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_compressor(nb::module_ &m) {
  nb::class_<Compressor<float>, Plugin>(
      m, "Compressor",
      "A dynamic range compressor, used to reduce the volume of loud sounds "
      "and \"compress\" the loudness of the signal.\n\nFor a lossy compression "
      "algorithm that introduces noise or artifacts, see "
      "``pedalboard.MP3Compressor`` or ``pedalboard.GSMCompressor``.")
      .def(nb::init([](float thresholddB, float ratio, float attackMs,
                       float releaseMs) {
             auto plugin = std::make_unique<Compressor<float>>();
             plugin->setThreshold(thresholddB);
             plugin->setRatio(ratio);
             plugin->setAttack(attackMs);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           nb::arg("threshold_db") = 0, nb::arg("ratio") = 1,
           nb::arg("attack_ms") = 1.0, nb::arg("release_ms") = 100)
      .def("__repr__",
           [](const Compressor<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Compressor";
             ss << " threshold_db=" << plugin.getThreshold();
             ss << " ratio=" << plugin.getRatio();
             ss << " attack_ms=" << plugin.getAttack();
             ss << " release_ms=" << plugin.getRelease();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_prop_rw("threshold_db", &Compressor<float>::getThreshold,
                   &Compressor<float>::setThreshold)
      .def_prop_rw("ratio", &Compressor<float>::getRatio,
                   &Compressor<float>::setRatio)
      .def_prop_rw("attack_ms", &Compressor<float>::getAttack,
                   &Compressor<float>::setAttack)
      .def_prop_rw("release_ms", &Compressor<float>::getRelease,
                   &Compressor<float>::setRelease);
}
}; // namespace Pedalboard