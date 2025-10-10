#pragma once

#include "../JuceHeader.h"
#include "../Plugin.h"

namespace Pedalboard {

/**
 * A simple plugin used for tests that counts MIDI events passed during
 * processing while leaving audio unchanged.
 */
class MidiMonitor : public Plugin {
public:
  MidiMonitor() = default;
  virtual ~MidiMonitor() = default;

  // Store the spec for completeness; nothing is allocated.
  void prepare(const juce::dsp::ProcessSpec &spec) override {
    lastSpec = spec;
  }

  // Backwards compatibility shim for callers that do not provide MIDI data.
  int process(const juce::dsp::ProcessContextReplacing<float> &context) override {
    juce::MidiBuffer empty;
    return process(context, empty);
  }

  // Record the number of MIDI events received and pass audio through.
  int process(const juce::dsp::ProcessContextReplacing<float> &context,
              const juce::MidiBuffer &midiMessages) override {
    lastEventCount = midiMessages.getNumEvents();
    // No processing; return the buffer size unchanged.
    return context.getOutputBlock().getNumSamples();
  }

  // Clear the internal MIDI counter.
  void reset() override { lastEventCount = 0; }

  // Expose the most recently seen MIDI event count for test inspection.
  int getLastEventCount() const { return lastEventCount; }

private:
  int lastEventCount = 0;
};

inline void init_midi_monitor(py::module &m) {
  py::class_<MidiMonitor, Plugin, std::shared_ptr<MidiMonitor>>(m, "MidiMonitor")
      .def(py::init([]() { return std::make_unique<MidiMonitor>(); }))
      .def("get_last_event_count", &MidiMonitor::getLastEventCount,
           "Return the number of MIDI events provided during the last process call.")
      .def("__repr__", [](const MidiMonitor &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.MidiMonitor at " << &plugin << ">";
        return ss.str();
      });
}

} // namespace Pedalboard