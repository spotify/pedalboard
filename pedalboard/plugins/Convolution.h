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

  const std::optional<std::string> &getImpulseResponseFilename() const {
    return impulseResponseFilename;
  }

  void setImpulseResponse(juce::AudioBuffer<float> &&ir) {
    impulseResponse = {std::move(ir)};
  }

  const std::optional<juce::AudioBuffer<float>> &getImpulseResponse() const {
    return {impulseResponse};
  }

  void setSampleRate(double sr) { sampleRate = {sr}; }

  const std::optional<double> &getSampleRate() const { return sampleRate; }

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
  std::optional<std::string> impulseResponseFilename;
  std::optional<juce::AudioBuffer<float>> impulseResponse;
  std::optional<double> sampleRate;
};

inline void init_convolution(py::module &m) {
  py::class_<JucePlugin<ConvolutionWithMix>, Plugin,
             std::shared_ptr<JucePlugin<ConvolutionWithMix>>>(
      m, "Convolution",
      "An audio convolution, suitable for things like speaker simulation or "
      "reverb modeling.\n\n"
      "The convolution impulse response can be specified either by filename or "
      "as a 32-bit floating point NumPy array. If a NumPy array is provided, "
      "the ``sample_rate`` argument must also be provided to indicate the "
      "sample rate of the impulse response.\n\n*Support for passing NumPy "
      "arrays as impulse responses introduced in v0.9.10.*")
      .def(py::init([](std::variant<std::string,
                                    py::array_t<float, py::array::c_style>>
                           impulseResponse,
                       float mix, std::optional<double> sampleRate) {
             auto plugin = std::make_unique<JucePlugin<ConvolutionWithMix>>();

             if (auto *impulseResponseFilename =
                     std::get_if<std::string>(&impulseResponse)) {
               py::gil_scoped_release release;
               // Load the IR file on construction, to handle errors
               auto inputFile = juce::File(*impulseResponseFilename);
               // Test opening the file before we pass it to
               // loadImpulseResponse, which reloads it in the background:
               {
                 juce::FileInputStream stream(inputFile);
                 if (!stream.openedOk()) {
                   throw std::runtime_error(
                       "Unable to load impulse response: " +
                       *impulseResponseFilename);
                 }
               }

               plugin->getDSP().getConvolution().loadImpulseResponse(
                   inputFile, juce::dsp::Convolution::Stereo::yes,
                   juce::dsp::Convolution::Trim::no, 0);
               plugin->getDSP().setImpulseResponseFilename(
                   *impulseResponseFilename);
             } else if (auto *inputArray =
                            std::get_if<py::array_t<float, py::array::c_style>>(
                                &impulseResponse)) {
               if (!sampleRate) {
                 throw std::runtime_error(
                     "sample_rate must be provided when passing a numpy array "
                     "as an impulse response.");
               }
               plugin->getDSP().getConvolution().loadImpulseResponse(
                   std::move(copyPyArrayIntoJuceBuffer(*inputArray)),
                   *sampleRate, juce::dsp::Convolution::Stereo::yes,
                   juce::dsp::Convolution::Trim::no,
                   juce::dsp::Convolution::Normalise::yes);

               plugin->getDSP().setImpulseResponse(
                   copyPyArrayIntoJuceBuffer(*inputArray));
               plugin->getDSP().setSampleRate(*sampleRate);
             }
             plugin->getDSP().setMix(mix);
             return plugin;
           }),
           py::arg("impulse_response_filename"), py::arg("mix") = 1.0,
           py::arg("sample_rate") = py::none())
      .def("__repr__",
           [](JucePlugin<ConvolutionWithMix> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Convolution";
             if (plugin.getDSP().getImpulseResponseFilename()) {
               ss << " impulse_response_filename="
                  << *plugin.getDSP().getImpulseResponseFilename();
             } else if (plugin.getDSP().getImpulseResponse()) {
               ss << " impulse_response=<"
                  << plugin.getDSP().getImpulseResponse()->getNumSamples()
                  << " samples of "
                  << plugin.getDSP().getImpulseResponse()->getNumChannels()
                  << "-channel audio at " << *plugin.getDSP().getSampleRate()
                  << " Hz>";
             }

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
      .def_property_readonly(
          "impulse_response",
          [](JucePlugin<ConvolutionWithMix> &plugin)
              -> std::optional<py::array_t<float, py::array::c_style>> {
            if (plugin.getDSP().getImpulseResponse()) {
              return {copyJuceBufferIntoPyArray(
                  *plugin.getDSP().getImpulseResponse(),
                  ChannelLayout::NotInterleaved, 0, 2)};
            } else {
              return {};
            }
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