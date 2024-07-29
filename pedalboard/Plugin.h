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

#include "BufferUtils.h"
#include "JuceHeader.h"
#include <mutex>

static constexpr int DEFAULT_BUFFER_SIZE = 8192;

namespace Pedalboard {
/**
 * A base class for all Pedalboard plugins, JUCE-derived or external.
 */
class Plugin {
public:
  virtual ~Plugin(){};

  /**
   * Prepare the data structures that will be necessary for this plugin to
   * process audio at the provided sample rate, maximum block size, and number
   * of channels.
   */
  virtual void prepare(const juce::dsp::ProcessSpec &spec) = 0;

  /**
   * Process a single buffer of audio through this plugin.
   * Returns the number of samples that were output.
   *
   * If less than a whole buffer of audio was output, the samples that
   * were produced should be right-aligned in the buffer
   * (i.e.: they should come last).
   */
  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) = 0;

  /**
   * Reset this plugin's state, clearing any internal buffers or delay lines.
   */
  virtual void reset() = 0;

  /**
   * Reset this plugin's memory of the last channel layout and/or last channel
   * count. This should usually not be called directly.
   */
  void resetLastChannelLayout() {
    lastSpec = {0};
    lastChannelLayout = {};
  };

  /**
   * Get the number of samples of latency introduced by this plugin.
   * This is the number of samples that must be provided to the plugin
   * before meaningful output will be returned.
   * Pedalboard will automatically compensate for this latency when processing
   * by using the return value from the process() call, but this hint can
   * make processing more efficient.
   *
   * This function will only be called after prepare(), so it can take into
   * account variables like the current sample rate, maximum block size, and
   * other plugin parameters.
   *
   * Returning a value from getLatencyHint() that is larger than necessary will
   * allocate that many extra samples during processing, increasing memory
   * usage. Returning a value that is too small will cause memory to be
   * reallocated during rendering, impacting rendering speed.
   */
  virtual int getLatencyHint() { return 0; }

  /**
   * Returns true iff this plugin accepts audio input (i.e.: is an effect).
   */
  virtual bool acceptsAudioInput() { return true; }

  // A mutex to gate access to this plugin, as its internals may not be
  // thread-safe. Note: use std::lock or std::scoped_lock when locking multiple
  // plugins to avoid deadlocking.
  std::mutex mutex;

  template <typename T>
  ChannelLayout parseAndCacheChannelLayout(
      const py::array_t<T, py::array::c_style> inputArray,
      std::optional<int> channelCountHint = {}) {

    if (!channelCountHint && lastSpec.numChannels != 0) {
      channelCountHint = {lastSpec.numChannels};
    }

    if (lastChannelLayout) {
      try {
        lastChannelLayout = detectChannelLayout(inputArray, channelCountHint);
      } catch (...) {
        // Use the last cached layout.
      }
    } else {
      // We have no cached layout; detect it now and raise if necessary:
      try {
        lastChannelLayout = detectChannelLayout(inputArray, channelCountHint);
      } catch (const std::exception &e) {
        throw std::runtime_error(
            std::string(e.what()) +
            " Provide a non-square array first to allow Pedalboard to "
            "determine which dimension corresponds with the number of channels "
            "and which dimension corresponds with the number of samples.");
      }
    }

    return *lastChannelLayout;
  }

protected:
  juce::dsp::ProcessSpec lastSpec = {0};
  std::optional<ChannelLayout> lastChannelLayout = {};
};
} // namespace Pedalboard