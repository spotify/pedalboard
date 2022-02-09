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
class Phaser : public JucePlugin<juce::dsp::Phaser<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Rate, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Depth, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, CentreFrequency, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Feedback, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Mix, {});
};

inline void init_phaser(py::module &m) {

  py::class_<Phaser<float>, Plugin, std::shared_ptr<Phaser<float>>>(
      m, "Phaser",
      "A 6 stage phaser that modulates first order all-pass filters to create "
      "sweeping notches in the magnitude frequency response. This audio effect "
      "can be controlled with standard phaser parameters: the speed and depth "
      "of the LFO controlling the frequency response, a mix control, a "
      "feedback control, and the centre frequency of the modulation.")
      .def(py::init([](float rateHz, float depth, float centreFrequency,
                       float feedback, float mix) {
             auto plugin = std::make_unique<Phaser<float>>();
             plugin->setRate(rateHz);
             plugin->setDepth(depth);
             plugin->setCentreFrequency(centreFrequency);
             plugin->setFeedback(feedback);
             plugin->setMix(mix);
             return plugin;
           }),
           py::arg("rate_hz") = 1.0, py::arg("depth") = 0.5,
           py::arg("centre_frequency_hz") = 1300.0, py::arg("feedback") = 0.0,
           py::arg("mix") = 0.5)
      .def("__repr__",
           [](const Phaser<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Phaser";
             ss << " rate_hz=" << plugin.getRate();
             ss << " depth=" << plugin.getDepth();
             ss << " centre_frequency_hz=" << plugin.getCentreFrequency();
             ss << " feedback=" << plugin.getFeedback();
             ss << " mix=" << plugin.getMix();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("rate_hz", &Phaser<float>::getRate, &Phaser<float>::setRate)
      .def_property("depth", &Phaser<float>::getDepth, &Phaser<float>::setDepth)
      .def_property("centre_frequency_hz", &Phaser<float>::getCentreFrequency,
                    &Phaser<float>::setCentreFrequency)
      .def_property("feedback", &Phaser<float>::getFeedback,
                    &Phaser<float>::setFeedback)
      .def_property("mix", &Phaser<float>::getMix, &Phaser<float>::setMix);
}
}; // namespace Pedalboard