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

#pragma once

#include "../JuceHeader.h"
#include "../Plugin.h"
#include <vector>

#include "../plugins/AddLatency.h"

namespace Pedalboard {

/**
 * A template class that wraps a mono Pedalboard plugin and runs an
 * independent instance of it on each channel of a multichannel signal.
 * This is the inverse of ForceMono: instead of mixing down to mono,
 * it fans out each channel to its own plugin copy.
 */
template <typename T, typename SampleType = float>
class MonoToMulti : public Plugin {
public:
  virtual ~MonoToMulti(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    while (plugins.size() < spec.numChannels) {
      plugins.push_back(std::make_unique<T>());
    }

    for (size_t i = 0; i < spec.numChannels; i++) {
      plugins[i]->prepare(monoSpec);
    }

    lastSpec = spec;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<SampleType> &context) {
    auto ioBlock = context.getOutputBlock();
    int samplesProcessed = 0;

    for (size_t c = 0; c < ioBlock.getNumChannels(); c++) {
      juce::dsp::AudioBlock<SampleType> monoBlock =
          ioBlock.getSingleChannelBlock(c);
      juce::dsp::ProcessContextReplacing<SampleType> monoContext(monoBlock);
      int result = plugins[c]->process(monoContext);
      if (c == 0) {
        samplesProcessed = result;
      }
    }

    return samplesProcessed;
  }

  virtual void reset() {
    for (auto &p : plugins) {
      p->reset();
    }
  }

  T &getNestedPlugin(size_t channel = 0) { return *plugins[channel]; }

private:
  std::vector<std::unique_ptr<T>> plugins;
};

/**
 * A test plugin used to verify the behaviour of the MonoToMulti wrapper.
 * Reuses ExpectsMono from ForceMono.h to assert each channel is processed
 * as mono independently.
 */
class ExpectsMonoPerChannel : public AddLatency {
public:
  virtual ~ExpectsMonoPerChannel(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    if (spec.numChannels != 1) {
      throw std::runtime_error("Expected mono input!");
    }
    AddLatency::prepare(spec);
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    if (context.getInputBlock().getNumChannels() != 1) {
      throw std::runtime_error("Expected mono input!");
    }
    return AddLatency::process(context);
  }
};

using MonoToMultiTestPlugin = MonoToMulti<ExpectsMonoPerChannel>;

inline void init_mono_to_multi_test_plugin(py::module &m) {
  py::class_<MonoToMultiTestPlugin, Plugin,
             std::shared_ptr<MonoToMultiTestPlugin>>(m,
                                                     "MonoToMultiTestPlugin")
      .def(py::init(
          []() { return std::make_unique<MonoToMultiTestPlugin>(); }))
      .def("__repr__", [](const MonoToMultiTestPlugin &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.MonoToMultiTestPlugin";
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard
