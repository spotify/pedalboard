/*
 * pedalboard
 * Copyright 2023 Spotify AB
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

#include "../vendors/rubberband/rubberband/RubberBandStretcher.h"
#include "StreamUtils.h"

using namespace RubberBand;

namespace Pedalboard {

static const int MAX_SEMITONES_TO_PITCH_SHIFT = 72;

/*
 * A wrapper around Rubber Band that allows calling it independently of a plugin
 * context, to allow for both pitch shifting and time stretching on fixed-size
 * chunks of audio.
 *
 * The `Plugin` base class requires that one sample of audio output is always
 * provided for every sample input, but this assumption does not hold true
 */
static juce::AudioBuffer<float>
timeStretch(const juce::AudioBuffer<float> input, double sampleRate,
            double stretchFactor, double pitchShiftInSemitones,
            std::string transientMode, std::string transientDetector,
            bool retainPhaseContinuity, std::optional<bool> useLongFFTWindow,
            bool useTimeDomainSmoothing, bool preserveFormants) {
  RubberBandStretcher::Options options =
      RubberBandStretcher::OptionProcessOffline |
      RubberBandStretcher::OptionThreadingNever |
      RubberBandStretcher::OptionChannelsTogether |
      RubberBandStretcher::OptionPitchHighQuality;

  if (transientMode == "crisp") {
    options |= RubberBandStretcher::OptionTransientsCrisp;
  } else if (transientMode == "mixed") {
    options |= RubberBandStretcher::OptionTransientsMixed;
  } else if (transientMode == "smooth") {
    options |= RubberBandStretcher::OptionTransientsSmooth;
  } else {
    throw std::domain_error("time_stretch got an unknown value for "
                            "transient_mode; expected one of \"crisp\", "
                            "\"mixed\", or \"smooth\", but was passed: \"" +
                            transientMode + "\"");
  }

  if (transientDetector == "compound") {
    options |= RubberBandStretcher::OptionDetectorCompound;
  } else if (transientDetector == "percussive") {
    options |= RubberBandStretcher::OptionDetectorPercussive;
  } else if (transientDetector == "soft") {
    options |= RubberBandStretcher::OptionDetectorSoft;
  } else {
    throw std::domain_error("time_stretch got an unknown value for "
                            "transient_mode; expected one of \"compound\", "
                            "\"percussive\", or \"soft\", but was passed: \"" +
                            transientDetector + "\"");
  }

  if (!retainPhaseContinuity) {
    options |= RubberBandStretcher::OptionPhaseIndependent;
  }

  // useLongFFTWindow is an optional type:
  if (useLongFFTWindow) {
    if (*useLongFFTWindow) {
      options |= RubberBandStretcher::OptionWindowLong;
    } else {
      options |= RubberBandStretcher::OptionWindowShort;
    }
  }

  if (useTimeDomainSmoothing) {
    options |= RubberBandStretcher::OptionSmoothingOn;
  }

  if (preserveFormants) {
    options |= RubberBandStretcher::OptionFormantPreserved;
  }

  SuppressOutput suppress_cerr(std::cerr);
  RubberBandStretcher rubberBandStretcher(
      sampleRate, input.getNumChannels(), options, 1.0 / stretchFactor,
      pow(2.0, (pitchShiftInSemitones / 12.0)));
  rubberBandStretcher.setMaxProcessSize(input.getNumSamples());
  rubberBandStretcher.setExpectedInputDuration(input.getNumSamples());

  rubberBandStretcher.study(input.getArrayOfReadPointers(),
                            input.getNumSamples(), true);

  rubberBandStretcher.process(input.getArrayOfReadPointers(),
                              input.getNumSamples(), true);

  juce::AudioBuffer<float> output(input.getNumChannels(),
                                  rubberBandStretcher.available());
  rubberBandStretcher.retrieve(output.getArrayOfWritePointers(),
                               output.getNumSamples());
  return output;
}

inline void init_time_stretch(py::module &m) {
  m.def(
      "time_stretch",
      [](py::array_t<float, py::array::c_style> input, double sampleRate,
         double stretchFactor, double pitchShiftInSemitones,
         std::string transientMode, std::string transientDetector,
         bool retainPhaseContinuity, std::optional<bool> useLongFFTWindow,
         bool useTimeDomainSmoothing, bool preserveFormants) {
        if (stretchFactor == 0)
          throw std::domain_error(
              "stretch_factor must be greater than 0.0x, but was passed " +
              std::to_string(stretchFactor) + "x.");

        if (pitchShiftInSemitones < -MAX_SEMITONES_TO_PITCH_SHIFT ||
            pitchShiftInSemitones > MAX_SEMITONES_TO_PITCH_SHIFT)
          throw std::domain_error(
              "pitch_shift_in_semitones must be between -" +
              std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) + " and +" +
              std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) +
              " semitones, but was passed " +
              std::to_string(pitchShiftInSemitones) + " semitones.");

        juce::AudioBuffer<float> inputBuffer =
            convertPyArrayIntoJuceBuffer(input, detectChannelLayout(input));

        juce::AudioBuffer<float> output;
        {
          py::gil_scoped_release release;
          output = timeStretch(
              inputBuffer, sampleRate, stretchFactor, pitchShiftInSemitones,
              transientMode, transientDetector, retainPhaseContinuity,
              useLongFFTWindow, useTimeDomainSmoothing, preserveFormants);
        }

        return copyJuceBufferIntoPyArray(output, detectChannelLayout(input), 0);
      },
      R"(
Time-stretch (and optionally pitch-shift) a buffer of audio, changing its length.

Using a higher ``stretch_factor`` will shorten the audio - i.e., a ``stretch_factor``
of ``2.0`` will double the *speed* of the audio and halve the *length* of the audio,
without changing the pitch of the audio.

This function allows for changing the pitch of the audio during the time stretching
operation. The ``stretch_factor`` and ``pitch_shift_in_semitones`` arguments are
independent and do not affect each other (i.e.: you can change one, the other, or both
without worrying about how they interact).

The additional arguments provided to this function allow for more fine-grained control
over the behavior of the time stretcher:

  - ``transient_mode`` controls the behavior of the stretcher around transients
    (percussive parts of the audio). Valid options are ``"crisp"`` (the default),
    ``"mixed"``, or ``"smooth"``.
 
  - ``transient_detector`` controls which method is used to detect transients in the
    audio signal. Valid options are ``"compound"`` (the default), ``"percussive"``,
    or ``"soft"``.
 
  - ``retain_phase_continuity`` ensures that the phases of adjacent frequency bins in
    the audio stream are kept as similar as possible. Set this to ``False`` for a
    softer, phasier sound.
 
  - ``use_long_fft_window`` controls the size of the fast-Fourier transform window
    used during stretching. The default (``None``) will result in a window size that
    varies based on other parameters and should produce better results in most
    situations. Set this option to ``True`` to result in a smoother sound (at the
    expense of clarity and timing), or ``False`` to result in a crisper sound.
 
  - ``use_time_domain_smoothing`` can be enabled to produce a softer sound with
    audible artifacts around sharp transients. This option mixes well with
    ``use_long_fft_window=False``.
   
  - ``preserve_formants`` allows shifting the pitch of notes without substantially
    affecting the pitch profile (formants) of a voice or instrument.

.. warning::
    This is a function, not a :py:class:`Plugin` instance, and cannot be
    used in :py:class:`Pedalboard` objects, as it changes the duration of
    the audio stream.
)",
      py::arg("input_audio"), py::arg("samplerate"),
      py::arg("stretch_factor") = 1.0,
      py::arg("pitch_shift_in_semitones") = 0.0,
      py::arg("transient_mode") = "crisp",
      py::arg("transient_detector") = "compound",
      py::arg("retain_phase_continuity") = true,
      py::arg("use_long_fft_window") = py::none(),
      py::arg("use_time_domain_smoothing") = false,
      py::arg("preserve_formants") = true);
}
}; // namespace Pedalboard
