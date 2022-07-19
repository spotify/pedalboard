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
class LadderFilter : public JucePlugin<juce::dsp::LadderFilter<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, CutoffFrequencyHz, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Drive, {
    if (value < 1.0) {
      throw std::range_error("Drive must be greater than 1.0.");
    }
  });
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Resonance, {
    if (value < 0.0 || value > 1.0) {
      throw std::range_error("Resonance must be between 0.0 and 1.0.");
    }
  });
  DEFINE_DSP_SETTER_AND_GETTER(juce::dsp::LadderFilterMode, Mode, {
    switch (value) {
    case juce::dsp::LadderFilterMode::LPF12:
      break;
    case juce::dsp::LadderFilterMode::HPF12:
      break;
    case juce::dsp::LadderFilterMode::BPF12:
      break;
    case juce::dsp::LadderFilterMode::LPF24:
      break;
    case juce::dsp::LadderFilterMode::HPF24:
      break;
    case juce::dsp::LadderFilterMode::BPF24:
      break;
    default:
      throw std::range_error("Ladder filter mode must be one of: LPF12, HPF12, "
                             "BPF12, LPF24, HPF24, or BPF24.");
    }
  });
};

inline void init_ladderfilter(py::module &m) {
  py::class_<LadderFilter<float>, Plugin, std::shared_ptr<LadderFilter<float>>>
      ladderFilter(
          m, "LadderFilter",
          "A multi-mode audio filter based on the classic Moog "
          "synthesizer ladder filter, invented by Dr. Bob Moog in "
          "1968.\n\nDepending on the filter's mode, frequencies above, "
          "below, or on both sides of the cutoff frequency will be "
          "attenuated. Higher values for the ``resonance`` parameter "
          "may cause peaks in the frequency response around the "
          "cutoff frequency.");

  py::enum_<juce::dsp::LadderFilterMode>(
      ladderFilter, "Mode", "The type of filter architecture to use.")
      .value("LPF12", juce::dsp::LadderFilterMode::LPF12,
             "A low-pass filter with 12 dB of attenuation per octave above the "
             "cutoff frequency.")
      .value(
          "HPF12", juce::dsp::LadderFilterMode::HPF12,
          "A high-pass filter with 12 dB of attenuation per octave below the "
          "cutoff frequency.")
      .value("BPF12", juce::dsp::LadderFilterMode::BPF12,
             "A band-pass filter with 12 dB of attenuation per octave on both "
             "sides of the cutoff frequency.")
      .value("LPF24", juce::dsp::LadderFilterMode::LPF24,
             "A low-pass filter with 24 dB of attenuation per octave above the "
             "cutoff frequency.")
      .value(
          "HPF24", juce::dsp::LadderFilterMode::HPF24,
          "A high-pass filter with 24 dB of attenuation per octave below the "
          "cutoff frequency.")
      .value("BPF24", juce::dsp::LadderFilterMode::BPF24,
             "A band-pass filter with 24 dB of attenuation per octave on both "
             "sides of the cutoff frequency.")
      .export_values();

  ladderFilter
      .def(py::init([](juce::dsp::LadderFilterMode mode, float cutoffHz,
                       float resonance, float drive) {
             auto plugin = std::make_unique<LadderFilter<float>>();
             plugin->setMode(mode);
             plugin->setCutoffFrequencyHz(cutoffHz);
             plugin->setResonance(resonance);
             plugin->setDrive(drive);
             return plugin;
           }),
           py::arg("mode") = juce::dsp::LadderFilterMode::LPF12,
           py::arg("cutoff_hz") = 200, py::arg("resonance") = 0,
           py::arg("drive") = 1.0)
      .def("__repr__",
           [](const LadderFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.LadderFilter";
             ss << " mode=";
             switch (plugin.getMode()) {
             case juce::dsp::LadderFilterMode::LPF12:
               ss << "pedalboard.LadderFilter.LPF12";
               break;
             case juce::dsp::LadderFilterMode::HPF12:
               ss << "pedalboard.LadderFilter.HPF12";
               break;
             case juce::dsp::LadderFilterMode::BPF12:
               ss << "pedalboard.LadderFilter.BPF12";
               break;
             case juce::dsp::LadderFilterMode::LPF24:
               ss << "pedalboard.LadderFilter.LPF24";
               break;
             case juce::dsp::LadderFilterMode::HPF24:
               ss << "pedalboard.LadderFilter.HPF24";
               break;
             case juce::dsp::LadderFilterMode::BPF24:
               ss << "pedalboard.LadderFilter.BPF24";
               break;
             default:
               ss << "unknown";
               break;
             }
             ss << " cutoff_hz=" << plugin.getCutoffFrequencyHz();
             ss << " resonance=" << plugin.getResonance();
             ss << " drive=" << plugin.getDrive();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("mode", &LadderFilter<float>::getMode,
                    &LadderFilter<float>::setMode)
      .def_property("cutoff_hz", &LadderFilter<float>::getCutoffFrequencyHz,
                    &LadderFilter<float>::setCutoffFrequencyHz)
      .def_property("resonance", &LadderFilter<float>::getResonance,
                    &LadderFilter<float>::setResonance)
      .def_property("drive", &LadderFilter<float>::getDrive,
                    &LadderFilter<float>::setDrive);
  ;
}
}; // namespace Pedalboard