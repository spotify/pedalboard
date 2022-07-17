/*
 * pedalboard
 * Copyright 2022 Spotify AB
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

#include "../Plugin.h"

namespace Pedalboard {
template <typename SampleType> class Invert : public Plugin {
  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {}
  int process(const juce::dsp::ProcessContextReplacing<SampleType> &context)
      override final {
    context.getOutputBlock().negate();
    return context.getOutputBlock().getNumSamples();
  }
  void reset() noexcept override {}
};

inline void init_invert(py::module &m) {
  py::class_<Invert<float>, Plugin, std::shared_ptr<Invert<float>>>(
      m, "Invert",
      "Flip the polarity of the signal. This effect is not audible on its own "
      "and takes no parameters. This effect is mathematically identical to "
      "``def invert(x): return -x``.\n"
      "\n"
      "Inverting a signal may be useful to cancel out signals in many cases; "
      "for instance, ``Invert`` can be used with the ``Mix`` plugin to remove "
      "the original signal from an effects chain that contains multiple "
      "signals.")
      .def(py::init([]() { return std::make_unique<Invert<float>>(); }))
      .def("__repr__", [](const Invert<float> &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.Invert";
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}
}; // namespace Pedalboard