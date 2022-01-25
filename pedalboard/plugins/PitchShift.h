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

#include "../RubberbandPlugin.h"

namespace Pedalboard {
/*
Modifies pitch of an audio without affecting duration
*/
class PitchShift : public RubberbandPlugin

{
private:
  double _scaleFactor = 1.0;

  // Allow pitch shifting by up to 6 octaves up or down:
  static constexpr float MIN_SCALE_FACTOR = 0.015625;
  static constexpr float MAX_SCALE_FACTOR = 64.0;

public:
  void setScaleFactor(double scale) {
    if (scale < MIN_SCALE_FACTOR || scale > MAX_SCALE_FACTOR) {
      throw std::range_error("Scale factor must be a value between " +
                             std::to_string(MIN_SCALE_FACTOR) + " and " +
                             std::to_string(MAX_SCALE_FACTOR) + ".");
    }

    _scaleFactor = scale;

    if (rbPtr)
      rbPtr->setPitchScale(_scaleFactor);
  }

  double getScaleFactor() { return _scaleFactor; }

  void prepare(const juce::dsp::ProcessSpec &spec) override final {
    RubberbandPlugin::prepare(spec);
    rbPtr->setPitchScale(_scaleFactor);
  }
};

inline void init_pitch_shift(py::module &m) {
  py::class_<PitchShift, Plugin>(
      m, "PitchShift",
      "Shift pitch without affecting audio duration. Shifted audio may start "
      "with a short duration of silence, depending on the scale factor used. "
      "The rate of the audio will be multiplied by scale_factor (i.e.: 2x "
      "means one octave up, 0.5x means one octave down).")
      .def(py::init([](double scale) {
             auto plugin = new PitchShift();
             plugin->setScaleFactor(scale);
             return plugin;
           }),
           py::arg("scale_factor") = 1.0)
      .def_property("scale_factor", &PitchShift::getScaleFactor,
                    &PitchShift::setScaleFactor);
}
}; // namespace Pedalboard
