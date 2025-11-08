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

#define TO_STRING(s) _TO_STRING(s)
#define _TO_STRING(s) #s
#define CHORUS_MIN_RATE_HZ 0
#define CHORUS_MAX_RATE_HZ 100

template <typename SampleType>
class Chorus : public JucePlugin<juce::dsp::Chorus<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Rate, {
    if (value < CHORUS_MIN_RATE_HZ || value > CHORUS_MAX_RATE_HZ) {
      throw std::range_error("Rate must be between " TO_STRING(
          CHORUS_MIN_RATE_HZ) " Hz and " TO_STRING(CHORUS_MAX_RATE_HZ) " Hz.");
    }
  });
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Depth, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, CentreDelay, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Feedback, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Mix, {
    if (value < 0.0 || value > 1.0) {
      throw std::range_error("Mix must be between 0.0 and 1.0.");
    }
  });
};

inline void init_chorus(py::module &m) {
  py::class_<Chorus<float>, Plugin, std::shared_ptr<Chorus<float>>>(
      m, "Chorus",
      "A basic chorus effect.\n\nThis audio effect can be controlled via the "
      "speed and depth of the LFO controlling the frequency response, a mix "
      "control, a feedback control, and the centre delay of the modulation. "
      "\n\n"
      "Note: To get classic chorus sounds try to use a centre delay time "
      "around 7-8 ms with a low feedback volume and a low depth. This effect "
      "can also be used as a flanger with a lower centre delay time and a "
      "lot of feedback, and as a vibrato effect if the mix value is 1.")
      .def(py::init([](float rateHz, float depth, float centreDelayMs,
                       float feedback, float mix) {
             auto plugin = std::make_unique<Chorus<float>>();
             plugin->setRate(rateHz);
             plugin->setDepth(depth);
             plugin->setCentreDelay(centreDelayMs);
             plugin->setFeedback(feedback);
             plugin->setMix(mix);
             return plugin;
           }),
           py::arg("rate_hz") = 1.0, py::arg("depth") = 0.25,
           py::arg("centre_delay_ms") = 7.0, py::arg("feedback") = 0.0,
           py::arg("mix") = 0.5)
      .def("__repr__",
           [](const Chorus<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Chorus";
             ss << " rate_hz=" << plugin.getRate();
             ss << " depth=" << plugin.getDepth();
             ss << " centre_delay_ms=" << plugin.getCentreDelay();
             ss << " feedback=" << plugin.getFeedback();
             ss << " mix=" << plugin.getMix();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property(
          "rate_hz", &Chorus<float>::getRate, &Chorus<float>::setRate,
          "The speed of the chorus effect's low-frequency oscillator (LFO), in "
          "Hertz. This value must be between " TO_STRING(
              CHORUS_MIN_RATE_HZ) " Hz and " TO_STRING(CHORUS_MAX_RATE_HZ) " Hz"
                                                                           ".")
      .def_property("depth", &Chorus<float>::getDepth, &Chorus<float>::setDepth)
      .def_property("centre_delay_ms", &Chorus<float>::getCentreDelay,
                    &Chorus<float>::setCentreDelay)
      .def_property("feedback", &Chorus<float>::getFeedback,
                    &Chorus<float>::setFeedback)
      .def_property("mix", &Chorus<float>::getMix, &Chorus<float>::setMix);
  ;
}
}; // namespace Pedalboard