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

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

namespace nb = nanobind;

namespace Pedalboard {
enum class ChannelLayout {
  Interleaved,
  NotInterleaved,
};

/**
 * Detect the channel layout of a NumPy array.
 * For nanobind, we use nb::ndarray<> which provides direct access to shape/ndim.
 */
template <typename T>
ChannelLayout
detectChannelLayout(nb::ndarray<T, nb::c_contig, nb::device::cpu> inputArray,
                    std::optional<int> channelCountHint = {}) {
  size_t ndim = inputArray.ndim();
  
  if (ndim == 1) {
    return ChannelLayout::NotInterleaved;
  } else if (ndim == 2) {
    size_t shape0 = inputArray.shape(0);
    size_t shape1 = inputArray.shape(1);
    
    if (channelCountHint) {
      if (shape0 == shape1 && shape0 > 1) {
        throw std::runtime_error(
            "Unable to determine channel layout from shape: (" +
            std::to_string(shape0) + ", " +
            std::to_string(shape1) + ").");
      } else if (shape0 == (size_t)*channelCountHint) {
        return ChannelLayout::NotInterleaved;
      } else if (shape1 == (size_t)*channelCountHint) {
        return ChannelLayout::Interleaved;
      } else {
        // The hint was not used; fall through to the next case.
      }
    }

    // Try to auto-detect the channel layout from the shape
    if (shape0 == 0 && shape1 > 0) {
      // Zero channels doesn't make sense; but zero samples does.
      return ChannelLayout::Interleaved;
    } else if (shape1 == 0 && shape0 > 0) {
      return ChannelLayout::NotInterleaved;
    } else if (shape1 < shape0) {
      return ChannelLayout::Interleaved;
    } else if (shape0 < shape1) {
      return ChannelLayout::NotInterleaved;
    } else if (shape0 == 1 || shape1 == 1) {
      // Do we only have one sample? Then the layout doesn't matter:
      return ChannelLayout::NotInterleaved;
    } else {
      throw std::runtime_error(
          "Unable to determine channel layout from shape: (" +
          std::to_string(shape0) + ", " +
          std::to_string(shape1) + ").");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2 (got " +
                             std::to_string(ndim) + ").");
  }
}

/**
 * Overload for generic ndarray (used when dtype is not known at compile time)
 */
inline ChannelLayout
detectChannelLayout(nb::ndarray<> inputArray,
                    std::optional<int> channelCountHint = {}) {
  size_t ndim = inputArray.ndim();
  
  if (ndim == 1) {
    return ChannelLayout::NotInterleaved;
  } else if (ndim == 2) {
    size_t shape0 = inputArray.shape(0);
    size_t shape1 = inputArray.shape(1);
    
    if (channelCountHint) {
      if (shape0 == shape1 && shape0 > 1) {
        throw std::runtime_error(
            "Unable to determine channel layout from shape: (" +
            std::to_string(shape0) + ", " +
            std::to_string(shape1) + ").");
      } else if (shape0 == (size_t)*channelCountHint) {
        return ChannelLayout::NotInterleaved;
      } else if (shape1 == (size_t)*channelCountHint) {
        return ChannelLayout::Interleaved;
      } else {
        // The hint was not used; fall through to the next case.
      }
    }

    // Try to auto-detect the channel layout from the shape
    if (shape0 == 0 && shape1 > 0) {
      return ChannelLayout::Interleaved;
    } else if (shape1 == 0 && shape0 > 0) {
      return ChannelLayout::NotInterleaved;
    } else if (shape1 < shape0) {
      return ChannelLayout::Interleaved;
    } else if (shape0 < shape1) {
      return ChannelLayout::NotInterleaved;
    } else if (shape0 == 1 || shape1 == 1) {
      return ChannelLayout::NotInterleaved;
    } else {
      throw std::runtime_error(
          "Unable to determine channel layout from shape: (" +
          std::to_string(shape0) + ", " +
          std::to_string(shape1) + ").");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2 (got " +
                             std::to_string(ndim) + ").");
  }
}

template <typename T>
juce::AudioBuffer<T> copyPyArrayIntoJuceBuffer(
    nb::ndarray<T, nb::c_contig, nb::device::cpu> inputArray,
    std::optional<ChannelLayout> providedChannelLayout = {}) {
  // Numpy/Librosa convention is (num_samples, num_channels)
  size_t ndim = inputArray.ndim();

  unsigned int numChannels = 0;
  unsigned int numSamples = 0;

  ChannelLayout inputChannelLayout;
  if (providedChannelLayout) {
    inputChannelLayout = *providedChannelLayout;
  } else {
    inputChannelLayout = detectChannelLayout(inputArray);
  }

  if (ndim == 1) {
    numSamples = inputArray.shape(0);
    numChannels = 1;
  } else if (ndim == 2) {
    // Try to auto-detect the channel layout from the shape
    switch (inputChannelLayout) {
    case ChannelLayout::NotInterleaved:
      numSamples = inputArray.shape(1);
      numChannels = inputArray.shape(0);
      break;
    case ChannelLayout::Interleaved:
      numSamples = inputArray.shape(0);
      numChannels = inputArray.shape(1);
      break;
    default:
      throw std::runtime_error("Unable to determine shape of audio input!");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2 (got " +
                             std::to_string(ndim) + ").");
  }

  juce::AudioBuffer<T> ioBuffer(numChannels, numSamples);

  // Get raw pointer to the data
  const T *inputPtr = inputArray.data();

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
        channelBuffer[j] = inputPtr[j * numChannels + i];
      }
    }
    break;
  case ChannelLayout::NotInterleaved:
    for (unsigned int i = 0; i < numChannels; i++) {
      ioBuffer.copyFrom(
          i, 0, inputPtr + (numSamples * i), numSamples);
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
    nb::ndarray<T, nb::c_contig, nb::device::cpu> inputArray,
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
    size_t ndim = inputArray.ndim();

    unsigned int numChannels = 0;
    unsigned int numSamples = 0;

    if (ndim == 1) {
      numSamples = inputArray.shape(0);
      numChannels = 1;
    } else if (ndim == 2) {
      switch (inputChannelLayout) {
      case ChannelLayout::NotInterleaved:
        numSamples = inputArray.shape(1);
        numChannels = inputArray.shape(0);
        break;
      case ChannelLayout::Interleaved:
        numSamples = inputArray.shape(0);
        numChannels = inputArray.shape(1);
        break;
      default:
        throw std::runtime_error("Unable to determine shape of audio input!");
      }
    } else {
      throw std::runtime_error(
          "Number of input dimensions must be 1 or 2 (got " +
          std::to_string(ndim) + ").");
    }

    T **channelPointers = (T **)alloca(numChannels * sizeof(T *));
    T *dataPtr = const_cast<T*>(inputArray.data());
    for (unsigned int c = 0; c < numChannels; c++) {
      channelPointers[c] = dataPtr + (c * numSamples);
    }

    return juce::AudioBuffer<T>(channelPointers, numChannels, numSamples);
  }
  default:
    throw std::runtime_error("Internal error: got unexpected channel layout.");
  }
}

/**
 * Copy a JUCE AudioBuffer into a new NumPy array.
 * This function allocates new memory for the output array.
 */
template <typename T>
nb::ndarray<nb::numpy, T> copyJuceBufferIntoPyArray(const juce::AudioBuffer<T> &juceBuffer,
                                                     ChannelLayout channelLayout,
                                                     int offsetSamples, int ndim = 2) {
  unsigned int numChannels = juceBuffer.getNumChannels();
  unsigned int numSamples = juceBuffer.getNumSamples();
  unsigned int outputSampleCount =
      std::max((int)numSamples - (int)offsetSamples, 0);

  // Allocate memory for the output
  size_t totalElements;
  size_t shape[2];
  size_t actualNdim;
  
  if (ndim == 2) {
    switch (channelLayout) {
    case ChannelLayout::Interleaved:
      shape[0] = outputSampleCount;
      shape[1] = numChannels;
      break;
    case ChannelLayout::NotInterleaved:
      shape[0] = numChannels;
      shape[1] = outputSampleCount;
      break;
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }
    totalElements = shape[0] * shape[1];
    actualNdim = 2;
  } else {
    shape[0] = outputSampleCount;
    totalElements = outputSampleCount;
    actualNdim = 1;
  }

  // Allocate the output buffer
  T *outputData = new T[totalElements];
  
  // Create a capsule to own the memory and ensure it gets freed
  nb::capsule owner(outputData, [](void *p) noexcept {
    delete[] static_cast<T*>(p);
  });

  // Copy the data
  if (juceBuffer.getNumSamples() > 0) {
    switch (channelLayout) {
    case ChannelLayout::Interleaved:
      for (unsigned int i = 0; i < numChannels; i++) {
        const T *channelBuffer = juceBuffer.getReadPointer(i, offsetSamples);
        // We're interleaving the data here
        for (unsigned int j = 0; j < outputSampleCount; j++) {
          outputData[j * numChannels + i] = channelBuffer[j];
        }
      }
      break;
    case ChannelLayout::NotInterleaved:
      for (unsigned int i = 0; i < numChannels; i++) {
        const T *channelBuffer = juceBuffer.getReadPointer(i, offsetSamples);
        std::copy(channelBuffer, channelBuffer + outputSampleCount,
                  &outputData[outputSampleCount * i]);
      }
      break;
    default:
      throw std::runtime_error(
          "Internal error: got unexpected channel layout.");
    }
  }

  // Create and return the ndarray
  if (actualNdim == 2) {
    return nb::ndarray<nb::numpy, T>(outputData, actualNdim, shape, owner);
  } else {
    return nb::ndarray<nb::numpy, T>(outputData, actualNdim, shape, owner);
  }
}
} // namespace Pedalboard