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
 * This is a sample-peak implementation: it uses a lookahead delay line and
 * hold mechanism to catch peaks before they occur, but does not detect
 * inter-sample (true) peaks. True-peak detection (e.g. via oversampling)
 * is planned for a follow-up commit.
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

  float getLookaheadMs() const noexcept { return lookaheadMs_; }
  void setLookaheadMs(float value) {
    if (value <= 0.0f || value > 100.0f || std::isnan(value) ||
        std::isinf(value))
      throw std::range_error("lookahead_ms must be in (0, 100]");
    lookaheadMs_ = value;
    // Takes effect on next prepare() — no reallocation here
  }

  // ── Plugin interface ──

  virtual void prepare(const juce::dsp::ProcessSpec &spec) override {
    // Match upstream JucePlugin.h pattern: re-prepare only when sampleRate
    // changes, maximumBlockSize GROWS, numChannels changes, or the
    // lookahead parameter changes (which requires re-sizing the delay line).
    bool specChanged =
        (spec.sampleRate != preparedSpec_.sampleRate ||
         preparedSpec_.maximumBlockSize < spec.maximumBlockSize ||
         spec.numChannels != preparedSpec_.numChannels);
    bool paramsChanged = (lookaheadMs_ != preparedLookaheadMs_);

    if (!specChanged && !paramsChanged) {
      return; // preserve state for reset=False streaming
    }

    preparedSpec_ = spec;
    preparedLookaheadMs_ = lookaheadMs_;

    activeLookaheadSamples_ =
        std::max(1, (int)(spec.sampleRate * preparedLookaheadMs_ / 1000.0f));

    delayLine_.setMaximumDelayInSamples(activeLookaheadSamples_ + 1);
    delayLine_.prepare(spec);
    delayLine_.setDelay((SampleType)activeLookaheadSamples_);

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
    int lookaheadSamples = activeLookaheadSamples_;

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

      // 4-branch gain envelope with hold + hold-refresh
      if (targetGain < currentGain_) {
        currentGain_ = targetGain;       // instant attack (deeper reduction)
        holdCounter_ = lookaheadSamples; // reset hold timer
      } else if (targetGain < 1.0f) {
        holdCounter_ = lookaheadSamples; // above-ceiling: refresh hold
      } else if (holdCounter_ > 0) {
        holdCounter_--; // hold at current gain
      } else {
        currentGain_ +=
            (1.0f - releaseCoeff) * (1.0f - currentGain_); // release
      }

      // Push input into delay, pop delayed sample, apply gain + safety clip
      for (int ch = 0; ch < numChannels; ++ch) {
        SampleType *channelPtr = ioBlock.getChannelPointer(ch);
        delayLine_.pushSample(ch, channelPtr[i]);
        SampleType delayed = delayLine_.popSample(ch);
        channelPtr[i] = juce::jlimit(
            (SampleType)(-ceilingLinear), (SampleType)ceilingLinear,
            (SampleType)(delayed * currentGain_));
      }
    }

    samplesProvided_ += numSamples;
    return std::min(numSamples,
                     std::max(0, samplesProvided_ - activeLookaheadSamples_));
  }

  virtual void reset() override {
    delayLine_.reset();
    currentGain_ = 1.0f;
    holdCounter_ = 0;
    samplesProvided_ = 0;
  }

  virtual int getLatencyHint() override { return activeLookaheadSamples_; }

private:
  // Requested parameters (written by setters, read by process() at block start)
  float ceilingDb_ = -1.0f;
  float releaseMs_ = 100.0f;
  float lookaheadMs_ = 5.0f;

  // Active (prepared) state — only written by prepare(), read by process()
  int activeLookaheadSamples_ = 0;

  // DSP state
  float currentGain_ = 1.0f;
  int holdCounter_ = 0;
  int samplesProvided_ = 0;
  float preparedLookaheadMs_ = 0.0f;

  juce::dsp::DelayLine<SampleType, juce::dsp::DelayLineInterpolationTypes::None>
      delayLine_{441000};

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
      "- Uses a lookahead delay line and hold mechanism to catch peaks "
      "before they occur\n\n"
      "This is currently a sample-peak-only implementation: it does not "
      "detect inter-sample (true) peaks. True-peak detection (e.g. via "
      "oversampling) is planned for a follow-up release.\n\n"
      "Typical use: enforce a peak ceiling after loudness normalization.\n\n"
      ".. code-block:: python\n\n"
      "    limiter = BrickwallLimiter(ceiling_db=-1.0)\n"
      "    output = limiter(audio, sample_rate)\n")
      .def(py::init([](float ceiling, float release, float lookahead) {
             auto p = std::make_unique<BrickwallLimiter<float>>();
             p->setCeilingDb(ceiling);
             p->setReleaseMs(release);
             p->setLookaheadMs(lookahead);
             return p;
           }),
           py::arg("ceiling_db") = -1.0f, py::arg("release_ms") = 100.0f,
           py::arg("lookahead_ms") = 5.0f)
      .def("__repr__",
           [](const BrickwallLimiter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.BrickwallLimiter"
                << " ceiling_db=" << plugin.getCeilingDb()
                << " release_ms=" << plugin.getReleaseMs()
                << " lookahead_ms=" << plugin.getLookaheadMs() << " at "
                << &plugin << ">";
             return ss.str();
           })
      .def_property("ceiling_db", &BrickwallLimiter<float>::getCeilingDb,
                    &BrickwallLimiter<float>::setCeilingDb)
      .def_property("release_ms", &BrickwallLimiter<float>::getReleaseMs,
                    &BrickwallLimiter<float>::setReleaseMs)
      .def_property("lookahead_ms", &BrickwallLimiter<float>::getLookaheadMs,
                    &BrickwallLimiter<float>::setLookaheadMs);
}

} // namespace Pedalboard
