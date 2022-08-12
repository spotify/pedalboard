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

namespace Pedalboard {
/**
 * A class that allows parallel processing of zero or more separate plugin
 * chains.
 */
class Mix : public PluginContainer {
public:
  Mix(std::vector<std::shared_ptr<Plugin>> plugins)
      : PluginContainer(plugins), pluginBuffers(plugins.size()),
        samplesAvailablePerPlugin(plugins.size()) {}
  virtual ~Mix(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    for (auto plugin : plugins) {
      if (plugin) {
        plugin->prepare(spec);
      }
    }

    int maximumBufferSize = getLatencyHint() + spec.maximumBlockSize;
    for (auto &buffer : pluginBuffers)
      buffer.setSize(spec.numChannels, maximumBufferSize);
    for (int i = 0; i < samplesAvailablePerPlugin.size(); i++)
      samplesAvailablePerPlugin[i] = 0;
    lastSpec = spec;
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    auto ioBlock = context.getOutputBlock();

    for (int i = 0; i < plugins.size(); i++) {
      std::shared_ptr<Plugin> plugin = plugins[i];
      juce::AudioBuffer<float> &buffer = pluginBuffers[i];

      int startInBuffer = samplesAvailablePerPlugin[i];
      int endInBuffer = startInBuffer + ioBlock.getNumSamples();
      // If we don't have enough space, reallocate. (Reluctantly. This is the
      // "audio thread!")
      if (endInBuffer > buffer.getNumSamples()) {
        buffer.setSize(buffer.getNumChannels(), endInBuffer);
      }

      // Copy the audio input into each of these buffers:
      context.getInputBlock().copyTo(buffer, 0, samplesAvailablePerPlugin[i]);

      float **channelPointers =
          (float **)alloca(ioBlock.getNumChannels() * sizeof(float *));
      for (int c = 0; c < buffer.getNumChannels(); c++) {
        channelPointers[c] = buffer.getWritePointer(c, startInBuffer);
      }

      auto subBlock = juce::dsp::AudioBlock<float>(
          channelPointers, buffer.getNumChannels(), ioBlock.getNumSamples());

      juce::dsp::ProcessContextReplacing<float> subContext(subBlock);

      int samplesRendered = subBlock.getNumSamples();

      if (plugin) {
        samplesRendered = plugin->process(subContext);
      }
      samplesAvailablePerPlugin[i] += samplesRendered;

      if (samplesRendered < subBlock.getNumSamples()) {
        // Left-align the results in the buffer, as we'll need all
        // of the plugins' outputs to be aligned:
        for (int c = 0; c < pluginBuffers[i].getNumChannels(); c++) {
          std::memmove(channelPointers[c],
                       channelPointers[c] +
                           (subBlock.getNumSamples() - samplesRendered),
                       sizeof(float) * samplesRendered);
        }
      }
    }

    // Figure out the maximum number of samples we can return,
    // which is the min across all buffers:
    int maxSamplesAvailable = ioBlock.getNumSamples();
    for (int i = 0; i < plugins.size(); i++) {
      maxSamplesAvailable =
          std::min(samplesAvailablePerPlugin[i], maxSamplesAvailable);
    }

    // Now that each plugin has rendered into its own buffer, mix the output:
    ioBlock.clear();
    if (maxSamplesAvailable) {
      int leftEdge = ioBlock.getNumSamples() - maxSamplesAvailable;
      auto subBlock = ioBlock.getSubBlock(leftEdge, maxSamplesAvailable);

      for (auto &pluginBuffer : pluginBuffers) {
        // Right-align exactly `maxSamplesAvailable` samples from each buffer:
        juce::dsp::AudioBlock<float> pluginBufferAsBlock(pluginBuffer);

        // Add as many samples as we can (which is maxSamplesAvailable,
        // because subBlock is only that size):
        subBlock.add(pluginBufferAsBlock);
      }
    }

    // Delete the samples we just returned from each buffer and shift the
    // remaining content left:
    int samplesToDelete = maxSamplesAvailable;
    if (samplesToDelete) {
      for (int i = 0; i < plugins.size(); i++) {
        int samplesRemaining = samplesAvailablePerPlugin[i] - samplesToDelete;
        for (int c = 0; c < pluginBuffers[i].getNumChannels(); c++) {
          float *channelBuffer = pluginBuffers[i].getWritePointer(c);

          // Shift the remaining samples to the start of the buffer:
          std::memmove(channelBuffer, channelBuffer + samplesToDelete,
                       sizeof(float) * samplesRemaining);
        }
        samplesAvailablePerPlugin[i] -= samplesToDelete;
      }
    }

    return maxSamplesAvailable;
  }

  virtual void reset() {
    for (auto plugin : plugins) {
      if (plugin) {
        plugin->reset();
      }
    }

    for (auto buffer : pluginBuffers)
      buffer.clear();
  }

  virtual int getLatencyHint() {
    int maxHint = 0;
    for (auto plugin : plugins) {
      if (plugin) {
        maxHint = std::max(maxHint, plugin->getLatencyHint());
      }
    }
    return maxHint;
  }

protected:
  std::vector<juce::AudioBuffer<float>> pluginBuffers;
  std::vector<int> samplesAvailablePerPlugin;
};

inline void init_mix(py::module &m) {
  py::class_<Mix, PluginContainer, std::shared_ptr<Mix>>(
      m, "Mix",
      "A utility plugin that allows running other plugins in parallel. All "
      "plugins provided will be mixed equally.")
      .def(py::init([](std::vector<std::shared_ptr<Plugin>> plugins) {
             return new Mix(plugins);
           }),
           py::arg("plugins"))
      .def(py::init([]() { return new Mix({}); }))
      .def("__repr__", [](Mix &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.Mix with " << plugin.getPlugins().size()
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