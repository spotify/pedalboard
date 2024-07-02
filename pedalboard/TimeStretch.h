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
static const size_t STUDY_BLOCK_SAMPLE_SIZE = 2048;
// This is the minimum number of samples to process in a single
// block for efficiency. It's assumed that changing pitch or stretch factor more
// frequently than this number of samples is not useful.
// TODO: is it though?
static const size_t MINIMUM_BLOCK_SIZE = 1024;

/**
 * @brief Given a vector of doubles representing the value of a parameter over
 * time and a current chunk size, return the size of the next chunk to process.
 * Discontinuities in the parameter are used to determine how big each chunk can
 * be; if a parameter changes in a stair-step fashion, each chunk can be as wide
 * as each stair step to maximize efficiency. If a parameter changes
 * continuously, the chunk size is bounded by MINIMUM_BLOCK_SIZE and
 * maximumBlockSize.
 *
 * @param chunkSize
 * @param currentOffset
 * @param variableParameter
 * @param maximumBlockSize
 * @return size_t
 */
size_t chooseChunkSize(size_t chunkSize, size_t currentOffset,
                       std::vector<double> &variableParameter,
                       size_t maximumBlockSize) {
  size_t size = variableParameter.size();
  size_t distanceToNextChange = size - currentOffset;
  if (currentOffset >= size) {
    throw std::domain_error("chooseChunkSize called with currentOffset >= "
                            "variableParameter.size(). This is an internal "
                            "Pedalboard logic error and should be reported.");
  }

  double *values = variableParameter.data();
  double startValue = values[currentOffset];
  for (size_t i = currentOffset; i < size; i++) {
    if (values[i] != startValue) {
      distanceToNextChange = i - currentOffset;
      break;
    }
  }

  if (distanceToNextChange < chunkSize) {
    // Only make the chunk size smaller (as other chained calls may have altered
    // it already).
    return std::max(MINIMUM_BLOCK_SIZE,
                    std::min(maximumBlockSize, distanceToNextChange));
  } else {
    return chunkSize;
  }
}

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
            std::variant<double, std::vector<double>> stretchFactor,
            std::variant<double, std::vector<double>> pitchShiftInSemitones,
            bool highQuality, std::string transientMode,
            std::string transientDetector, bool retainPhaseContinuity,
            std::optional<bool> useLongFFTWindow, bool useTimeDomainSmoothing,
            bool preserveFormants) {
  RubberBandStretcher::Options options =
      RubberBandStretcher::OptionProcessOffline |
      RubberBandStretcher::OptionThreadingNever |
      RubberBandStretcher::OptionChannelsTogether |
      RubberBandStretcher::OptionPitchHighQuality;

  if (highQuality) {
    options |= RubberBandStretcher::OptionEngineFiner;
  } else {
    options |= RubberBandStretcher::OptionEngineFaster;
  }

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
  double initialStretchFactor = 1;
  double initialPitchShiftInSemitones = 0;
  size_t expectedNumberOfOutputSamples = 0;

  if (auto *constantStretchFactor = std::get_if<double>(&stretchFactor)) {
    if (*constantStretchFactor == 0)
      throw std::domain_error(
          "stretch_factor must be greater than 0.0x, but was passed " +
          std::to_string(*constantStretchFactor) + "x.");

    initialStretchFactor = *constantStretchFactor;
    expectedNumberOfOutputSamples =
        (((double)input.getNumSamples()) / *constantStretchFactor);
  } else if (auto *variableStretchFactor =
                 std::get_if<std::vector<double>>(&stretchFactor)) {
    for (int i = 0; i < variableStretchFactor->size(); i++) {
      if (variableStretchFactor->data()[i] == 0)
        throw std::domain_error(
            "stretch_factor must be greater than 0.0x, but element at index " +
            std::to_string(i) + " was " +
            std::to_string(variableStretchFactor->data()[i]) + "x.");
    }

    expectedNumberOfOutputSamples =
        (((double)input.getNumSamples()) /
         *std::min_element(variableStretchFactor->begin(),
                           variableStretchFactor->end()));

    if (variableStretchFactor->size() != input.getNumSamples())
      throw std::domain_error(
          "stretch_factor must be the same length as the input audio "
          "buffer, but was passed an array of length " +
          std::to_string(variableStretchFactor->size()) + " instead of " +
          std::to_string(input.getNumSamples()) + " samples.");

    options |= RubberBandStretcher::OptionProcessRealTime;
  }

  if (auto *constantPitchShiftInSemitones =
          std::get_if<double>(&pitchShiftInSemitones)) {
    if (*constantPitchShiftInSemitones < -MAX_SEMITONES_TO_PITCH_SHIFT ||
        *constantPitchShiftInSemitones > MAX_SEMITONES_TO_PITCH_SHIFT)
      throw std::domain_error(
          "pitch_shift_in_semitones must be between -" +
          std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) + " and +" +
          std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) +
          " semitones, but was passed " +
          std::to_string(*constantPitchShiftInSemitones) + " semitones.");
    initialPitchShiftInSemitones = *constantPitchShiftInSemitones;
  } else if (auto *variablePitchShift =
                 std::get_if<std::vector<double>>(&pitchShiftInSemitones)) {
    for (int i = 0; i < variablePitchShift->size(); i++) {
      if (variablePitchShift->data()[i] < -MAX_SEMITONES_TO_PITCH_SHIFT ||
          variablePitchShift->data()[i] > MAX_SEMITONES_TO_PITCH_SHIFT)
        throw std::domain_error(
            "pitch_shift_in_semitones must be between -" +
            std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) + " and +" +
            std::to_string(MAX_SEMITONES_TO_PITCH_SHIFT) +
            " semitones, but element at index " + std::to_string(i) + " was " +
            std::to_string(variablePitchShift->data()[i]) + " semitones.");
    }

    if (variablePitchShift->size() != input.getNumSamples())
      throw std::domain_error(
          "pitch_shift_in_semitones must be the same length as the input audio "
          "buffer, but was passed an array of length " +
          std::to_string(variablePitchShift->size()) + " instead of " +
          std::to_string(input.getNumSamples()) + " samples.");
    options |= RubberBandStretcher::OptionProcessRealTime;
  }

  RubberBandStretcher rubberBandStretcher(
      sampleRate, input.getNumChannels(), options, 1.0 / initialStretchFactor,
      pow(2.0, (initialPitchShiftInSemitones / 12.0)));

  const float **inputChannelPointers =
      (const float **)alloca(sizeof(float *) * input.getNumChannels());

  size_t maximumBlockSize = rubberBandStretcher.getProcessSizeLimit();
  if (!(options & RubberBandStretcher::OptionProcessRealTime)) {
    rubberBandStretcher.setExpectedInputDuration(input.getNumSamples());
    rubberBandStretcher.setMaxProcessSize(maximumBlockSize);

    for (size_t i = 0; i < input.getNumSamples();
         i += STUDY_BLOCK_SAMPLE_SIZE) {
      size_t numSamples =
          std::min(input.getNumSamples() - i, (size_t)STUDY_BLOCK_SAMPLE_SIZE);
      for (int c = 0; c < input.getNumChannels(); c++) {
        inputChannelPointers[c] = input.getReadPointer(c, i);
      }
      bool isLast = i + numSamples >= input.getNumSamples();
      rubberBandStretcher.study(inputChannelPointers, numSamples, isLast);
    }
  }

  juce::AudioBuffer<float> output(input.getNumChannels(),
                                  expectedNumberOfOutputSamples);

  // Keep the buffer we just allocated, but set the size to 0 so we can
  // grow this buffer "for free".
  output.setSize(output.getNumChannels(), 0,
                 /* keepExistingContent */ false, /* clearExtraSpace */ false,
                 /* avoidReallocating */ true);

  float **outputChannelPointers =
      (float **)alloca(sizeof(float *) * output.getNumChannels());

  // An optimization; if we know the pitch and/or stretch factor is constant
  // for a certain amount of time,
  for (size_t i = 0;
       rubberBandStretcher.available() > 0 || i < input.getNumSamples();) {
    if (i < input.getNumSamples()) {
      size_t chunkSize = std::min(maximumBlockSize, input.getNumSamples() - i);

      for (int c = 0; c < input.getNumChannels(); c++) {
        inputChannelPointers[c] = input.getReadPointer(c, i);
      }

      if (options & RubberBandStretcher::OptionProcessRealTime) {
        if (auto *variableStretchFactor =
                std::get_if<std::vector<double>>(&stretchFactor)) {
          chunkSize = chooseChunkSize(chunkSize, i, *variableStretchFactor,
                                      maximumBlockSize);
          rubberBandStretcher.setTimeRatio(1.0 /
                                           variableStretchFactor->data()[i]);
        }

        if (auto *variablePitchShift =
                std::get_if<std::vector<double>>(&pitchShiftInSemitones)) {
          chunkSize = chooseChunkSize(chunkSize, i, *variablePitchShift,
                                      maximumBlockSize);
          double scale = pow(2.0, (variablePitchShift->data()[i] / 12.0));
          rubberBandStretcher.setPitchScale(scale);
        }
      }

      chunkSize = std::min(chunkSize, input.getNumSamples() - i);
      bool isLastCall = i + chunkSize >= input.getNumSamples();
      rubberBandStretcher.process(inputChannelPointers, chunkSize, isLastCall);
      i += chunkSize;
    }

    if (rubberBandStretcher.available() > 0) {
      size_t outputIndex = output.getNumSamples();
      output.setSize(output.getNumChannels(),
                     output.getNumSamples() + rubberBandStretcher.available(),
                     /* keepExistingContent */ true,
                     /* clearExtraSpace */ false,
                     /* avoidReallocating */ true);
      for (int c = 0; c < output.getNumChannels(); c++) {
        outputChannelPointers[c] = output.getWritePointer(c, outputIndex);
      }
      rubberBandStretcher.retrieve(outputChannelPointers,
                                   rubberBandStretcher.available());
    }
  }

  if (rubberBandStretcher.available() > 0) {
    throw std::runtime_error("More samples remained after stretch was done!");
  }

  return output;
}

inline void init_time_stretch(py::module &m) {
  m.def(
      "time_stretch",
      [](py::array_t<float, py::array::c_style> input, double sampleRate,
         std::variant<double, py::array_t<double, py::array::c_style>>
             stretchFactor,
         std::variant<double, py::array_t<double, py::array::c_style>>
             pitchShiftInSemitones,
         bool highQuality, std::string transientMode,
         std::string transientDetector, bool retainPhaseContinuity,
         std::optional<bool> useLongFFTWindow, bool useTimeDomainSmoothing,
         bool preserveFormants) {
        // Convert from Python arrays to std::vector<double> or double:
        std::variant<double, std::vector<double>> cppStretchFactor;
        if (auto *variableStretchFactor =
                std::get_if<py::array_t<double, py::array::c_style>>(
                    &stretchFactor)) {
          py::buffer_info inputInfo = variableStretchFactor->request();
          if (inputInfo.ndim != 1) {
            throw std::domain_error(
                "stretch_factor must be a one-dimensional array of "
                "double-precision floating point numbers, but a " +
                std::to_string(inputInfo.ndim) +
                "-dimensional array was provided.");
          }
          cppStretchFactor = std::vector<double>(
              static_cast<double *>(inputInfo.ptr),
              static_cast<double *>(inputInfo.ptr) + inputInfo.size);
        } else {
          cppStretchFactor = std::get<double>(stretchFactor);
        }

        std::variant<double, std::vector<double>> cppPitchShift;
        if (auto *variablePitchShift =
                std::get_if<py::array_t<double, py::array::c_style>>(
                    &pitchShiftInSemitones)) {
          py::buffer_info inputInfo = variablePitchShift->request();
          if (inputInfo.ndim != 1) {
            throw std::domain_error(
                "stretch_factor must be a one-dimensional array of "
                "double-precision floating point numbers, but a " +
                std::to_string(inputInfo.ndim) +
                "-dimensional array was provided.");
          }
          cppPitchShift = std::vector<double>(
              static_cast<double *>(inputInfo.ptr),
              static_cast<double *>(inputInfo.ptr) + inputInfo.size);
        } else {
          cppPitchShift = std::get<double>(pitchShiftInSemitones);
        }

        juce::AudioBuffer<float> inputBuffer =
            convertPyArrayIntoJuceBuffer(input, detectChannelLayout(input));
        juce::AudioBuffer<float> output;
        {
          py::gil_scoped_release release;
          output = timeStretch(inputBuffer, sampleRate, cppStretchFactor,
                               cppPitchShift, highQuality, transientMode,
                               transientDetector, retainPhaseContinuity,
                               useLongFFTWindow, useTimeDomainSmoothing,
                               preserveFormants);
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

Both ``stretch_factor`` and ``pitch_shift_in_semitones`` can be either floating-point
numbers or NumPy arrays of double-precision floating point numbers. Providing a NumPy
array allows the stretch factor and/or pitch shift to vary over the length of the
output audio.

.. note::
    If a NumPy array is provided for ``stretch_factor`` or ``pitch_shift_in_semitones``:
      - The length of each array must be the same as the length of the input audio.
      - More frequent changes in the stretch factor or pitch shift will result in
        slower processing, as the audio will be processed in smaller chunks.
      - Changes to the ``stretch_factor`` or ``pitch_shift_in_semitones`` more frequent
        than once every 1,024 samples (23 milliseconds at 44.1kHz) will not have any
        effect.

The additional arguments provided to this function allow for more fine-grained control
over the behavior of the time stretcher:

  - ``high_quality`` (the default) enables a higher quality time stretching mode.
    Set this option to ``False`` to use less CPU power.

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


.. note::
    The ability to pass a NumPy array for ``stretch_factor`` and
    ``pitch_shift_in_semitones`` was added in Pedalboard v0.9.8.

)",
      py::arg("input_audio"), py::arg("samplerate"),
      py::arg("stretch_factor") = 1.0,
      py::arg("pitch_shift_in_semitones") = 0.0, py::arg("high_quality") = true,
      py::arg("transient_mode") = "crisp",
      py::arg("transient_detector") = "compound",
      py::arg("retain_phase_continuity") = true,
      py::arg("use_long_fft_window") = py::none(),
      py::arg("use_time_domain_smoothing") = false,
      py::arg("preserve_formants") = true);
}
}; // namespace Pedalboard
