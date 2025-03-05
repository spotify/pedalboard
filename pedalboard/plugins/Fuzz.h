/*
 * pedalboard
 * Copyright 2025 Spotify AB
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

#include <cmath>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"
#include <juce_dsp/juce_dsp.h>

namespace Pedalboard {

template <typename SampleType>
class Fuzz
    : public JucePlugin<juce::dsp::ProcessorChain<
          juce::dsp::Gain<SampleType>,       // Drive stage
          juce::dsp::WaveShaper<SampleType>, // Hard diode clipping stage
          juce::dsp::WaveShaper<SampleType>, // Soft clipping stage (tanh)
          juce::dsp::IIR::Filter<SampleType> // Tone control (low-pass filter)
          >> {
public:
  Fuzz()
      : driveDecibels(static_cast<SampleType>(25)),
        toneHz(static_cast<SampleType>(800)) {}

  void setDriveDecibels(const float f) noexcept { driveDecibels = f; }
  float getDriveDecibels() const noexcept { return driveDecibels; }

  void setToneHz(const float f) noexcept { toneHz = f; }
  float getToneHz() const noexcept { return toneHz; }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    // Prepare the four-stage DSP chain
    JucePlugin<juce::dsp::ProcessorChain<
        juce::dsp::Gain<SampleType>, juce::dsp::WaveShaper<SampleType>,
        juce::dsp::WaveShaper<SampleType>,
        juce::dsp::IIR::Filter<SampleType>>>::prepare(spec);

    // First stage (1st): apply gain to drive
    this->getDSP().template get<gainIndex>().setGainDecibels(
        getDriveDecibels());

    // First stage (2nd): diode hard clipping with threshold 0.25
    this->getDSP().template get<clipperIndex>().functionToUse =
        [](SampleType x) -> SampleType {
      constexpr SampleType threshold = static_cast<SampleType>(0.25);
      if (x > threshold)
        return threshold;
      if (x < -threshold)
        return -threshold;
      return x;
    };

    // Second stage: soft clipping via tanh
    this->getDSP().template get<shaperIndex>().functionToUse =
        [](SampleType x) -> SampleType {
      return static_cast<SampleType>(std::tanh(x));
    };

    // Third stage: Tone control via low-pass filter
    auto coeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowPass(
        spec.sampleRate, toneHz);
    this->getDSP().template get<filterIndex>().coefficients = coeffs;
  }

private:
  SampleType driveDecibels;
  SampleType toneHz;

  enum { gainIndex, clipperIndex, shaperIndex, filterIndex };
};

inline void init_fuzz(py::module &m) {
  py::class_<Fuzz<float>, Plugin, std::shared_ptr<Fuzz<float>>>(
      m, "Fuzz",
      "A Fuzz effect emulating a classic fuzz pedal.\\n\\n"
      "It features a two-stage clipping process: first a hard diode clipping "
      "(threshold=0.25),"
      "then a soft clipping via tanh, followed by a tone control stage "
      "implemented as a low-pass filter.\\n")
      .def(py::init([](float drive_db, float tone_hz) {
             auto plugin = std::make_unique<Fuzz<float>>();
             plugin->setDriveDecibels(drive_db);
             plugin->setToneHz(tone_hz);
             return plugin;
           }),
           py::arg("drive_db") = 25, py::arg("tone_hz") = 800)
      .def("__repr__",
           [](const Fuzz<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Fuzz";
             ss << " drive_db=" << plugin.getDriveDecibels();
             ss << " tone_hz=" << plugin.getToneHz();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("drive_db", &Fuzz<float>::getDriveDecibels,
                    &Fuzz<float>::setDriveDecibels)
      .def_property("tone_hz", &Fuzz<float>::getToneHz,
                    &Fuzz<float>::setToneHz);
}
}; // namespace Pedalboard
