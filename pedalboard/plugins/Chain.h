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

#include "../PluginContainer.h"
#include "../process.h"

namespace Pedalboard {
/**
 * A class that allows nesting a list of plugins within another.
 */
class Chain : public PluginContainer {
public:
  Chain(std::vector<std::shared_ptr<Plugin>> plugins)
      : PluginContainer(plugins) {}
  virtual ~Chain(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    for (auto plugin : plugins) {
      if (plugin) {
        plugin->prepare(spec);
      }
    }
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
    for (auto plugin : plugins) {
      if (plugin) {
        plugin->reset();
      }
    }
  }

  virtual int getLatencyHint() {
    int hint = 0;
    for (auto plugin : plugins) {
      if (plugin) {
        hint += plugin->getLatencyHint();
      }
    }
    return hint;
  }
};

inline void init_chain(py::module &m) {
  py::class_<Chain, PluginContainer, std::shared_ptr<Chain>>(
      m, "Chain",
      "Run zero or more plugins as a plugin. Useful when "
      "used with the Mix plugin.")
      .def(py::init([](std::vector<std::shared_ptr<Plugin>> plugins) {
             return new Chain(plugins);
           }),
           py::arg("plugins"))
      .def(py::init([]() { return new Chain({}); }))
      .def("__repr__", [](Chain &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.Chain with " << plugin.getPlugins().size()
           << " plugin";
        if (plugin.getPlugins().size() != 1) {
          ss << "s";
        }
        ss << ": [";
        for (int i = 0; i < plugin.getPlugins().size(); i++) {
          py::object nestedPlugin = py::cast(plugin.getPlugins()[i]);
          ss << nestedPlugin.attr("__repr__")();
          if (i < plugin.getPlugins().size() - 1) {
            ss << ", ";
          }
        }
        ss << "] at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard