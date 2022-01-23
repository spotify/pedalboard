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
#include <mutex>

#include "../Plugin.h"
#include "../process.h"

namespace Pedalboard {
/**
 * A class that allows nesting a list of plugins within another.
 */
class Chain : public Plugin {
public:
  Chain(std::vector<Plugin *> plugins) : plugins(plugins) {}
  virtual ~Chain(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    for (auto plugin : plugins)
      plugin->prepare(spec);
    lastSpec = spec;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    // assuming process context replacing
    auto ioBlock = context.getOutputBlock();

    float **channels =
        (float **)alloca(ioBlock.getNumChannels() * sizeof(float *));
    for (int i = 0; i < ioBlock.getNumChannels(); i++) {
      channels[i] = ioBlock.getChannelPointer(i);
    }

    juce::AudioBuffer<float> ioBuffer(channels, ioBlock.getNumChannels(),
                                      ioBlock.getNumSamples());
    return ::Pedalboard::process(ioBuffer, lastSpec, plugins, false);
  }

  virtual void reset() {
    for (auto plugin : plugins)
      plugin->reset();
  }

  virtual int getLatencyHint() {
    int hint = 0;
    for (auto plugin : plugins)
      hint += plugin->getLatencyHint();
    return hint;
  }

  virtual std::vector<Plugin *> getNestedPlugins() const override { return plugins; }

protected:
  std::vector<Plugin *> plugins;
};

inline void init_chain(py::module &m) {
  py::class_<Chain, Plugin>(m, "Chain",
                            "Run zero or more plugins as a plugin. Useful when "
                            "used with the Mix plugin.")
      .def(py::init([](std::vector<Plugin *> plugins) {
             return new Chain(plugins);
           }),
           py::arg("plugins"), py::keep_alive<1, 2>())
      .def("__repr__",
           [](const Chain &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Chain with " << plugin.getNestedPlugins().size()
                << " plugin";
             if (plugin.getNestedPlugins().size() != 1) {
               ss << "s";
             }
             ss << ": [";
             for (int i = 0; i < plugin.getNestedPlugins().size(); i++) {
               py::object nestedPlugin = py::cast(plugin.getNestedPlugins()[i]);
               ss << nestedPlugin.attr("__repr__")();
               if (i < plugin.getNestedPlugins().size() - 1) {
                 ss << ", ";
               }
             }
             ss << "] at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly("plugins", &Chain::getNestedPlugins);
}

} // namespace Pedalboard