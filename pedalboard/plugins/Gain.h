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
class Gain : public JucePlugin<juce::dsp::Gain<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, GainDecibels, {});
};

inline void init_gain(py::module &m) {
  py::class_<Gain<float>, Plugin, std::shared_ptr<Gain<float>>>(
      m, "Gain",
      "A gain plugin that increases or decreases the volume of a signal by "
      "amplifying or attenuating it by the provided value (in decibels). No "
      "distortion or other effects are applied.\n\nThink of this as a volume "
      "control.")
      .def(py::init([](float gaindB) {
             auto plugin = std::make_unique<Gain<float>>();
             plugin->setGainDecibels(gaindB);
             return plugin;
           }),
           py::arg("gain_db") = 1.0)
      .def("__repr__",
           [](const Gain<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Gain";
             ss << " gain_db=" << plugin.getGainDecibels();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("gain_db", &Gain<float>::getGainDecibels,
                    &Gain<float>::setGainDecibels);
}
}; // namespace Pedalboard