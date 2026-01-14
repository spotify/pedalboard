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
class Reverb : public JucePlugin<juce::dsp::Reverb> {
public:
  float getRoomSize() { return this->getDSP().getParameters().roomSize; }
  float getDamping() { return this->getDSP().getParameters().damping; }
  float getWetLevel() { return this->getDSP().getParameters().wetLevel; }
  float getDryLevel() { return this->getDSP().getParameters().dryLevel; }
  float getWidth() { return this->getDSP().getParameters().width; }
  float getFreezeMode() { return this->getDSP().getParameters().freezeMode; }

  void setRoomSize(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Room Size value must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.roomSize = value;
    this->getDSP().setParameters(parameters);
  }
  void setDamping(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Damping value must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.damping = value;
    this->getDSP().setParameters(parameters);
  }
  void setWetLevel(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Wet Level must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.wetLevel = value;
    this->getDSP().setParameters(parameters);
  }
  void setDryLevel(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Dry Level must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.dryLevel = value;
    this->getDSP().setParameters(parameters);
  }
  void setWidth(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Width value must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.width = value;
    this->getDSP().setParameters(parameters);
  }
  void setFreezeMode(float value) {
    if (value < 0.0 || value > 1.0)
      throw std::range_error("Freeze Mode value must be between 0.0 and 1.0.");
    auto parameters = this->getDSP().getParameters();
    parameters.freezeMode = value;
    this->getDSP().setParameters(parameters);
  }
};

inline void init_reverb(nb::module_ &m) {
  nb::class_<Reverb, Plugin>(
      m, "Reverb",
      "A simple reverb effect. Uses a simple stereo reverb algorithm, based on "
      "the technique and tunings used in `FreeVerb "
      "<https://ccrma.stanford.edu/~jos/pasp/Freeverb.html>_`.")
      .def(nb::init([](float roomSize, float damping, float wetLevel,
                       float dryLevel, float width, float freezeMode) {
             auto plugin = std::make_unique<Reverb>();
             plugin->setRoomSize(roomSize);
             plugin->setDamping(damping);
             plugin->setWetLevel(wetLevel);
             plugin->setDryLevel(dryLevel);
             plugin->setWidth(width);
             plugin->setFreezeMode(freezeMode);
             return plugin;
           }),
           nb::arg("room_size") = 0.5, nb::arg("damping") = 0.5,
           nb::arg("wet_level") = 0.33, nb::arg("dry_level") = 0.4,
           nb::arg("width") = 1.0, nb::arg("freeze_mode") = 0.0)
      .def("__repr__",
           [](Reverb &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Reverb";
             ss << " room_size=" << plugin.getRoomSize();
             ss << " damping=" << plugin.getDamping();
             ss << " wet_level=" << plugin.getWetLevel();
             ss << " dry_level=" << plugin.getDryLevel();
             ss << " width=" << plugin.getWidth();
             ss << " freeze_mode=" << plugin.getFreezeMode();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_prop_rw("room_size", &Reverb::getRoomSize, &Reverb::setRoomSize)
      .def_prop_rw("damping", &Reverb::getDamping, &Reverb::setDamping)
      .def_prop_rw("wet_level", &Reverb::getWetLevel, &Reverb::setWetLevel)
      .def_prop_rw("dry_level", &Reverb::getDryLevel, &Reverb::setDryLevel)
      .def_prop_rw("width", &Reverb::getWidth, &Reverb::setWidth)
      .def_prop_rw("freeze_mode", &Reverb::getFreezeMode,
                   &Reverb::setFreezeMode);
}
}; // namespace Pedalboard