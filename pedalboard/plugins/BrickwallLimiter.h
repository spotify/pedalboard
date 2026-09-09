/*
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

#pragma once

#include "../JuceHeader.h"
#include "../Plugin.h"

#include <atomic>
#include <cmath>

namespace Pedalboard {

/**
 * A lookahead brick-wall limiter with configurable ceiling.
 *
 * Unlike the built-in Limiter (which wraps JUCE's dsp::Limiter and applies
 * makeup gain), this plugin:
 *   - Has a configurable output ceiling (not fixed at 0 dBFS)
 *   - Does NOT apply makeup gain
 *   - Uses a smoothed gain envelope (instant attack, exponential release)
 *   - Uses a lookahead delay line and hold mechanism to catch peaks
 *     before they occur
 *   - Optionally detects inter-sample (true) peaks via 4x oversampled
 *     sidechain when true_peak=True
 *
 * Design: setters store REQUESTED parameter values. prepare() copies them
 * to ACTIVE state. process() reads only ACTIVE state.
 * All four requested-state parameters (ceiling_db, release_ms, lookahead_ms,
 * true_peak) use std::atomic for safe concurrent access between Python
 * setters and the native audio callback (AudioStream releases the GIL
 * during processing, so plain field access would be a C++ data race).
 * Coefficients are snapshotted at block boundaries for thread safety.
 */
template <typename SampleType> class BrickwallLimiter : public Plugin {
public:
  // ── Parameter getters/setters ──

  float getCeilingDb() const noexcept {
    return ceilingDb_.load(std::memory_order_relaxed);
  }
  void setCeilingDb(float value) {
    if (std::isnan(value) || std::isinf(value))
      throw std::range_error("ceiling_db must be a finite number");
    ceilingDb_.store(value, std::memory_order_relaxed);
  }

  float getReleaseMs() const noexcept {
    return releaseMs_.load(std::memory_order_relaxed);
  }
  void setReleaseMs(float value) {
    if (value <= 0.0f || std::isnan(value) || std::isinf(value))
      throw std::range_error("release_ms must be > 0");
    releaseMs_.store(value, std::memory_order_relaxed);
  }

  float getLookaheadMs() const noexcept {
    return lookaheadMs_.load(std::memory_order_relaxed);
  }
  void setLookaheadMs(float value) {
    if (value <= 0.0f || value > 100.0f || std::isnan(value) ||
        std::isinf(value))
      throw std::range_error("lookahead_ms must be in (0, 100]");
    lookaheadMs_.store(value, std::memory_order_relaxed);
    // Takes effect on next prepare() — no reallocation here
  }

  bool getTruePeak() const noexcept {
    return truePeak_.load(std::memory_order_relaxed);
  }
  void setTruePeak(bool value) {
    truePeak_.store(value, std::memory_order_relaxed);
    // Takes effect on next prepare()
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
    float currentLookaheadMs = lookaheadMs_.load(std::memory_order_relaxed);
    bool currentTruePeak = truePeak_.load(std::memory_order_relaxed);
    bool paramsChanged = (currentLookaheadMs != preparedLookaheadMs_ ||
                          currentTruePeak != preparedTruePeak_);

    if (!specChanged && !paramsChanged) {
      return; // preserve state for reset=False streaming
    }

    preparedSpec_ = spec;
    preparedLookaheadMs_ = currentLookaheadMs;
    preparedTruePeak_ = currentTruePeak;

    // Oversampler
    upsampleDelay_ = 0;
    if (preparedTruePeak_) {
      oversampler_ = std::make_unique<juce::dsp::Oversampling<SampleType>>(
          spec.numChannels, 2, // order=2 → 4×
          juce::dsp::Oversampling<SampleType>::filterHalfBandFIREquiripple,
          true); // isMaxQuality
      oversampler_->initProcessing(spec.maximumBlockSize);
      // Conservative upsampler delay compensation: use the FULL reported
      // round-trip latency as the delay line extension.
      //
      // getLatencyInSamples() reports the combined up+down path latency.
      // The actual upsampling-only delay is somewhere between half and the
      // full value (JUCE's equiripple up- and down-filters have different
      // orders, and no per-direction accessor is exposed). Using the full
      // value over-compensates slightly — the delay line is a bit longer
      // than strictly needed — but this guarantees the gain reduction
      // always arrives before the peak exits the delay line, even with
      // very short lookahead_ms values. The extra latency is typically
      // < 10 samples, negligible for most use cases.
      upsampleDelay_ = (int)std::ceil(oversampler_->getLatencyInSamples());
    } else {
      oversampler_.reset();
    }

    int baseLookahead =
        std::max(1, (int)(spec.sampleRate * preparedLookaheadMs_ / 1000.0f));
    activeLookaheadSamples_ = baseLookahead + upsampleDelay_;

    delayLine_.setMaximumDelayInSamples(activeLookaheadSamples_ + 1);
    delayLine_.prepare(spec);
    delayLine_.setDelay((SampleType)activeLookaheadSamples_);

    scratchBuffer_.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    peakBuffer_.resize(spec.maximumBlockSize, 0.0f);

    reset();
  }

  virtual int process(
      const juce::dsp::ProcessContextReplacing<SampleType> &context) override {
    auto ioBlock = context.getOutputBlock();
    int numSamples = (int)ioBlock.getNumSamples();
    int numChannels = (int)ioBlock.getNumChannels();

    // Snapshot atomic parameters at block boundary.
    float ceilingLinear =
        std::pow(10.0f, ceilingDb_.load(std::memory_order_relaxed) / 20.0f);
    float releaseCoeff = std::exp(
        -1.0f / std::max(1, (int)(preparedSpec_.sampleRate *
                                  releaseMs_.load(std::memory_order_relaxed) /
                                  1000.0f)));
    int lookaheadSamples = activeLookaheadSamples_;

    // ── Step 1: Detect peaks ──
    if (preparedTruePeak_ && oversampler_) {
      // Copy ONLY active N samples to scratch (avoid stale tail data)
      for (int ch = 0; ch < numChannels; ++ch) {
        scratchBuffer_.copyFrom(ch, 0, ioBlock.getChannelPointer(ch),
                                numSamples);
      }
      juce::dsp::AudioBlock<SampleType> scratchBlock(scratchBuffer_);
      auto activeBlock = scratchBlock.getSubBlock(0, (size_t)numSamples);

      auto oversampledBlock = oversampler_->processSamplesUp(activeBlock);
      int oversampledLen = (int)oversampledBlock.getNumSamples();

      for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
          auto *ptr = oversampledBlock.getChannelPointer(ch);
          for (int k = 0; k < 4; ++k) {
            int idx = i * 4 + k;
            if (idx < oversampledLen) {
              float s = std::abs(ptr[idx]);
              if (s > peak)
                peak = s;
            }
          }
        }
        peakBuffer_[(size_t)i] = peak;
      }
      // No processSamplesDown() — upsampling filter state is independent
    } else {
      for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
          float s = std::abs(ioBlock.getChannelPointer(ch)[i]);
          if (s > peak)
            peak = s;
        }
        peakBuffer_[(size_t)i] = peak;
      }
    }

    // ── Step 2: Apply gain with lookahead + hold ──
    for (int i = 0; i < numSamples; ++i) {
      float peak = peakBuffer_[(size_t)i];

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
        channelPtr[i] = juce::jlimit((SampleType)(-ceilingLinear),
                                     (SampleType)ceilingLinear,
                                     (SampleType)(delayed * currentGain_));
      }
    }

    samplesProvided_ += numSamples;
    return std::min(numSamples,
                    std::max(0, samplesProvided_ - activeLookaheadSamples_));
  }

  virtual void reset() override {
    delayLine_.reset();
    if (oversampler_)
      oversampler_->reset();
    currentGain_ = 1.0f;
    holdCounter_ = 0;
    samplesProvided_ = 0;
  }

  virtual int getLatencyHint() override { return activeLookaheadSamples_; }

private:
  // Requested parameters — ALL atomic for thread safety with AudioStream.
  // Written by setters (Python thread), read by process()/prepare() (which
  // may run on a different thread). Relaxed ordering is sufficient since we
  // snapshot at block boundaries (process()) or preparation boundaries
  // (prepare()), needing only eventual visibility, not ordering guarantees.
  std::atomic<float> ceilingDb_{-1.0f};
  std::atomic<float> releaseMs_{100.0f};
  std::atomic<float> lookaheadMs_{5.0f};
  std::atomic<bool> truePeak_{false};

  // Active (prepared) state — only written by prepare(), read by process()
  int activeLookaheadSamples_ = 0;
  bool preparedTruePeak_ = false;
  int upsampleDelay_ = 0;

  // DSP state
  float currentGain_ = 1.0f;
  int holdCounter_ = 0;
  int samplesProvided_ = 0;
  float preparedLookaheadMs_ = 0.0f;

  juce::dsp::DelayLine<SampleType, juce::dsp::DelayLineInterpolationTypes::None>
      delayLine_{441000};

  std::unique_ptr<juce::dsp::Oversampling<SampleType>> oversampler_;
  juce::AudioBuffer<SampleType> scratchBuffer_;
  std::vector<float> peakBuffer_;

  // Cached spec
  juce::dsp::ProcessSpec preparedSpec_{};
};

inline void init_brickwall_limiter(py::module &m) {
  py::class_<BrickwallLimiter<float>, Plugin,
             std::shared_ptr<BrickwallLimiter<float>>>(
      m, "BrickwallLimiter",
      "A lookahead brick-wall limiter with configurable ceiling.\n\n"
      "Unlike the built-in :class:`Limiter`, ``BrickwallLimiter``:\n\n"
      "- Has a configurable ceiling (not fixed at 0 dBFS)\n"
      "- Does **not** apply makeup gain\n"
      "- Uses a smoothed gain envelope (instant attack, exponential "
      "release)\n"
      "- Uses a lookahead delay line and hold mechanism to catch peaks "
      "before they occur\n\n"
      "When ``true_peak=True``, the sidechain uses 4x oversampling to detect\n"
      "inter-sample peaks.  This substantially reduces inter-sample overshoot\n"
      "in the output but does not guarantee the reconstructed waveform stays\n"
      "below the ceiling — use a safety margin (e.g. ``ceiling_db=-1.5``) for\n"
      "strict ITU-R BS.1770 compliance.\n\n"
      ".. note::\n\n"
      "   Changing ``lookahead_ms`` or ``true_peak`` requires a stream "
      "restart\n"
      "   to take effect.  ``ceiling_db`` and ``release_ms`` can be changed "
      "at\n"
      "   any time.\n\n"
      "Typical use: enforce a peak ceiling after loudness normalization.\n\n"
      ".. code-block:: python\n\n"
      "    limiter = BrickwallLimiter(ceiling_db=-1.0)\n"
      "    output = limiter(audio, sample_rate)\n")
      .def(py::init([](float ceiling, float release, float lookahead, bool tp) {
             auto p = std::make_unique<BrickwallLimiter<float>>();
             p->setCeilingDb(ceiling);
             p->setReleaseMs(release);
             p->setLookaheadMs(lookahead);
             p->setTruePeak(tp);
             return p;
           }),
           py::arg("ceiling_db") = -1.0f, py::arg("release_ms") = 100.0f,
           py::arg("lookahead_ms") = 5.0f, py::arg("true_peak") = false)
      .def("__repr__",
           [](const BrickwallLimiter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.BrickwallLimiter"
                << " ceiling_db=" << plugin.getCeilingDb()
                << " release_ms=" << plugin.getReleaseMs()
                << " lookahead_ms=" << plugin.getLookaheadMs()
                << " true_peak=" << (plugin.getTruePeak() ? "True" : "False")
                << " at " << &plugin << ">";
             return ss.str();
           })
      .def_property("ceiling_db", &BrickwallLimiter<float>::getCeilingDb,
                    &BrickwallLimiter<float>::setCeilingDb)
      .def_property("release_ms", &BrickwallLimiter<float>::getReleaseMs,
                    &BrickwallLimiter<float>::setReleaseMs)
      .def_property("lookahead_ms", &BrickwallLimiter<float>::getLookaheadMs,
                    &BrickwallLimiter<float>::setLookaheadMs)
      .def_property("true_peak", &BrickwallLimiter<float>::getTruePeak,
                    &BrickwallLimiter<float>::setTruePeak);
}

} // namespace Pedalboard
