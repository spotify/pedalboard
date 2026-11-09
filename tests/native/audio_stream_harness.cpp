/* Hardware-independent regression harness for the production AudioStream.
 * Only device discovery is substituted; JUCE's real manager, buffers, and
 * AudioStream constructor/callback execute normally. Each case is a subprocess
 * because a regression may cause SIGFPE or SIGSEGV.
 */
#include "../../pedalboard/JuceHeader.h"
// JuceHeader exposes global colour constants even for audio-only consumers.
#include <juce_graphics/colour/juce_Colour.cpp>
#include <optional>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
namespace py = pybind11;
#include "../../pedalboard/io/PythonFileLike.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>

namespace {
struct OpenRequest {
  juce::String input, output;
};
std::vector<OpenRequest> openedDevices;
bool sparseOutput = false;
class TestDevice : public juce::AudioIODevice {
public:
  TestDevice(const juce::String &name, const juce::String &type,
             const juce::String &input, const juce::String &output)
      : AudioIODevice(name, type), requestedInput(input),
        requestedOutput(output) {}
  juce::StringArray getOutputChannelNames() override { return {"L", "R"}; }
  juce::StringArray getInputChannelNames() override { return {"L", "R"}; }
  juce::Array<double> getAvailableSampleRates() override {
    return {44100.0, 48000.0};
  }
  juce::Array<int> getAvailableBufferSizes() override { return {16, 32}; }
  int getDefaultBufferSize() override { return 16; }
  juce::String open(const juce::BigInteger &in, const juce::BigInteger &out,
                    double rate, int size) override {
    openedDevices.push_back({requestedInput, requestedOutput});
    inputs = in;
    outputs = out;
    if (sparseOutput) {
      outputs.clear();
      outputs.setBit(1);
    }
    sampleRate = rate;
    bufferSize = size;
    opened = true;
    return {};
  }
  void close() override {
    stop();
    opened = false;
  }
  bool isOpen() override { return opened; }
  void start(juce::AudioIODeviceCallback *next) override {
    callback = next;
    if (callback)
      callback->audioDeviceAboutToStart(this);
  }
  void stop() override {
    if (callback)
      callback->audioDeviceStopped();
    callback = nullptr;
  }
  bool isPlaying() override { return callback != nullptr; }
  juce::String getLastError() override { return {}; }
  int getCurrentBufferSizeSamples() override { return bufferSize; }
  double getCurrentSampleRate() override { return sampleRate; }
  int getCurrentBitDepth() override { return 32; }
  juce::BigInteger getActiveOutputChannels() const override { return outputs; }
  juce::BigInteger getActiveInputChannels() const override { return inputs; }
  int getOutputLatencyInSamples() override { return 0; }
  int getInputLatencyInSamples() override { return 0; }

private:
  juce::String requestedInput, requestedOutput;
  juce::BigInteger inputs, outputs;
  double sampleRate = 48000;
  int bufferSize = 32;
  bool opened = false;
  juce::AudioIODeviceCallback *callback = nullptr;
};

class TestDeviceType : public juce::AudioIODeviceType {
public:
  explicit TestDeviceType(const juce::String &name) : AudioIODeviceType(name) {}
  void scanForDevices() override {}
  juce::StringArray getDeviceNames(bool input) const override {
    return {getTypeName() + (input ? " Input" : " Output")};
  }
  int getDefaultDeviceIndex(bool) const override { return 0; }
  int getIndexOfDevice(juce::AudioIODevice *, bool) const override { return 0; }
  bool hasSeparateInputsAndOutputs() const override { return true; }
  juce::AudioIODevice *createDevice(const juce::String &output,
                                    const juce::String &input) override {
    if ((!output.isEmpty() &&
         !getDeviceNames(false)[0].equalsIgnoreCase(output.trim())) ||
        (!input.isEmpty() &&
         !getDeviceNames(true)[0].equalsIgnoreCase(input.trim())))
      return nullptr;
    return new TestDevice(output.isEmpty() ? input : output, getTypeName(),
                          input, output);
  }
};
} // namespace

namespace juce {
class TestAudioDeviceManager : public AudioDeviceManager {
public:
  void createAudioDeviceTypes(OwnedArray<AudioIODeviceType> &types) override {
    types.add(new TestDeviceType("First"));
    types.add(new TestDeviceType("Second"));
  }
};
} // namespace juce

// A compile-time dependency substitution local to this test translation unit.
// Include JUCE first so this affects only AudioStream's manager member and
// local managers, without replacing any JUCE implementation or processing
// algorithm.
#define AudioDeviceManager TestAudioDeviceManager
#include "../../pedalboard/io/AudioStream.h"
#undef AudioDeviceManager

namespace {
constexpr int samples = 8;
using Channel = std::array<float, samples>;
void require(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
void expectSilence(const Channel &channel) {
  require(std::all_of(channel.begin(), channel.end(),
                      [](float x) { return x == 0; }),
          "Expected every output sample to be silent");
}

std::shared_ptr<Pedalboard::AudioStream> makeStream(bool input, bool output) {
  return std::make_shared<Pedalboard::AudioStream>(
      input ? std::optional<std::string>("First Input") : std::nullopt,
      output ? std::optional<std::string>("First Output") : std::nullopt,
      std::nullopt, 48000, 32, true, 2, 2);
}

class DoubleGain : public Pedalboard::Plugin {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) override {
    preparedChannels = spec.numChannels;
  }
  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override {
    auto block = context.getOutputBlock();
    block.multiplyBy(2.0f);
    ++processed;
    return static_cast<int>(block.getNumSamples());
  }
  void reset() override {}
  std::atomic<int> processed{0};
  std::atomic<unsigned int> preparedChannels{0};
};

void testPlugin(const std::string &scenario) {
  const bool silent = scenario == "plugin_zero_input";
  sparseOutput = !silent;
  auto stream = makeStream(true, true);
  auto gain = std::make_shared<DoubleGain>();
  stream->setPedalboard(std::make_shared<Pedalboard::Chain>(
      std::vector<std::shared_ptr<Pedalboard::Plugin>>{gain}));
  stream->start();
  Channel input{1, -1, 0.5, -0.5, 0.25, -0.25, 0, 1};
  const Channel expected{2, -2, 1, -1, 0.5, -0.5, 0, 2};
  Channel left, output;
  const float *inputs[] = {input.data()};
  float *outputs[] = {silent ? left.data() : nullptr, output.data()};
  // Plugin updates are asynchronous. Wait for the observer to install it by
  // observing its processing, bounded independently of machine speed.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  do {
    left.fill(123);
    output.fill(123);
    stream->audioDeviceIOCallback(silent ? nullptr : inputs, silent ? 0 : 1,
                                  outputs, 2, samples);
    if (gain->processed.load() > 0)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  require(gain->processed.load() > 0, "Live plugin was never processed");
  require(gain->preparedChannels.load() == (silent ? 2 : 1),
          "Plugin preparation must reflect active device outputs");
  if (silent) {
    expectSilence(left);
    expectSilence(output);
  } else
    require(output == expected, "Plugin must apply gain to active output");
}

const std::array<float, 24> firstChunk{1,  2,  3,  4,  5,  6,  7,  8,
                                       9,  10, 11, 12, 13, 14, 15, 16,
                                       17, 18, 19, 20, 21, 22, 23, 24};
const std::array<float, 16> wrappedChunk{101, 102, 103, 104, 105, 106,
                                         107, 108, 109, 110, 111, 112,
                                         113, 114, 115, 116};

void testRecordingWrap() {
  auto stream = makeStream(true, false);
  stream->start();
  const float *first[] = {firstChunk.data(), firstChunk.data()};
  stream->audioDeviceIOCallback(first, 2, nullptr, 0, firstChunk.size());
  auto initial = stream->read(firstChunk.size());
  for (int channel = 0; channel < 2; ++channel)
    for (int i = 0; i < static_cast<int>(firstChunk.size()); ++i)
      require(initial.getSample(channel, i) == firstChunk[i],
              "Recording changed initial sample order");
  const float *wrapped[] = {nullptr, wrappedChunk.data()};
  stream->audioDeviceIOCallback(wrapped, 2, nullptr, 0, wrappedChunk.size());
  auto recorded = stream->read(wrappedChunk.size());
  for (int i = 0; i < static_cast<int>(wrappedChunk.size()); ++i) {
    require(recorded.getSample(0, i) == 0,
            "Inactive recording channel must be silent across FIFO wrap");
    require(recorded.getSample(1, i) == wrappedChunk[i],
            "Recording changed wrapped sample order");
  }
}

void testRecordingMissingInputs(bool noInputs) {
  auto stream = makeStream(true, false);
  stream->start();
  const float *first[] = {firstChunk.data(), firstChunk.data()};
  stream->audioDeviceIOCallback(first, 2, nullptr, 0, firstChunk.size());
  stream->read(firstChunk.size());
  const float *next[] = {wrappedChunk.data()};
  stream->audioDeviceIOCallback(noInputs ? nullptr : next, noInputs ? 0 : 1,
                                nullptr, 0, wrappedChunk.size());
  auto recorded = stream->read(wrappedChunk.size());
  for (int i = 0; i < static_cast<int>(wrappedChunk.size()); ++i) {
    require(recorded.getSample(0, i) == (noInputs ? 0 : wrappedChunk[i]),
            "Recording first channel lost samples or retained stale data");
    require(recorded.getSample(1, i) == 0,
            "Missing recording channel must contain silence");
  }
}

template <size_t N>
void queueChunk(Pedalboard::AudioStream &stream,
                const std::array<float, N> &data) {
  juce::AudioBuffer<float> buffer(2, N);
  for (int c = 0; c < 2; ++c)
    for (int i = 0; i < static_cast<int>(N); ++i)
      buffer.setSample(c, i, data[i]);
  stream.write(buffer);
}

void testPlaybackWrap() {
  auto stream = makeStream(false, true);
  stream->start();
  queueChunk(*stream, firstChunk);
  std::array<float, 24> left, right;
  float *first[] = {left.data(), right.data()};
  stream->audioDeviceIOCallback(nullptr, 0, first, 2, firstChunk.size());
  require(left == firstChunk && right == firstChunk,
          "Playback changed initial sample order");
  queueChunk(*stream, wrappedChunk);
  std::array<float, 16> output;
  output.fill(123);
  float *wrapped[] = {nullptr, output.data()};
  stream->audioDeviceIOCallback(nullptr, 0, wrapped, 2, wrappedChunk.size());
  require(output == wrappedChunk, "Playback changed wrapped sample order");
  Channel silentLeft, silentRight;
  silentLeft.fill(123);
  silentRight.fill(123);
  float *empty[] = {silentLeft.data(), silentRight.data()};
  stream->audioDeviceIOCallback(nullptr, 0, empty, 2, samples);
  expectSilence(silentLeft);
  expectSilence(silentRight);
}

void testBackendVariant(const std::string &scenario) {
  const bool inputOnly = scenario == "second_backend_input_only";
  const bool outputOnly = scenario == "second_backend_output_only";
  const std::string backend =
      scenario.find("first_backend") == 0 ? "First" : "Second";
  const bool variant = scenario.find("name_variant") != std::string::npos;
  const std::string input =
      variant ? "  " + backend + " iNpUt  " : backend + " Input";
  const std::string output =
      variant ? "  " + backend + " oUtPuT  " : backend + " Output";
  openedDevices.clear();
  Pedalboard::AudioStream stream(
      outputOnly ? std::nullopt : std::optional<std::string>(input),
      inputOnly ? std::nullopt : std::optional<std::string>(output),
      std::nullopt, 48000, 32, true, 2, 2);
  const auto setup = stream.getAudioDeviceSetup();
  require(!openedDevices.empty(), "Requested device was never opened");
  for (const auto &request : openedDevices) {
    require(outputOnly ? request.input.isEmpty()
                       : request.input.trim().equalsIgnoreCase(
                             juce::String(backend + " Input")),
            "Stream opened an unrelated input device");
    require(inputOnly ? request.output.isEmpty()
                      : request.output.trim().equalsIgnoreCase(
                            juce::String(backend + " Output")),
            "Stream opened an unrelated output device");
  }
  require(outputOnly ? setup.inputDeviceName.isEmpty()
                     : setup.inputDeviceName.trim().equalsIgnoreCase(
                           juce::String(backend + " Input")),
          "Requested backend input was not retained");
  require(inputOnly ? setup.outputDeviceName.isEmpty()
                    : setup.outputDeviceName.trim().equalsIgnoreCase(
                          juce::String(backend + " Output")),
          "Requested backend output was not retained");
  require(setup.sampleRate == 48000 && setup.bufferSize == 32,
          "Backend selection changed requested sample rate or buffer size");
  require(stream.getNumInputChannels() == (outputOnly ? 0 : 2),
          "Wrong input channel count");
  require(stream.getNumOutputChannels() == (inputOnly ? 0 : 2),
          "Wrong output channel count");
}

void testEmptyDeviceName(const std::string &scenario) {
  const std::string backend =
      scenario.find("first_backend") == 0 ? "First" : "Second";
  const bool emptyInput = scenario.find("empty_input") != std::string::npos;
  Pedalboard::AudioStream stream(
      std::optional<std::string>(emptyInput ? "" : backend + " Input"),
      std::optional<std::string>(emptyInput ? backend + " Output" : ""),
      std::nullopt, 48000, 32, true, 2, 2);
  const auto setup = stream.getAudioDeviceSetup();
  require(
      setup.inputDeviceName.toStdString() == backend + " Input",
      "Present empty input name must choose selected backend's default input");
  require(setup.outputDeviceName.toStdString() == backend + " Output",
          "Present empty output name must choose selected backend's default "
          "output");
  require(stream.getNumInputChannels() == 2,
          "Empty input name must retain requested input channels");
  require(stream.getNumOutputChannels() == 2,
          "Empty output name must retain requested output channels");
  require(setup.sampleRate == 48000 && setup.bufferSize == 32,
          "Default device choice changed requested sample rate or buffer size");
}

void run(const std::string &scenario) {
  if (scenario.find("empty_input") != std::string::npos ||
      scenario.find("empty_output") != std::string::npos) {
    testEmptyDeviceName(scenario);
    return;
  }
  if (scenario == "record_zero_inputs" || scenario == "record_fewer_inputs") {
    testRecordingMissingInputs(scenario == "record_zero_inputs");
    return;
  }
  if (scenario == "plugin_null_output" || scenario == "plugin_zero_input") {
    testPlugin(scenario);
    return;
  }
  if (scenario == "record_fifo_wrap") {
    testRecordingWrap();
    return;
  }
  if (scenario == "play_fifo_wrap") {
    testPlaybackWrap();
    return;
  }
  if (scenario == "second_backend_input_only" ||
      scenario == "second_backend_output_only" ||
      scenario.find("name_variant") != std::string::npos) {
    testBackendVariant(scenario);
    return;
  }

  const bool recordOnly = scenario == "record_null_input";
  const bool playOnly = scenario == "play_null_output";
  const std::string backend = scenario == "second_backend" ? "Second" : "First";
  Pedalboard::AudioStream stream(
      playOnly ? std::nullopt : std::optional<std::string>(backend + " Input"),
      recordOnly ? std::nullopt
                 : std::optional<std::string>(backend + " Output"),
      std::nullopt, 48000, 32, true, 2, 2);
  if (scenario == "second_backend" || scenario == "first_backend") {
    auto setup = stream.getAudioDeviceSetup();
    require(setup.inputDeviceName.toStdString() == backend + " Input",
            "Requested input was not selected");
    require(setup.outputDeviceName.toStdString() == backend + " Output",
            "Requested output was not selected");
    require(setup.sampleRate == 48000,
            "Requested sample rate was not retained");
    require(setup.bufferSize == 32, "Requested buffer size was not retained");
    return;
  }

  require(stream.getSampleRate() == 48000,
          "Wrong sample rate on first backend");
  require(stream.getAudioDeviceSetup().bufferSize == 32,
          "Wrong first backend buffer size");
  if (recordOnly)
    stream.start();

  Channel input{1, -1, 0.5, -0.5, 0.25, -0.25, 0, 1};
  Channel left, right;
  left.fill(123);
  right.fill(123);
  const float *inputs[] = {input.data(), input.data()};
  float *outputs[] = {left.data(), right.data()};
  int numInputs = 2;
  if (scenario == "zero_input")
    numInputs = 0;
  if (scenario == "null_input" || recordOnly)
    inputs[0] = nullptr;
  if (scenario == "null_output" || playOnly)
    outputs[0] = nullptr;
  if (scenario == "mono")
    numInputs = 1;
  stream.audioDeviceIOCallback(numInputs ? inputs : nullptr, numInputs,
                               recordOnly ? nullptr : outputs,
                               recordOnly ? 0 : 2, samples);
  if (recordOnly) {
    auto recorded = stream.read(samples);
    require(recorded.getNumChannels() == 2, "Wrong recorded channel count");
    for (int i = 0; i < samples; ++i) {
      require(recorded.getSample(0, i) == 0,
              "Inactive input must record silence");
      require(recorded.getSample(1, i) == input[i],
              "Active input must still record");
    }
  } else if (playOnly) {
    expectSilence(right);
  } else if (scenario == "zero_input") {
    expectSilence(left);
    expectSilence(right);
  } else if (scenario == "null_input") {
    expectSilence(left);
    require(right == input, "Active input must still pass through");
  } else if (scenario == "null_output") {
    require(right == input, "Active output must still receive audio");
  } else {
    require(left == input && right == input,
            "Mono input must reach both outputs");
  }
}
} // namespace

int main(int argc, char **argv) {
  pybind11::scoped_interpreter python;
  juce::ScopedJuceInitialiser_GUI juce;
  try {
    require(argc == 2, "Expected one scenario name");
    run(argv[1]);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
