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
class Distortion
    : public JucePlugin<juce::dsp::ProcessorChain<
          juce::dsp::Gain<SampleType>, juce::dsp::WaveShaper<SampleType>>> {
public:
  void setDriveDecibels(const float f) noexcept { driveDecibels = f; }
  float getDriveDecibels() const noexcept { return driveDecibels; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    JucePlugin<juce::dsp::ProcessorChain<juce::dsp::Gain<SampleType>,
                                         juce::dsp::WaveShaper<SampleType>>>::
        prepare(spec);
    this->getDSP().template get<gainIndex>().setGainDecibels(
        getDriveDecibels());
    this->getDSP().template get<waveshaperIndex>().functionToUse =
        [](SampleType x) { return std::tanh(x); };
  }

private:
  SampleType driveDecibels;

  enum { gainIndex, waveshaperIndex };
};

inline void init_distortion(py::module &m) {
  py::class_<Distortion<float>, Plugin, std::shared_ptr<Distortion<float>>>(
      m, "Distortion",
      "A distortion effect, which applies a non-linear (``tanh``, or "
      "hyperbolic tangent) waveshaping function to apply harmonically pleasing "
      "distortion to a signal.\n\nThis plugin produces a signal that is "
      "roughly equivalent to running: ``def distortion(x): return tanh(x * "
      "db_to_gain(drive_db))``")
      .def(py::init([](float drive_db) {
             auto plugin = std::make_unique<Distortion<float>>();
             plugin->setDriveDecibels(drive_db);
             return plugin;
           }),
           py::arg("drive_db") = 25)
      .def("__repr__",
           [](const Distortion<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Distortion";
             ss << " drive_db=" << plugin.getDriveDecibels();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("drive_db", &Distortion<float>::getDriveDecibels,
                    &Distortion<float>::setDriveDecibels);
}
}; // namespace Pedalboard
