/*
 * Copyright 2021-2024 Spotify AB
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

#pragma once

#include "../JuceHeader.h"
#include "../Plugin.h"

#include <cmath>

namespace Pedalboard {

/**
 * A sample-peak brick-wall limiter with configurable ceiling.
 *
 * Unlike the built-in Limiter (which wraps JUCE's dsp::Limiter and applies
 * makeup gain), this plugin:
 *   - Has a configurable output ceiling (not fixed at 0 dBFS)
 *   - Does NOT apply makeup gain
 *   - Uses a smoothed gain envelope (instant attack, exponential release)
 *
 * This is a sample-peak-only implementation: it has no lookahead and does
 * not detect inter-sample (true) peaks. Lookahead and true-peak detection
 * are planned for follow-up commits.
 *
 * Design: setters store REQUESTED parameter values. prepare() copies them
 * to ACTIVE state. process() reads only ACTIVE state.
 * Coefficients are snapshotted at block boundaries for thread safety.
 */
template <typename SampleType> class BrickwallLimiter : public Plugin {
public:
  // ── Parameter getters/setters ──
  // Setters store requested values. Derived coefficients are snapshotted
  // in process() at block boundaries for thread safety with AudioStream.

  float getCeilingDb() const noexcept { return ceilingDb_; }
  void setCeilingDb(float value) {
    if (std::isnan(value) || std::isinf(value))
      throw std::range_error("ceiling_db must be a finite number");
    ceilingDb_ = value;
  }

  float getReleaseMs() const noexcept { return releaseMs_; }
  void setReleaseMs(float value) {
    if (value <= 0.0f || std::isnan(value) || std::isinf(value))
      throw std::range_error("release_ms must be > 0");
    releaseMs_ = value;
  }

  // ── Plugin interface ──

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    // Match upstream JucePlugin.h pattern: re-prepare only when sampleRate
    // changes, maximumBlockSize GROWS, or numChannels changes.
    if (spec.sampleRate == preparedSpec_.sampleRate &&
        spec.maximumBlockSize <= preparedSpec_.maximumBlockSize &&
        spec.numChannels == preparedSpec_.numChannels) {
      return; // preserve state for reset=False streaming
    }

    preparedSpec_ = spec;
    reset();
  }

  virtual int process(
      const juce::dsp::ProcessContextReplacing<SampleType> &context) override {
    auto ioBlock = context.getOutputBlock();
    int numSamples = (int)ioBlock.getNumSamples();
    int numChannels = (int)ioBlock.getNumChannels();

    // Snapshot parameters at block boundary for thread safety
    float ceilingLinear = std::pow(10.0f, ceilingDb_ / 20.0f);
    float releaseCoeff = std::exp(
        -1.0f /
        std::max(1, (int)(preparedSpec_.sampleRate * releaseMs_ / 1000.0f)));

    for (int i = 0; i < numSamples; ++i) {
      // Detect cross-channel peak for this sample
      float peak = 0.0f;
      for (int ch = 0; ch < numChannels; ++ch) {
        float s = std::abs(ioBlock.getChannelPointer(ch)[i]);
        if (s > peak)
          peak = s;
      }

      // Compute target gain
      float targetGain = (peak > ceilingLinear)
                             ? (ceilingLinear / std::max(peak, 1e-30f))
                             : 1.0f;

      // Instant attack, exponential release
      if (targetGain < currentGain_) {
        currentGain_ = targetGain;
      } else {
        currentGain_ += (1.0f - releaseCoeff) * (1.0f - currentGain_);
      }

      // Apply gain + safety clip
      for (int ch = 0; ch < numChannels; ++ch) {
        SampleType *channelPtr = ioBlock.getChannelPointer(ch);
        channelPtr[i] = juce::jlimit(
            (SampleType)(-ceilingLinear), (SampleType)ceilingLinear,
            (SampleType)(channelPtr[i] * currentGain_));
      }
    }

    return numSamples;
  }

  virtual void reset() override { currentGain_ = 1.0f; }

private:
  // Requested parameters (written by setters, read by process() at block start)
  float ceilingDb_ = -1.0f;
  float releaseMs_ = 100.0f;

  // DSP state
  float currentGain_ = 1.0f;

  // Cached spec
  juce::dsp::ProcessSpec preparedSpec_{};
};

inline void init_brickwall_limiter(py::module &m) {
  py::class_<BrickwallLimiter<float>, Plugin,
             std::shared_ptr<BrickwallLimiter<float>>>(
      m, "BrickwallLimiter",
      "A sample-peak brick-wall limiter with configurable ceiling.\n\n"
      "Unlike the built-in :class:`Limiter`, ``BrickwallLimiter``:\n\n"
      "- Has a configurable ceiling (not fixed at 0 dBFS)\n"
      "- Does **not** apply makeup gain\n"
      "- Uses a smoothed gain envelope (instant attack, exponential "
      "release)\n\n"
      "This is currently a sample-peak-only implementation: it has no "
      "lookahead and does not detect inter-sample (true) peaks. Lookahead "
      "and true-peak detection (e.g. via oversampling) are planned for "
      "follow-up releases.\n\n"
      "Typical use: enforce a peak ceiling after loudness normalization.\n\n"
      ".. code-block:: python\n\n"
      "    limiter = BrickwallLimiter(ceiling_db=-1.0)\n"
      "    output = limiter(audio, sample_rate)\n")
      .def(py::init([](float ceiling, float release) {
             auto p = std::make_unique<BrickwallLimiter<float>>();
             p->setCeilingDb(ceiling);
             p->setReleaseMs(release);
             return p;
           }),
           py::arg("ceiling_db") = -1.0f, py::arg("release_ms") = 100.0f)
      .def("__repr__",
           [](const BrickwallLimiter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.BrickwallLimiter"
                << " ceiling_db=" << plugin.getCeilingDb()
                << " release_ms=" << plugin.getReleaseMs() << " at " << &plugin
                << ">";
             return ss.str();
           })
      .def_property("ceiling_db", &BrickwallLimiter<float>::getCeilingDb,
                    &BrickwallLimiter<float>::setCeilingDb)
      .def_property("release_ms", &BrickwallLimiter<float>::getReleaseMs,
                    &BrickwallLimiter<float>::setReleaseMs);
}

} // namespace Pedalboard
