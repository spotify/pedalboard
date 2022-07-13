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

juce::MidiMessageSequence
copyPyArrayIntoJuceMidiMessageSequence(const py::array_t<float, py::array::c_style> midiMessages) {
  py::buffer_info inputInfo = midiMessages.request();

  if(inputInfo.shape[1] != 4) {
     throw std::runtime_error("Each element must have length 4 (got " +
                             std::to_string(inputInfo.shape[1]) + ").");
  }

  juce::MidiMessageSequence midiSequence;

  float *data = static_cast<float *>(inputInfo.ptr);

  for(int i = 0; i < inputInfo.shape[0]; i++) {
    int byte1 = (int) data[i * 4 + 0];
    int byte2 = (int) data[i * 4 + 1];
    int byte3 = (int) data[i * 4 + 2];
    float timeSeconds = data[i * 4 + 3];

    midiSequence.addEvent(juce::MidiMessage(byte1, byte2, byte3), timeSeconds);

    DBG( byte1 );
  }

  return midiSequence;
}

} // namespace Pedalboard