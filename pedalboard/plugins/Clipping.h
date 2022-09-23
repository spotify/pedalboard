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
template <typename SampleType> class Clipping : public Plugin {
public:
  void setThresholdDecibels(const SampleType f) noexcept {
    thresholdDecibels = f;

    negativeThresholdGain = -juce::Decibels::decibelsToGain<SampleType>(f);
    positiveThresholdGain = juce::Decibels::decibelsToGain<SampleType>(f);
  }

  float getThresholdDecibels() const noexcept { return thresholdDecibels; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {}

  virtual int process(
      const juce::dsp::ProcessContextReplacing<SampleType> &context) override {
    auto ioBlock = context.getOutputBlock();

    for (int c = 0; c < ioBlock.getNumChannels(); c++) {
      SampleType *channelPointer = ioBlock.getChannelPointer(c);

      juce::FloatVectorOperations::clip(
          channelPointer, channelPointer, negativeThresholdGain,
          positiveThresholdGain, ioBlock.getNumSamples());
    }

    return context.getOutputBlock().getNumSamples();
  }

  virtual void reset() {}

private:
  SampleType thresholdDecibels;

  SampleType negativeThresholdGain;
  SampleType positiveThresholdGain;
};

inline void init_clipping(py::module &m) {
  py::class_<Clipping<float>, Plugin, std::shared_ptr<Clipping<float>>>(
      m, "Clipping",
      "A distortion plugin that adds hard distortion to the signal "
      "by clipping the signal at the provided threshold (in decibels).")
      .def(py::init([](float thresholdDb) {
             auto plugin = std::make_unique<Clipping<float>>();
             plugin->setThresholdDecibels(thresholdDb);
             return plugin;
           }),
           py::arg("threshold_db") = -6.0)
      .def("__repr__",
           [](const Clipping<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Clipping";
             ss << " threshold_db=" << plugin.getThresholdDecibels();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("threshold_db", &Clipping<float>::getThresholdDecibels,
                    &Clipping<float>::setThresholdDecibels);
}
}; // namespace Pedalboard