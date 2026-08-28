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
      "A loudness-maximizing limiter.\n\n"
      "This plugin wraps JUCE's ``dsp::Limiter``, which applies two stages of "
      "compression followed by automatic makeup gain and a hard clip at 0 dBFS. "
      "It is designed to increase perceived loudness, not to enforce a specific "
      "peak ceiling.\n\n"
      ".. note::\n\n"
      "   ``threshold_db`` controls when compression *engages*, not the output "
      "   ceiling.  The output is always hard-clipped at 0 dBFS.  Makeup gain "
      "   is applied automatically and cannot be disabled — signals below the "
      "   threshold will be boosted.\n\n"
      "   For a limiter that enforces a configurable ceiling without makeup gain, "
      "   see :class:`BrickwallLimiter`.\n")
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