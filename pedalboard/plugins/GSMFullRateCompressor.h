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
#include "../plugin_templates/FixedBlockSize.h"
#include "../plugin_templates/ForceMono.h"
#include "../plugin_templates/PrimeWithSilence.h"
#include "../plugin_templates/Resample.h"

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

class GSMFullRateCompressorInternal : public Plugin {
public:
  virtual ~GSMFullRateCompressorInternal(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                       lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                       lastSpec.numChannels != spec.numChannels;
    if (!encoder || specChanged) {
      reset();

      if (spec.sampleRate != GSM_SAMPLE_RATE) {
        throw std::runtime_error("GSMCompressor plugin must be run at " +
                                 std::to_string(GSM_SAMPLE_RATE) + "Hz!");
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

    if (ioBlock.getNumSamples() != GSM_FRAME_SIZE_SAMPLES) {
      throw std::runtime_error("GSMCompressor plugin must be passed exactly " +
                               std::to_string(GSM_FRAME_SIZE_SAMPLES) +
                               " at a time.");
    }

    if (ioBlock.getNumChannels() != 1) {
      throw std::runtime_error(
          "GSMCompressor plugin must be passed mono input!");
    }

    // Convert samples to signed 16-bit integer first,
    // then pass to the GSM Encoder, then immediately back
    // around to the GSM decoder.
    short frame[GSM_FRAME_SIZE_SAMPLES];

    juce::AudioDataConverters::convertFloatToInt16LE(
        ioBlock.getChannelPointer(0), frame, GSM_FRAME_SIZE_SAMPLES);

    // Actually do the GSM processing!
    gsm_frame encodedFrame;

    gsm_encode(encoder.getContext(), frame, encodedFrame);
    if (gsm_decode(decoder.getContext(), encodedFrame, frame) < 0) {
      throw std::runtime_error("GSM decoder could not decode frame!");
    }

    juce::AudioDataConverters::convertInt16LEToFloat(
        frame, ioBlock.getChannelPointer(0), GSM_FRAME_SIZE_SAMPLES);

    return GSM_FRAME_SIZE_SAMPLES;
  }

  void reset() override final {
    encoder.reset();
    decoder.reset();
  }

  static constexpr size_t GSM_FRAME_SIZE_SAMPLES = 160;
  static constexpr int GSM_SAMPLE_RATE = 8000;

private:
  GSMWrapper encoder;
  GSMWrapper decoder;
};

/**
 * Use the GSMFullRateCompressorInternal plugin, but:
 *  - ensure that it only ever sees fixed-size blocks of 160 samples
 *  - prime the input with a single block of silence
 *  - resample whatever input sample rate is provided down to 8kHz
 *  - only provide mono input to the plugin, and copy the mono signal
 *    back to stereo if necessary
 */
using GSMFullRateCompressor = ForceMono<Resample<
    PrimeWithSilence<
        FixedBlockSize<GSMFullRateCompressorInternal,
                       GSMFullRateCompressorInternal::GSM_FRAME_SIZE_SAMPLES>,
        float, GSMFullRateCompressorInternal::GSM_FRAME_SIZE_SAMPLES>,
    float, GSMFullRateCompressorInternal::GSM_SAMPLE_RATE>>;

inline void init_gsm_full_rate_compressor(py::module &m) {
  py::class_<GSMFullRateCompressor, Plugin,
             std::shared_ptr<GSMFullRateCompressor>>(
      m, "GSMFullRateCompressor",
      "An audio degradation/compression plugin that applies the GSM \"Full "
      "Rate\" compression algorithm to emulate the sound of a "
      "2G cellular phone connection. This plugin internally resamples the "
      "input audio to a fixed sample rate of 8kHz (required by the GSM Full "
      "Rate codec), although the quality of the resampling algorithm "
      "can be specified.")
      .def(py::init([](ResamplingQuality quality) {
             auto plugin = std::make_unique<GSMFullRateCompressor>();
             plugin->getNestedPlugin().setQuality(quality);
             return plugin;
           }),
           py::arg("quality") = ResamplingQuality::WindowedSinc8)
      .def("__repr__",
           [](const GSMFullRateCompressor &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.GSMFullRateCompressor";
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property(
          "quality",
          [](GSMFullRateCompressor &plugin) {
            return plugin.getNestedPlugin().getQuality();
          },
          [](GSMFullRateCompressor &plugin, ResamplingQuality quality) {
            return plugin.getNestedPlugin().setQuality(quality);
          });
}

}; // namespace Pedalboard