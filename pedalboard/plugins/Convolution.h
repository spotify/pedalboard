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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"

#include "../juce_overrides/juce_BlockingConvolution.h"

namespace Pedalboard {

/**
 * Quick wrapper around juce::dsp::BlockingConvolution to
 * allow mixing at arbitrary levels.
 */
class ConvolutionWithMix {
public:
  ConvolutionWithMix() = default;

  juce::dsp::BlockingConvolution &getConvolution() { return convolution; }

  void setMix(double newMix) noexcept {
    mixer.setWetMixProportion(newMix);
    mix = newMix;
  }

  double getMix() const noexcept { return mix; }

  void setImpulseResponseFilename(std::string &filename) {
    impulseResponseFilename = filename;
  }

  const std::string &getImpulseResponseFilename() const {
    return impulseResponseFilename;
  }

  void prepare(const juce::dsp::ProcessSpec &spec) {
    convolution.prepare(spec);
    mixer.prepare(spec);
    mixer.setWetMixProportion(mix);
  }

  void reset() noexcept {
    convolution.reset();
    mixer.reset();
    mixer.setWetMixProportion(mix);
  }

  template <typename ProcessContext>
  void process(const ProcessContext &context) noexcept {
    mixer.pushDrySamples(context.getInputBlock());
    convolution.process(context);
    mixer.mixWetSamples(context.getOutputBlock());
  }

private:
  juce::dsp::BlockingConvolution convolution;
  juce::dsp::DryWetMixer<float> mixer;
  float mix = 1.0;
  std::string impulseResponseFilename;
};

inline void init_convolution(py::module &m) {
  py::class_<JucePlugin<ConvolutionWithMix>, Plugin,
             std::shared_ptr<JucePlugin<ConvolutionWithMix>>>(
      m, "Convolution",
      "An audio convolution, suitable for things like speaker simulation or "
      "reverb modeling.")
      .def(py::init([](std::string &impulseResponseFilename, float mix) {
             py::gil_scoped_release release;
             auto plugin = std::make_unique<JucePlugin<ConvolutionWithMix>>();
             // Load the IR file on construction, to handle errors
             auto inputFile = juce::File(impulseResponseFilename);
             // Test opening the file before we pass it to
             // loadImpulseResponse, which reloads it in the background:
             {
               juce::FileInputStream stream(inputFile);
               if (!stream.openedOk()) {
                 throw std::runtime_error("Unable to load impulse response: " +
                                          impulseResponseFilename);
               }
             }

             plugin->getDSP().getConvolution().loadImpulseResponse(
                 inputFile, juce::dsp::Convolution::Stereo::yes,
                 juce::dsp::Convolution::Trim::no, 0);
             plugin->getDSP().setMix(mix);
             return plugin;
           }),
           py::arg("impulse_response_filename"), py::arg("mix") = 1.0)
      .def("__repr__",
           [](JucePlugin<ConvolutionWithMix> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Convolution";
             ss << " impulse_response_filename="
                << plugin.getDSP().getImpulseResponseFilename();
             ss << " mix=" << plugin.getDSP().getMix();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property_readonly(
          "impulse_response_filename",
          [](JucePlugin<ConvolutionWithMix> &plugin) {
            return plugin.getDSP().getImpulseResponseFilename();
          })
      .def_property(
          "mix",
          [](JucePlugin<ConvolutionWithMix> &plugin) {
            return plugin.getDSP().getMix();
          },
          [](JucePlugin<ConvolutionWithMix> &plugin, double newMix) {
            return plugin.getDSP().setMix(newMix);
          });
}
}; // namespace Pedalboard