/*
 * Copyright 2026 Spotify AB
 * Licensed under the GNU Public License, Version 3.0.
 */

#include <pybind11/embed.h>

namespace py = pybind11;

#include "ExternalPlugin.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

// This is an instrument contract, independent of the host's buffer strategy:
// every enabled output must have writable storage, including mandatory buses.
class TestInstrument final : public juce::AudioPluginInstance {
public:
  explicit TestInstrument(bool auxiliaryOutput)
      : juce::AudioPluginInstance(makeBuses(auxiliaryOutput)),
        auxiliaryOutput(auxiliaryOutput) {}

  const juce::String getName() const override {
    return "Buffer regression instrument";
  }
  void
  fillInPluginDescription(juce::PluginDescription &description) const override {
    description.name = getName();
    description.isInstrument = true;
    description.numInputChannels = 0;
    description.numOutputChannels = getTotalNumOutputChannels();
  }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0; }
  void prepareToPlay(double, int) override {}
  void releaseResources() override {}
  void reset() override { noteHeld = false; }
  bool hasEditor() const override { return false; }
  juce::AudioProcessorEditor *createEditor() override { return nullptr; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String &) override {}
  void getStateInformation(juce::MemoryBlock &) override {}
  void setStateInformation(const void *, int) override {}

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midi) override {
    const int requiredChannels = getTotalNumOutputChannels();
    require(buffer.getNumChannels() >= requiredChannels,
            "Enabled output buses require " + std::to_string(requiredChannels) +
                " writable channels, but host supplied " +
                std::to_string(buffer.getNumChannels()));
    blockSizes.push_back(buffer.getNumSamples());
    for (const auto metadata : midi) {
      const auto message = metadata.getMessage();
      if (message.isNoteOn())
        noteHeld = true;
      else if (message.isNoteOff() || message.isAllNotesOff())
        noteHeld = false;
    }
    for (int channel = 0; channel < requiredChannels; ++channel) {
      auto *samples = buffer.getWritePointer(channel);
      require(samples != nullptr,
              "Enabled output channel has no writable buffer");
      // Main L/R: 0.25 / 0.5. Auxiliary L/R: 0.75 / 1.0.
      const float value = noteHeld ? 0.25f * (channel + 1) : 0.0f;
      std::fill(samples, samples + buffer.getNumSamples(), value);
    }
  }

  std::vector<int> blockSizes;

protected:
  bool isBusesLayoutSupported(const BusesLayout &layout) const override {
    if (!layout.inputBuses.isEmpty() ||
        layout.outputBuses.size() != (auxiliaryOutput ? 2 : 1))
      return false;
    for (const auto &channels : layout.outputBuses)
      if (channels != juce::AudioChannelSet::stereo())
        return false;
    return true;
  }

private:
  static BusesProperties makeBuses(bool auxiliaryOutput) {
    auto buses = BusesProperties().withOutput(
        "Main", juce::AudioChannelSet::stereo(), true);
    if (auxiliaryOutput)
      buses =
          buses.withOutput("Auxiliary", juce::AudioChannelSet::stereo(), true);
    return buses;
  }
  bool auxiliaryOutput;
  bool noteHeld = false;
};

// Injection is restricted to native subclasses. Production processing methods
// are inherited unchanged; file-based reset/re-instantiation is not exercised.
class Host final
    : public Pedalboard::ExternalPlugin<juce::PatchedVST3PluginFormat> {
public:
  Host(std::unique_ptr<juce::AudioPluginInstance> instance, float timeout)
      : ExternalPlugin(std::move(instance), timeout) {}
};

void testWarmUp(bool auxiliaryOutput) {
  auto instance = std::make_unique<TestInstrument>(auxiliaryOutput);
  auto *instrument = instance.get();
  Host host(std::move(instance), 1.0f);
  require(host.attemptToWarmUp(),
          "Instrument did not produce audio during warm-up");
  require(!instrument->blockSizes.empty(),
          "Warm-up never processed the instrument");
  require(instrument->getTotalNumOutputChannels() == (auxiliaryOutput ? 4 : 2),
          "Warm-up changed the instrument's mandatory output configuration");
}

void testRender(bool auxiliaryOutput, bool partialBlock) {
  auto instance = std::make_unique<TestInstrument>(auxiliaryOutput);
  auto *instrument = instance.get();
  Host host(std::move(instance), 0.0f);
  py::list notes;
  notes.append(py::make_tuple(py::bytes("\x90\x3c\x7f", 3), 0.0));
  // Exact binary values avoid sample-count rounding ambiguity: 32 frames.
  auto output = host.renderMIDIMessages(notes, 1.0f / 1024, 32768, 2,
                                        partialBlock ? 12 : 8, false);
  require(output.ndim() == 2 && output.shape(0) == 2 && output.shape(1) == 32,
          "MIDI render must return exactly the requested main stereo output");
  auto samples = output.unchecked<2>();
  for (int channel = 0; channel < 2; ++channel)
    for (int frame = 0; frame < 32; ++frame)
      require(samples(channel, frame) == 0.25f * (channel + 1),
              "Main output samples were missing, corrupted, or replaced by "
              "auxiliary audio");
  const std::vector<int> expected =
      partialBlock ? std::vector<int>{12, 12, 8} : std::vector<int>{8, 8, 8, 8};
  require(instrument->blockSizes == expected,
          "MIDI render did not process all full and partial blocks");
  require(
      instrument->getTotalNumOutputChannels() == (auxiliaryOutput ? 4 : 2),
      "MIDI render changed the instrument's mandatory output configuration");
}
} // namespace

int main(int argc, char **argv) {
  py::scoped_interpreter interpreter;
  try {
    require(argc == 2, "Usage: external_plugin_buffers <case>");
    // Only Python MIDI normalization is isolated. The real C++ parser, host,
    // bus negotiation, warm-up and MIDI rendering execute without substitutes.
    py::exec(R"(
import sys
import types
package = types.ModuleType("pedalboard")
package.__path__ = []
midi_utils = types.ModuleType("pedalboard.midi_utils")
midi_utils.normalize_midi_messages = lambda messages: messages
package.midi_utils = midi_utils
sys.modules["pedalboard"] = package
sys.modules["pedalboard.midi_utils"] = midi_utils
)");
    const std::string name(argv[1]);
    if (name == "single_bus_warm_up")
      testWarmUp(false);
    else if (name == "multi_bus_warm_up")
      testWarmUp(true);
    else if (name == "single_bus_render_full")
      testRender(false, false);
    else if (name == "single_bus_render_partial")
      testRender(false, true);
    else if (name == "multi_bus_render_full")
      testRender(true, false);
    else if (name == "multi_bus_render_partial")
      testRender(true, true);
    else
      throw std::runtime_error("Unknown test case: " + name);
    std::cout << "PASS " << name << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
