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

#include "../JucePlugin.h"
#include <cmath>

namespace Pedalboard {
template <typename SampleType> class Bitcrush : public Plugin {
public:
  SampleType getBitDepth() const { return bitDepth; }
  void setBitDepth(const SampleType value) {
    if (value < 0.0 || value > 32) {
      throw std::range_error("Bit depth must be between 0.0 and 32.0 bits.");
    }
    bitDepth = value;
  };

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    scaleFactor = pow(2, bitDepth);
    inverseScaleFactor = 1.0 / scaleFactor;
  }
  virtual void reset() override {}

  virtual int process(
      const juce::dsp::ProcessContextReplacing<SampleType> &context) override {
    auto block = context.getOutputBlock();

    block.multiplyBy(scaleFactor);

    for (int c = 0; c < block.getNumChannels(); c++) {
      SampleType *channelPointer = block.getChannelPointer(c);

      // To allow for some SIMD optimization:
      int numIterations = block.getNumSamples() / INNER_LOOP_DIMENSION;

      for (int i = 0; i < numIterations; i++) {
        for (int j = 0; j < INNER_LOOP_DIMENSION; j++) {
          channelPointer[(i * INNER_LOOP_DIMENSION) + j] =
              nearbyintf(channelPointer[(i * INNER_LOOP_DIMENSION) + j]);
        }
      }

      for (int i = numIterations * INNER_LOOP_DIMENSION;
           i < block.getNumSamples(); i++) {
        channelPointer[i] = nearbyintf(channelPointer[i]);
      }
    }

    block.multiplyBy(inverseScaleFactor);
    return block.getNumSamples();
  }

private:
  SampleType bitDepth = 8.0f;

  SampleType scaleFactor = 1.0f;
  SampleType inverseScaleFactor = 1.0f;

  static constexpr int INNER_LOOP_DIMENSION = 16;
};

inline void init_bitcrush(py::module &m) {
  py::class_<Bitcrush<float>, Plugin, std::shared_ptr<Bitcrush<float>>>(
      m, "Bitcrush",
      "A plugin that reduces the signal to a given bit depth, giving the audio "
      "a lo-fi, digitized sound. Floating-point bit depths are supported.")
      .def(py::init([](float bitDepth) {
             auto bitcrush = std::make_unique<Bitcrush<float>>();
             bitcrush->setBitDepth(bitDepth);
             return bitcrush;
           }),
           py::arg("bit_depth") = 8)
      .def("__repr__",
           [](const Bitcrush<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Bitcrush";
             ss << " bit_depth=" << plugin.getBitDepth();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("bit_depth", &Bitcrush<float>::getBitDepth,
                    &Bitcrush<float>::setBitDepth);
}
}; // namespace Pedalboard
