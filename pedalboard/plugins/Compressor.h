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

inline void init_compressor(py::module &m) {
  py::class_<Compressor<float>, Plugin, std::shared_ptr<Compressor<float>>>(
      m, "Compressor",
      "A dynamic range compressor, used to reduce the volume of loud sounds "
      "and \"compress\" the loudness of the signal.\n\nFor a lossy compression "
      "algorithm that introduces noise or artifacts, see "
      "``pedalboard.MP3Compressor`` or ``pedalboard.GSMCompressor``.")
      .def(py::init([](float thresholddB, float ratio, float attackMs,
                       float releaseMs) {
             auto plugin = std::make_unique<Compressor<float>>();
             plugin->setThreshold(thresholddB);
             plugin->setRatio(ratio);
             plugin->setAttack(attackMs);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           py::arg("threshold_db") = 0, py::arg("ratio") = 1,
           py::arg("attack_ms") = 1.0, py::arg("release_ms") = 100)
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
      .def_property("threshold_db", &Compressor<float>::getThreshold,
                    &Compressor<float>::setThreshold)
      .def_property("ratio", &Compressor<float>::getRatio,
                    &Compressor<float>::setRatio)
      .def_property("attack_ms", &Compressor<float>::getAttack,
                    &Compressor<float>::setAttack)
      .def_property("release_ms", &Compressor<float>::getRelease,
                    &Compressor<float>::setRelease);
}
}; // namespace Pedalboard