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

#include "../Plugin.h"
extern "C" {
#include <gsm.h>
}

namespace Pedalboard {

/*
 * A small C++ wrapper around the C-based libgsm object.
 * Used mostly to avoid leaking memory.
 */
class GSMWrapper {
public:
  GSMWrapper() {}
  ~GSMWrapper() { reset(); }

  operator bool() const { return _gsm != nullptr; }

  void reset() {
    gsm_destroy(_gsm);
    _gsm = nullptr;
  }

  gsm getContext() {
    if (!_gsm)
      _gsm = gsm_create();
    return _gsm;
  }

private:
  gsm _gsm = nullptr;
};

class GSMCompressor : public Plugin {
public:
  virtual ~GSMCompressor(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (!encoder || specChanged) {
      reset();

      if (spec.sampleRate != 8000) {
        throw std::domain_error(
            "GSM compression currently only works at 8kHz.");
      }

      if (spec.numChannels != 1) {
        throw std::domain_error(
            "GSM compression currently only works on mono signals.");
      }

      if (spec.maximumBlockSize % GSM_FRAME_SIZE_SAMPLES != 0) {
        throw std::domain_error("GSM compression currently requires a buffer "
                                "size of a multiple of " +
                                std::to_string(GSM_FRAME_SIZE_SAMPLES) + ".");
      }

      if (!encoder.getContext()) {
        throw std::runtime_error("Failed to initialize GSM encoder.");
      }
      if (!decoder.getContext()) {
        throw std::runtime_error("Failed to initialize GSM decoder.");
      }

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override final {
    auto ioBlock = context.getOutputBlock();

    for (int blockStart = 0; blockStart < ioBlock.getNumSamples();
         blockStart += GSM_FRAME_SIZE_SAMPLES) {
      int blockEnd = blockStart + GSM_FRAME_SIZE_SAMPLES;
      if (blockEnd > ioBlock.getNumSamples())
        blockEnd = ioBlock.getNumSamples();

      int blockSize = blockEnd - blockStart;

      // Convert samples to signed 16-bit integer first,
      // then pass to the GSM Encoder, then immediately back
      // around to the GSM decoder.
      short frame[GSM_FRAME_SIZE_SAMPLES];

      juce::AudioDataConverters::convertFloatToInt16LE(
          ioBlock.getChannelPointer(0) + blockStart, frame,
          GSM_FRAME_SIZE_SAMPLES);

      gsm_frame encodedFrame;

      gsm_encode(encoder.getContext(), frame, encodedFrame);
      if (gsm_decode(decoder.getContext(), encodedFrame, frame) < 0) {
        throw std::runtime_error("GSM decoder could not decode frame!");
      }

      juce::AudioDataConverters::convertInt16LEToFloat(
          frame, ioBlock.getChannelPointer(0) + blockStart,
          GSM_FRAME_SIZE_SAMPLES);
    }

    return ioBlock.getNumSamples();
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();

    samplesProduced = 0;
    encoderInStreamLatency = 0;
  }

protected:
  virtual int getLatencyHint() override { return 0; }

private:
  GSMWrapper encoder;
  GSMWrapper decoder;

  static constexpr size_t GSM_FRAME_SIZE_SAMPLES = 160;
};

inline void init_gsm_compressor(py::module &m) {
  py::class_<GSMCompressor, Plugin>(
      m, "GSMCompressor",
      "Apply an GSM compressor to emulate the sound of a GSM (\"2G\") cellular "
      "phone connection.")
      .def(py::init([]() { return new GSMCompressor(); }))
      .def("__repr__", [](const GSMCompressor &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.GSMCompressor";
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

}; // namespace Pedalboard