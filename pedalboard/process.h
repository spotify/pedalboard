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

#include "Plugin.h"

namespace py = pybind11;

namespace Pedalboard {
enum class ChannelLayout {
  Interleaved,
  NotInterleaved,
};

/**
 * Non-float32 overload.
 */
template <typename SampleType>
py::array_t<float>
process(const py::array_t<SampleType, py::array::c_style> inputArray,
        double sampleRate, const std::vector<Plugin *> &plugins,
        unsigned int bufferSize, bool reset) {
  const py::array_t<float, py::array::c_style> float32InputArray =
      inputArray.attr("astype")("float32");
  return process(float32InputArray, sampleRate, plugins, bufferSize, reset);
}

/**
 * Single-plugin overload.
 */
template <typename SampleType>
py::array_t<float>
processSingle(const py::array_t<SampleType, py::array::c_style> inputArray,
              double sampleRate, Plugin &plugin, unsigned int bufferSize,
              bool reset) {
  std::vector<Plugin *> plugins{&plugin};
  return process<SampleType>(inputArray, sampleRate, plugins, bufferSize,
                             reset);
}

/**
 * Process a given audio buffer through a list of
 * Pedalboard plugins at a given sample rate.
 * Only supports float processing, not double, at the moment.
 */
template <>
py::array_t<float>
process<float>(const py::array_t<float, py::array::c_style> inputArray,
               double sampleRate, const std::vector<Plugin *> &plugins,
               unsigned int bufferSize, bool reset) {
  // Numpy/Librosa convention is (num_samples, num_channels)
  py::buffer_info inputInfo = inputArray.request();

  unsigned int numChannels = 0;
  unsigned int numSamples = 0;
  ChannelLayout inputChannelLayout;

  if (inputInfo.ndim == 1) {
    numSamples = inputInfo.shape[0];
    numChannels = 1;
    inputChannelLayout = ChannelLayout::Interleaved;
  } else if (inputInfo.ndim == 2) {
    // Try to auto-detect the channel layout from the shape
    if (inputInfo.shape[1] < inputInfo.shape[0]) {
      numSamples = inputInfo.shape[0];
      numChannels = inputInfo.shape[1];
      inputChannelLayout = ChannelLayout::Interleaved;
    } else if (inputInfo.shape[0] < inputInfo.shape[1]) {
      numSamples = inputInfo.shape[1];
      numChannels = inputInfo.shape[0];
      inputChannelLayout = ChannelLayout::NotInterleaved;
    } else {
      throw std::runtime_error(
          "Unable to determine channel layout from shape!");
    }
  } else {
    throw std::runtime_error("Number of input dimensions must be 1 or 2.");
  }

  if (numChannels == 0) {
    throw std::runtime_error("No channels passed!");
  } else if (numChannels > 2) {
    throw std::runtime_error("More than two channels received!");
  }

  // Cap the buffer size in use to the size of the input data:
  bufferSize = std::min(bufferSize, numSamples);

  // JUCE uses separate channel buffers, so the output shape is (num_channels,
  // num_samples)
  py::array_t<float> outputArray =
      inputInfo.ndim == 2 ? py::array_t<float>({numChannels, numSamples})
                          : py::array_t<float>(numSamples);
  py::buffer_info outputInfo = outputArray.request();

  {
    py::gil_scoped_release release;

    unsigned int countOfPluginsIgnoringNull = 0;
    for (auto *plugin : plugins) {
      if (plugin == nullptr)
        continue;
      countOfPluginsIgnoringNull++;
    }

    // We'd pass multiple arguments to scoped_lock here, but we don't know how
    // many plugins have been passed at compile time - so instead, we do our own
    // deadlock-avoiding multiple-lock algorithm here. By locking each plugin
    // only in order of its pointers, we're guaranteed to avoid deadlocks with
    // other threads that may be running this same code on the same plugins.
    std::vector<Plugin *> uniquePluginsSortedByPointer;
    for (auto *plugin : plugins) {
      if (plugin == nullptr)
        continue;

      if (std::find(uniquePluginsSortedByPointer.begin(),
                    uniquePluginsSortedByPointer.end(),
                    plugin) == uniquePluginsSortedByPointer.end())
        uniquePluginsSortedByPointer.push_back(plugin);
    }

    if (uniquePluginsSortedByPointer.size() < countOfPluginsIgnoringNull) {
      throw std::runtime_error(
          "The same plugin instance is being used multiple times in the same "
          "chain of plugins, which would cause undefined results.");
    }

    std::sort(uniquePluginsSortedByPointer.begin(),
              uniquePluginsSortedByPointer.end(),
              [](const Plugin *lhs, const Plugin *rhs) { return lhs < rhs; });

    std::vector<std::unique_ptr<std::scoped_lock<std::mutex>>> pluginLocks;
    for (auto *plugin : uniquePluginsSortedByPointer) {
      pluginLocks.push_back(
          std::make_unique<std::scoped_lock<std::mutex>>(plugin->mutex));
    }

    if (reset) {
      for (auto *plugin : plugins) {
        if (plugin == nullptr)
          continue;
        plugin->reset();
      }
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(bufferSize);
    spec.numChannels = static_cast<juce::uint32>(numChannels);

    for (auto *plugin : plugins) {
      if (plugin == nullptr)
        continue;
      plugin->prepare(spec);
    }

    // Manually construct channel pointers to pass to AudioBuffer.
    std::vector<float *> ioBufferChannelPointers(numChannels);
    for (unsigned int i = 0; i < numChannels; i++) {
      ioBufferChannelPointers[i] = ((float *)outputInfo.ptr) + (i * numSamples);
    }

    juce::AudioBuffer<float> ioBuffer(ioBufferChannelPointers.data(),
                                      numChannels, numSamples);

    for (unsigned int blockStart = 0; blockStart < numSamples;
         blockStart += bufferSize) {
      unsigned int blockEnd = std::min(blockStart + bufferSize,
                                       static_cast<unsigned int>(numSamples));
      unsigned int blockSize = blockEnd - blockStart;

      // Copy the input audio into the ioBuffer, which will be used for
      // processing and will be returned.

      // Depending on the input channel layout, we need to copy data
      // differently. This loop is duplicated here to move the if statement
      // outside of the tight loop, as we don't need to re-check that the input
      // channel is still the same on every iteration of the loop.
      switch (inputChannelLayout) {
      case ChannelLayout::Interleaved:
        for (unsigned int i = 0; i < numChannels; i++) {
          // We're de-interleaving the data here, so we can't use std::copy.
          for (unsigned int j = blockStart; j < blockEnd; j++) {
            ioBufferChannelPointers[i][j] =
                static_cast<float *>(inputInfo.ptr)[j * numChannels + i];
          }
        }
        break;
      case ChannelLayout::NotInterleaved:
        for (unsigned int i = 0; i < numChannels; i++) {
          const float *channelBuffer =
              static_cast<float *>(inputInfo.ptr) + (i * numSamples);
          std::copy(channelBuffer + blockStart, channelBuffer + blockEnd,
                    ioBufferChannelPointers[i] + blockStart);
        }
      }

      auto ioBlock = juce::dsp::AudioBlock<float>(
          ioBufferChannelPointers.data(), numChannels, blockStart, blockSize);
      juce::dsp::ProcessContextReplacing<float> context(ioBlock);

      // Now all of the pointers in context are pointing to valid input data,
      // so let's run the plugins.
      for (auto *plugin : plugins) {
        if (plugin == nullptr)
          continue;
        plugin->process(context);
      }
    }
  }

  switch (inputChannelLayout) {
  case ChannelLayout::Interleaved:
    return outputArray.attr("transpose")();
  case ChannelLayout::NotInterleaved:
  default:
    return outputArray;
  }
};
} // namespace Pedalboard