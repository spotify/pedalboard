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

namespace Pedalboard {
/**
 * A base class for all Pedalboard plugins, JUCE-derived or external.
 */
class Plugin {
public:
  virtual ~Plugin(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) = 0;

  virtual void
  process(const juce::dsp::ProcessContextReplacing<float> &context) = 0;

  virtual void reset() = 0;

  // A mutex to gate access to this plugin, as its internals may not be
  // thread-safe. Note: use std::lock or std::scoped_lock when locking multiple
  // plugins to avoid deadlocking.
  std::mutex mutex;

protected:
  juce::dsp::ProcessSpec lastSpec;
};
} // namespace Pedalboard