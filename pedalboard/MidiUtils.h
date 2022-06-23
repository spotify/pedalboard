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

#pragma once
#include "JuceHeader.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace Pedalboard {

juce::MidiBuffer
copyPyArrayIntoJuceMidiBuffer(const py::array_t<int, py::array::c_style> midiMessages) {
  // Numpy/Librosa convention is (num_samples, num_channels)
  py::buffer_info inputInfo = midiMessages.request();

  return juce::MidiBuffer();
}

} // namespace Pedalboard