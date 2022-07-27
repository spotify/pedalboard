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

namespace Pedalboard {

/**
 * Given a cutoff frequency and sample rate, clamp the cutoff frequency to
 * [e, sr / 2 - e] for some small epsilon, to ensure that filter frequency
 * response is stable.
 */
inline float clampCutoffFrequency(float cutoffFrequencyHz, float sampleRate) {
  return juce::jlimit(1e-2f, (sampleRate / 2) - 1e2f, cutoffFrequencyHz);
}

/**
 * A base class for all IIR filter classes.
 */
template <typename SampleType>
class IIRFilter : public JucePlugin<juce::dsp::ProcessorDuplicator<
                      juce::dsp::IIR::Filter<SampleType>,
                      juce::dsp::IIR::Coefficients<SampleType>>> {
public:
  void setCutoffFrequencyHz(float f) {
    if (f <= 0)
      throw std::domain_error("Cutoff frequency must be greater than 0Hz.");
    cutoffFrequencyHz = f;
  }
  float getCutoffFrequencyHz() const noexcept { return cutoffFrequencyHz; }

  void setQ(float q) {
    if (q <= 0)
      throw std::domain_error("Q value must be greater than 0.");
    Q = q;
  }
  float getQ() const noexcept { return Q; }

  void setGainDecibels(float dB) {
    gainFactor = juce::Decibels::decibelsToGain<SampleType>(dB);
  }
  float getGainDecibels() const noexcept {
    return juce::Decibels::gainToDecibels<SampleType>(gainFactor);
  }

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    if (this->lastSpec.sampleRate != spec.sampleRate ||
        this->lastSpec.maximumBlockSize < spec.maximumBlockSize ||
        spec.numChannels != this->lastSpec.numChannels) {
      JucePlugin<juce::dsp::ProcessorDuplicator<
          juce::dsp::IIR::Filter<SampleType>,
          juce::dsp::IIR::Coefficients<SampleType>>>::prepare(spec);
      this->lastSpec = spec;
    }
  }

protected:
  float cutoffFrequencyHz;
  float Q;
  float gainFactor;
};

template <typename SampleType>
class HighShelfFilter : public IIRFilter<SampleType> {
public:
  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    *this->getDSP().state =
        *juce::dsp::IIR::Coefficients<SampleType>::makeHighShelf(
            spec.sampleRate,
            clampCutoffFrequency(this->cutoffFrequencyHz, spec.sampleRate),
            this->Q, this->gainFactor);

    IIRFilter<SampleType>::prepare(spec);
  }
};

template <typename SampleType>
class LowShelfFilter : public IIRFilter<SampleType> {
public:
  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    *this->getDSP().state =
        *juce::dsp::IIR::Coefficients<SampleType>::makeLowShelf(
            spec.sampleRate,
            clampCutoffFrequency(this->cutoffFrequencyHz, spec.sampleRate),
            this->Q, this->gainFactor);

    IIRFilter<SampleType>::prepare(spec);
  }
};

template <typename SampleType> class PeakFilter : public IIRFilter<SampleType> {
public:
  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    *this->getDSP().state =
        *juce::dsp::IIR::Coefficients<SampleType>::makePeakFilter(
            spec.sampleRate,
            clampCutoffFrequency(this->cutoffFrequencyHz, spec.sampleRate),
            this->Q, this->gainFactor);
    IIRFilter<SampleType>::prepare(spec);
  }
};

inline void init_iir_filters(py::module &m) {
  py::class_<IIRFilter<float>, Plugin, std::shared_ptr<IIRFilter<float>>>(
      m, "IIRFilter",
      "An abstract class that implements various kinds of infinite impulse "
      "response (IIR) filter designs. This should not be used directly; use "
      ":class:`HighShelfFilter`, :class:`LowShelfFilter`, or "
      ":class:`PeakFilter` directly instead.")
      .def(py::init([]() {
        throw std::runtime_error(
            "IIRFilter is not designed to be instantiated directly: "
            "use HighShelfFilter, LowShelfFilter, or PeakFilter instead.");
        return nullptr;
      }));

  py::class_<HighShelfFilter<float>, IIRFilter<float>,
             std::shared_ptr<HighShelfFilter<float>>>(
      m, "HighShelfFilter",
      "A high shelf filter plugin with variable Q and gain, as would be used "
      "in an equalizer. Frequencies above the cutoff frequency will be boosted "
      "(or cut) by the provided gain (in decibels).")
      .def(py::init([](float cutoffFrequencyHz, float gaindB, float Q) {
             auto plugin = std::make_unique<HighShelfFilter<float>>();
             plugin->setCutoffFrequencyHz(cutoffFrequencyHz);
             plugin->setGainDecibels(gaindB);
             plugin->setQ(Q);
             return plugin;
           }),
           py::arg("cutoff_frequency_hz") = 440, py::arg("gain_db") = 0.0,
           py::arg("q") = (juce::MathConstants<float>::sqrt2 / 2.0))
      .def("__repr__",
           [](const HighShelfFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.HighShelfFilter";
             ss << " cutoff_frequency_hz=" << plugin.getCutoffFrequencyHz();
             ss << " gain_db=" << plugin.getGainDecibels();
             ss << " q=" << plugin.getQ();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("cutoff_frequency_hz",
                    &HighShelfFilter<float>::getCutoffFrequencyHz,
                    &HighShelfFilter<float>::setCutoffFrequencyHz)
      .def_property("gain_db", &HighShelfFilter<float>::getGainDecibels,
                    &HighShelfFilter<float>::setGainDecibels)
      .def_property("q", &HighShelfFilter<float>::getQ,
                    &HighShelfFilter<float>::setQ);

  py::class_<LowShelfFilter<float>, IIRFilter<float>,
             std::shared_ptr<LowShelfFilter<float>>>(
      m, "LowShelfFilter",
      "A low shelf filter with variable Q and gain, as would be used in an "
      "equalizer. Frequencies below the cutoff frequency will be boosted (or "
      "cut) by the provided gain value.")
      .def(py::init([](float cutoffFrequencyHz, float gaindB, float Q) {
             auto plugin = std::make_unique<LowShelfFilter<float>>();
             plugin->setCutoffFrequencyHz(cutoffFrequencyHz);
             plugin->setGainDecibels(gaindB);
             plugin->setQ(Q);
             return plugin;
           }),
           py::arg("cutoff_frequency_hz") = 440, py::arg("gain_db") = 0.0,
           py::arg("q") = (juce::MathConstants<float>::sqrt2 / 2.0))
      .def("__repr__",
           [](const LowShelfFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.LowShelfFilter";
             ss << " cutoff_frequency_hz=" << plugin.getCutoffFrequencyHz();
             ss << " gain_db=" << plugin.getGainDecibels();
             ss << " q=" << plugin.getQ();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("cutoff_frequency_hz",
                    &LowShelfFilter<float>::getCutoffFrequencyHz,
                    &LowShelfFilter<float>::setCutoffFrequencyHz)
      .def_property("gain_db", &LowShelfFilter<float>::getGainDecibels,
                    &LowShelfFilter<float>::setGainDecibels)
      .def_property("q", &LowShelfFilter<float>::getQ,
                    &LowShelfFilter<float>::setQ);

  py::class_<PeakFilter<float>, IIRFilter<float>,
             std::shared_ptr<PeakFilter<float>>>(
      m, "PeakFilter",
      "A peak (or notch) filter with variable Q and gain, as would be used in "
      "an equalizer. Frequencies around the cutoff frequency will be boosted "
      "(or cut) by the provided gain value.")
      .def(py::init([](float cutoffFrequencyHz, float gaindB, float Q) {
             auto plugin = std::make_unique<PeakFilter<float>>();
             plugin->setCutoffFrequencyHz(cutoffFrequencyHz);
             plugin->setGainDecibels(gaindB);
             plugin->setQ(Q);
             return plugin;
           }),
           py::arg("cutoff_frequency_hz") = 440, py::arg("gain_db") = 0.0,
           py::arg("q") = (juce::MathConstants<float>::sqrt2 / 2.0))
      .def("__repr__",
           [](const PeakFilter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.PeakFilter";
             ss << " cutoff_frequency_hz=" << plugin.getCutoffFrequencyHz();
             ss << " gain_db=" << plugin.getGainDecibels();
             ss << " q=" << plugin.getQ();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("cutoff_frequency_hz",
                    &PeakFilter<float>::getCutoffFrequencyHz,
                    &PeakFilter<float>::setCutoffFrequencyHz)
      .def_property("gain_db", &PeakFilter<float>::getGainDecibels,
                    &PeakFilter<float>::setGainDecibels)
      .def_property("q", &PeakFilter<float>::getQ, &PeakFilter<float>::setQ);
}
}; // namespace Pedalboard
