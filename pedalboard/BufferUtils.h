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
enum class ChannelLayout {
  Interleaved,
  NotInterleaved,
};

template <typename T>
ChannelLayout
detectChannelLayout(const py::array_t<T, py::array::c_style> inputArray,
                    std::optional<int> channelCountHint = {}) {
  py::buffer_info inputInfo = inputArray.request();

  if (inputInfo.ndim == 1) {
    return ChannelLayout::NotInterleaved;
  } else if (inputInfo.ndim == 2) {
    if (channelCountHint) {
      if (inputInfo.shape[0] == inputInfo.shape[1] && inputInfo.shape[0] > 1) {
        throw std::runtime_error(
            "Unable to determine channel layout from shape: (" +
            std::to_string(inputInfo.shape[0]) + ", " +
            std::to_string(inputInfo.shape[1]) + ").");
      } else if (inputInfo.shape[0] == *channelCountHint) {
        return ChannelLayout::NotInterleaved;
      } else if (inputInfo.shape[1] == *channelCountHint) {
        return ChannelLayout::Interleaved;
      } else {
        // The hint was not used; fall through to the next case.
      }
    }

    // Try to auto-detect the channel layout from the shape
    if (inputInfo.shape[0] == 0 && inputInfo.shape[1] > 0) {
      // Zero channels doesn't make sense; but zero samples does.
      return ChannelLayout::Interleaved;
    } else if (inputInfo.shape[1] == 0 && inputInfo.shape[0] > 0) {
      return ChannelLayout::NotInterleaved;
    } else if (inputInfo.shape[1] < inputInfo.shape[0]) {
      return ChannelLayout::Interleaved;
    } else if (inputInfo.shape[0] < inputInfo.shape[1]) {
      return ChannelLayout::NotInterleaved;
    } else if (inputInfo.shape[0] == 1 || inputInfo.shape[1] == 1) {
      // Do we only have one sample? Then the layout doesn't matter:
      return ChannelLayout::NotInterleaved;
    } else {
      throw std::runtime_error(
          "Unable to determine channel layout from shape: (" +
          std::to_string(inputInfo.shape[0]) + ", " +
          std::to_string(inputInfo.shape[1]) + ").");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2 (got " +
                             std::to_string(inputInfo.ndim) + ").");
  }
}

template <typename T>
juce::AudioBuffer<T> copyPyArrayIntoJuceBuffer(
    const py::array_t<T, py::array::c_style> inputArray,
    std::optional<ChannelLayout> providedChannelLayout = {}) {
  // Numpy/Librosa convention is (num_samples, num_channels)
  py::buffer_info inputInfo = inputArray.request();

  unsigned int numChannels = 0;
  unsigned int numSamples = 0;

  ChannelLayout inputChannelLayout;
  if (providedChannelLayout) {
    inputChannelLayout = *providedChannelLayout;
  } else {
    inputChannelLayout = detectChannelLayout(inputArray);
  }

  if (inputInfo.ndim == 1) {
    numSamples = inputInfo.shape[0];
    numChannels = 1;
  } else if (inputInfo.ndim == 2) {
    // Try to auto-detect the channel layout from the shape
    switch (inputChannelLayout) {
    case ChannelLayout::NotInterleaved:
      numSamples = inputInfo.shape[1];
      numChannels = inputInfo.shape[0];
      break;
    case ChannelLayout::Interleaved:
      numSamples = inputInfo.shape[0];
      numChannels = inputInfo.shape[1];
      break;
    default:
      throw std::runtime_error("Unable to determine shape of audio input!");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2 (got " +
                             std::to_string(inputInfo.ndim) + ").");
  }

  juce::AudioBuffer<T> ioBuffer(numChannels, numSamples);

  // Depending on the input channel layout, we need to copy data
  // differently. This loop is duplicated here to move the if statement
  // outside of the tight loop, as we don't need to re-check that the input
  // channel is still the same on every iteration of the loop.
  switch (inputChannelLayout) {
  case ChannelLayout::Interleaved:
    for (unsigned int i = 0; i < numChannels; i++) {
      T *channelBuffer = ioBuffer.getWritePointer(i);
      // We're de-interleaving the data here, so we can't use copyFrom.
      for (unsigned int j = 0; j < numSamples; j++) {
        channelBuffer[j] = static_cast<T *>(inputInfo.ptr)[j * numChannels + i];
      }
    }
    break;
  case ChannelLayout::NotInterleaved:
    for (unsigned int i = 0; i < numChannels; i++) {
      ioBuffer.copyFrom(
          i, 0, static_cast<T *>(inputInfo.ptr) + (numSamples * i), numSamples);
    }
    break;
  default:
    throw std::runtime_error("Internal error: got unexpected channel layout.");
  }

  return ioBuffer;
}

/**
 * Convert a Python array into a const JUCE AudioBuffer, avoiding copying the
 * data if provided in the appropriate format.
 */
template <typename T>
const juce::AudioBuffer<T> convertPyArrayIntoJuceBuffer(
    const py::array_t<T, py::array::c_style> &inputArray,
    std::optional<ChannelLayout> providedLayout = {}) {
  ChannelLayout inputChannelLayout;
  if (providedLayout) {
    inputChannelLayout = *providedLayout;
  } else {
    inputChannelLayout = detectChannelLayout(inputArray);
  }

  switch (inputChannelLayout) {
  case ChannelLayout::Interleaved:
    return copyPyArrayIntoJuceBuffer(inputArray);
  case ChannelLayout::NotInterleaved: {
    // Return a JUCE AudioBuffer that just points to the original PyArray:
    py::buffer_info inputInfo = inputArray.request();

    unsigned int numChannels = 0;
    unsigned int numSamples = 0;

    if (inputInfo.ndim == 1) {
      numSamples = inputInfo.shape[0];
      numChannels = 1;
    } else if (inputInfo.ndim == 2) {
      switch (inputChannelLayout) {
      case ChannelLayout::NotInterleaved:
        numSamples = inputInfo.shape[1];
        numChannels = inputInfo.shape[0];
        break;
      case ChannelLayout::Interleaved:
        numSamples = inputInfo.shape[0];
        numChannels = inputInfo.shape[1];
        break;
      default:
        throw std::runtime_error("Unable to determine shape of audio input!");
      }
    } else {
      throw std::runtime_error(
          "Number of input dimensions must be 1 or 2 (got " +
          std::to_string(inputInfo.ndim) + ").");
    }

    T **channelPointers = (T **)alloca(numChannels * sizeof(T *));
    for (int c = 0; c < numChannels; c++) {
      channelPointers[c] = static_cast<T *>(inputInfo.ptr) + (c * numSamples);
    }

    return juce::AudioBuffer<T>(channelPointers, numChannels, numSamples);
  }
  default:
    throw std::runtime_error("Internal error: got unexpected channel layout.");
  }
}

template <typename T>
py::array_t<T> copyJuceBufferIntoPyArray(const juce::AudioBuffer<T> &juceBuffer,
                                         ChannelLayout channelLayout,
                                         int offsetSamples, int ndim = 2) {
  unsigned int numChannels = juceBuffer.getNumChannels();
  unsigned int numSamples = juceBuffer.getNumSamples();
  unsigned int outputSampleCount =
      std::max((int)numSamples - (int)offsetSamples, 0);

  // TODO: Avoid the need to copy here if offsetSamples is 0!
  py::array_t<T> outputArray;
  if (ndim == 2) {
    switch (channelLayout) {
    case ChannelLayout::Interleaved:
      outputArray = py::array_t<T>({outputSampleCount, numChannels});
      break;
    case ChannelLayout::NotInterleaved:
      outputArray = py::array_t<T>({numChannels, outputSampleCount});
      break;
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }
  } else {
    outputArray = py::array_t<T>(outputSampleCount);
  }

  py::buffer_info outputInfo = outputArray.request();

  // Depending on the input channel layout, we need to copy data
  // differently. This loop is duplicated here to move the if statement
  // outside of the tight loop, as we don't need to re-check that the input
  // channel is still the same on every iteration of the loop.
  T *outputBasePointer = static_cast<T *>(outputInfo.ptr);

  if (juceBuffer.getNumSamples() > 0) {
    switch (channelLayout) {
    case ChannelLayout::Interleaved:
      for (unsigned int i = 0; i < numChannels; i++) {
        const T *channelBuffer = juceBuffer.getReadPointer(i, offsetSamples);
        // We're interleaving the data here, so we can't use copyFrom.
        for (unsigned int j = 0; j < outputSampleCount; j++) {
          outputBasePointer[j * numChannels + i] = channelBuffer[j];
        }
      }
      break;
    case ChannelLayout::NotInterleaved:
      for (unsigned int i = 0; i < numChannels; i++) {
        const T *channelBuffer = juceBuffer.getReadPointer(i, offsetSamples);
        std::copy(channelBuffer, channelBuffer + outputSampleCount,
                  &outputBasePointer[outputSampleCount * i]);
      }
      break;
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }
  }

  return outputArray;
}
} // namespace Pedalboard