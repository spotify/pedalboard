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

#include "JuceHeader.h"
#include <mutex>

#include "Plugin.h"
#include "process.h"

namespace Pedalboard {
/**
 * A class that allows nesting a pedalboard within another.
 */
class ChainPlugin : public Plugin {
public:
  ChainPlugin(std::vector<Plugin *> chain) : chain(chain) {}
  virtual ~ChainPlugin(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    for (auto plugin : chain) plugin->prepare(spec);
    lastSpec = spec;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    // assuming process context replacing
    auto ioBlock = context.getOutputBlock();

    float *channels[8] = {};
    for (int i = 0; i < ioBlock.getNumChannels(); i++) {
      channels[i] = ioBlock.getChannelPointer(i);
    } 

    juce::AudioBuffer<float> ioBuffer(channels, ioBlock.getNumChannels(), ioBlock.getNumSamples());
    return ::Pedalboard::process(ioBuffer, lastSpec, chain, false);
  }

  virtual void reset() {
    for (auto plugin : chain) plugin->reset();
  }

  virtual int getLatencyHint() {
    int hint = 0;
    for (auto plugin : chain) hint += plugin->getLatencyHint();
    return hint;
  }

protected:
  std::vector<Plugin *> chain;
};

inline void init_chain(py::module &m) {
  py::class_<ChainPlugin, Plugin>(
      m, "ChainPlugin",
      "Run a pedalboard within a plugin. Meta.")
      .def(py::init([](std::vector<Plugin *> plugins) {
             return new ChainPlugin(plugins);
           }),
           py::arg("plugins"), py::keep_alive<1, 2>());
}

} // namespace Pedalboard