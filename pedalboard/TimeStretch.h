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
            double stretchFactor, double pitchShiftInSemitones) {
  RubberBandStretcher rubberBandStretcher(
      sampleRate, input.getNumChannels(),
      RubberBandStretcher::OptionProcessOffline |
          RubberBandStretcher::OptionThreadingNever |
          RubberBandStretcher::OptionChannelsTogether |
          RubberBandStretcher::OptionPitchHighQuality,
      1.0 / stretchFactor, pow(2.0, (pitchShiftInSemitones / 12.0)));
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
         double stretchFactor, double pitchShiftInSemitones) {
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
          output = timeStretch(inputBuffer, sampleRate, stretchFactor,
                               pitchShiftInSemitones);
        }

        return copyJuceBufferIntoPyArray(output, detectChannelLayout(input), 0);
      },
      "Time-stretch (and optionally pitch-shift) a buffer of audio, changing "
      "its length. Note that this is a function, not a :py:class:`Plugin` "
      "instance, and cannot be used in :py:class:`Pedalboard` objects, as it "
      "changes the duration of the audio stream.",
      py::arg("input_audio"), py::arg("samplerate"),
      py::arg("stretch_factor") = 1.0,
      py::arg("pitch_shift_in_semitones") = 0.0);
}
}; // namespace Pedalboard
