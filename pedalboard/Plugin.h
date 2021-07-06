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
};
} // namespace Pedalboard