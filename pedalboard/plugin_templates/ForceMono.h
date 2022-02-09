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

#pragma once

#include "../JuceHeader.h"
#include "../Plugin.h"
#include <mutex>

#include "../plugins/AddLatency.h"

namespace Pedalboard {

/**
 * A template class that wraps a Pedalboard plugin,
 * but ensures that its process() function is only ever passed a mono signal.
 */
template <typename T, typename SampleType = float>
class ForceMono : public Plugin {
public:
  virtual ~ForceMono(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    juce::dsp::ProcessSpec newSpec = spec;
    newSpec.numChannels = 1;
    plugin.prepare(newSpec);
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<SampleType> &context) {
    auto ioBlock = context.getOutputBlock();

    // Mix all channels to mono first, if necessary.
    if (ioBlock.getNumChannels() > 1) {
      float channelVolume = 1.0f / ioBlock.getNumChannels();
      for (int i = 0; i < ioBlock.getNumChannels(); i++) {
        ioBlock.getSingleChannelBlock(i) *= channelVolume;
      }

      // Copy all of the latter channels into the first channel,
      // which will be used for processing:
      auto firstChannel = ioBlock.getSingleChannelBlock(0);
      for (int i = 1; i < ioBlock.getNumChannels(); i++) {
        firstChannel += ioBlock.getSingleChannelBlock(i);
      }
    }

    juce::dsp::AudioBlock<SampleType> monoBlock =
        ioBlock.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<SampleType> subContext(monoBlock);
    int samplesProcessed = plugin.process(monoBlock);

    // Copy the mono signal back out to all other channels:
    if (ioBlock.getNumChannels() > 1) {
      auto firstChannel = ioBlock.getSingleChannelBlock(0);
      for (int i = 1; i < ioBlock.getNumChannels(); i++) {
        ioBlock.getSingleChannelBlock(i).copyFrom(firstChannel);
      }
    }

    return samplesProcessed;
  }

  virtual void reset() { plugin.reset(); }

  T &getNestedPlugin() { return plugin; }

private:
  T plugin;
};

/**
 * A test plugin used to verify the behaviour of the ForceMono wrapper.
 */
class ExpectsMono : public AddLatency {
public:
  virtual ~ExpectsMono(){};

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

using ForceMonoTestPlugin = ForceMono<ExpectsMono>;

inline void init_force_mono_test_plugin(py::module &m) {
  py::class_<ForceMonoTestPlugin, Plugin, std::shared_ptr<ForceMonoTestPlugin>>(
      m, "ForceMonoTestPlugin")
      .def(py::init([]() { return std::make_unique<ForceMonoTestPlugin>(); }))
      .def("__repr__", [](const ForceMonoTestPlugin &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.ForceMonoTestPlugin";
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard