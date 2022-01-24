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
class Limiter : public JucePlugin<juce::dsp::Limiter<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_limiter(py::module &m) {
  py::class_<Limiter<float>, Plugin, std::shared_ptr<Limiter<float>>>(
      m, "Limiter",
      "A simple limiter with standard threshold and release time controls, "
      "featuring two compressors and a hard clipper at 0 dB.")
      .def(py::init([](float thresholdDb, float releaseMs) {
             auto plugin = std::make_unique<Limiter<float>>();
             plugin->setThreshold(thresholdDb);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           py::arg("threshold_db") = -10.0, py::arg("release_ms") = 100.0)
      .def("__repr__",
           [](const Limiter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Limiter";
             ss << " threshold_db=" << plugin.getThreshold();
             ss << " release_ms=" << plugin.getRelease();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("threshold_db", &Limiter<float>::getThreshold,
                    &Limiter<float>::setThreshold)
      .def_property("release_ms", &Limiter<float>::getRelease,
                    &Limiter<float>::setRelease);
}
}; // namespace Pedalboard