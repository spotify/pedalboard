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

#include "BufferUtils.h"
#include "MidiUtils.h"

#include "Plugin.h"

namespace py = pybind11;

namespace Pedalboard {

/**
 * Non-float32 overload.
 */
template <typename SampleType>
py::array_t<float>
process(const py::array_t<SampleType, py::array::c_style> inputArray,
        const py::array_t<float, py::array::c_style> midiMessages,
        double sampleRate, const std::vector<std::shared_ptr<Plugin>> plugins,
        unsigned int bufferSize, bool reset) {
  const py::array_t<float, py::array::c_style> float32InputArray =
      inputArray.attr("astype")("float32");
  return process(float32InputArray, midiMessages, sampleRate, plugins, bufferSize, reset);
}

/**
 * Single-plugin overload.
 */
template <typename SampleType>
py::array_t<float>
processSingle(const py::array_t<SampleType, py::array::c_style> inputArray,
              const py::array_t<float, py::array::c_style> midiMessages,
              double sampleRate, std::shared_ptr<Plugin> plugin,
              unsigned int bufferSize, bool reset) {
  std::vector<std::shared_ptr<Plugin>> plugins{plugin};
  return process<SampleType>(inputArray, midiMessages, sampleRate, plugins, bufferSize,
                             reset);
}


inline int process(juce::AudioBuffer<float> &ioBuffer,
                   juce::MidiMessageSequence& midiSequence,
                   juce::dsp::ProcessSpec spec,
                   const std::vector<std::shared_ptr<Plugin>> &plugins,
                   bool isProbablyLastProcessCall) {
  int totalOutputLatencySamples = 0;
  int expectedOutputLatency = 0;

  for (auto plugin : plugins) {
    if (!plugin)
      continue;
    expectedOutputLatency += plugin->getLatencyHint();
  }

  int intendedOutputBufferSize = ioBuffer.getNumSamples();

  if (expectedOutputLatency > 0 && isProbablyLastProcessCall) {
    // This is a hint - it's possible that the plugin(s) latency values
    // will change and we'll have to reallocate again later on.
    ioBuffer.setSize(ioBuffer.getNumChannels(),
                     ioBuffer.getNumSamples() + expectedOutputLatency,
                     /* keepExistingContent= */ true,
                     /* clearExtraSpace= */ true);
  }

  // Actually run the plugins over the ioBuffer, in small chunks, to minimize
  // memory usage:
  int startOfOutputInBuffer = 0;
  int lastSampleInBuffer = 0;

  for (auto plugin : plugins) {
    if (!plugin)
      continue;

    int pluginSamplesReceived = 0;

    unsigned int blockSize = spec.maximumBlockSize;
    for (unsigned int blockStart = startOfOutputInBuffer;
         blockStart < (unsigned int)intendedOutputBufferSize;
         blockStart += blockSize) {
      unsigned int blockEnd =
          std::min(blockStart + spec.maximumBlockSize,
                   static_cast<unsigned int>(intendedOutputBufferSize));
      blockSize = blockEnd - blockStart;

      auto ioBlock = juce::dsp::AudioBlock<float>(
          ioBuffer.getArrayOfWritePointers(), ioBuffer.getNumChannels(),
          blockStart, blockSize);
      juce::dsp::ProcessContextReplacing<float> context(ioBlock);

      juce::MidiBuffer midiBuffer;
      auto startTime = blockStart / spec.sampleRate;
      auto endTime = blockEnd / spec.sampleRate;

      for (auto i = midiSequence.getNextIndexAtTime(startTime); i < midiSequence.getNumEvents(); i++)
      {
        juce::MidiMessageSequence::MidiEventHolder *event = midiSequence.getEventPointer(i);

        if (event->message.getTimeStamp() >= startTime && event->message.getTimeStamp() < endTime)
        {
          auto samplePosition = juce::roundToInt((event->message.getTimeStamp() - startTime) * spec.sampleRate);
          midiBuffer.addEvent(event->message, samplePosition);
        } else {
          break;
        }
      }


      int outputSamples = plugin->process(context, midiBuffer);
      if (outputSamples < 0) {
        throw std::runtime_error(
            "A plugin returned a negative number of output samples! "
            "This is an internal Pedalboard error and should be reported.");
      }
      pluginSamplesReceived += outputSamples;

      int missingSamples = blockSize - outputSamples;
      if (missingSamples < 0) {
        throw std::runtime_error(
            "A plugin returned more samples than were asked for! "
            "This is an internal Pedalboard error and should be reported.");
      }

      if (missingSamples > 0 && pluginSamplesReceived > 0) {
        // This can only happen if the plugin we're using is returning us more
        // than one chunk of audio that's not completely full, which can
        // happen sometimes. In this case, we would end up with gaps in the
        // audio output:
        //               empty  empty  full   part
        //              [______|______|AAAAAA|__BBBB]
        //   end of most recently rendered block-->-^
        // We need to consolidate those gaps by moving them forward in time.
        // To do so, we take the section from the earliest known output to the
        // start of this block, and right-align it to the left side of the
        // current block's content:
        //               empty  empty  part   full
        //              [______|______|__AAAA|AABBBB]
        //   end of most recently rendered block-->-^
        for (int c = 0; c < ioBuffer.getNumChannels(); c++) {
          // Only move the samples received before this latest block was
          // rendered, as audio is right-aligned within blocks by convention.
          int samplesToMove = pluginSamplesReceived - outputSamples;
          float *outputStart =
              ioBuffer.getWritePointer(c) + totalOutputLatencySamples;
          float *expectedOutputEnd =
              ioBuffer.getWritePointer(c) + blockEnd - outputSamples;
          float *expectedOutputStart = expectedOutputEnd - samplesToMove;

          std::memmove((char *)expectedOutputStart, (char *)outputStart,
                       sizeof(float) * samplesToMove);
        }
      }

      lastSampleInBuffer =
          std::max(lastSampleInBuffer, (int)(blockStart + outputSamples));
      startOfOutputInBuffer += missingSamples;
      totalOutputLatencySamples += missingSamples;

      if (missingSamples && isProbablyLastProcessCall) {
        // Resize the IO buffer to give us a bit more room
        // on the end, so we can continue to write delayed output.
        // Only do this if we think this is the last time process is called.
        intendedOutputBufferSize += missingSamples;

        // If we need to reallocate, then we reallocate.
        if (intendedOutputBufferSize > ioBuffer.getNumSamples()) {
          ioBuffer.setSize(ioBuffer.getNumChannels(), intendedOutputBufferSize,
                           /* keepExistingContent= */ true,
                           /* clearExtraSpace= */ true);
        }
      }
    }
  }

  // Trim the output buffer down to size; this operation should be
  // allocation-free.
  jassert(intendedOutputBufferSize <= ioBuffer.getNumSamples());
  ioBuffer.setSize(ioBuffer.getNumChannels(), intendedOutputBufferSize,
                   /* keepExistingContent= */ true,
                   /* clearExtraSpace= */ true,
                   /* avoidReallocating= */ true);
  return intendedOutputBufferSize - totalOutputLatencySamples;
}

inline int process(juce::AudioBuffer<float> &ioBuffer,
                   juce::MidiBuffer& midiBuffer,
                   juce::dsp::ProcessSpec spec,
                   const std::vector<std::shared_ptr<Plugin>> &plugins,
                   bool isProbablyLastProcessCall) {
  juce::MidiMessageSequence midiSequence;

  int time;
  juce::MidiMessage m;

  for (juce::MidiBuffer::Iterator i(midiBuffer); i.getNextEvent (m, time);)
  {
    midiSequence.addEvent(m, time / spec.sampleRate);
  }

  return process(ioBuffer, midiSequence, spec, plugins, isProbablyLastProcessCall);
}


/**
 * Process a given audio buffer through a list of
 * Pedalboard plugins at a given sample rate.
 * Only supports float processing, not double, at the moment.
 */
template <>
py::array_t<float>
process<float>(const py::array_t<float, py::array::c_style> inputArray,
               const py::array_t<float, py::array::c_style> midiMessages,
               double sampleRate, std::vector<std::shared_ptr<Plugin>> plugins,
               unsigned int bufferSize, bool reset) {
  const ChannelLayout inputChannelLayout = detectChannelLayout(inputArray);
  juce::AudioBuffer<float> ioBuffer = copyPyArrayIntoJuceBuffer(inputArray);
  juce::MidiMessageSequence midiSequence = copyPyArrayIntoJuceMidiMessageSequence(midiMessages);

  int totalOutputLatencySamples;

  {
    py::gil_scoped_release release;

    bufferSize = std::min(bufferSize, (unsigned int)ioBuffer.getNumSamples());

    // We'd pass multiple arguments to scoped_lock here, but we don't know how
    // many plugins have been passed at compile time - so instead, we do our own
    // deadlock-avoiding multiple-lock algorithm here. By locking each plugin
    // only in order of its pointers, we're guaranteed to avoid deadlocks with
    // other threads that may be running this same code on the same plugins.
    std::vector<std::shared_ptr<Plugin>> allPlugins;
    for (auto plugin : plugins) {
      if (!plugin)
        continue;
      allPlugins.push_back(plugin);
      if (auto pluginContainer =
              dynamic_cast<PluginContainer *>(plugin.get())) {
        auto children = pluginContainer->getAllPlugins();
        allPlugins.insert(allPlugins.end(), children.begin(), children.end());
      }
    }

    std::sort(allPlugins.begin(), allPlugins.end(),
              [](const std::shared_ptr<Plugin> lhs,
                 const std::shared_ptr<Plugin> rhs) {
                return lhs.get() < rhs.get();
              });

    bool containsDuplicates =
        std::adjacent_find(allPlugins.begin(), allPlugins.end()) !=
        allPlugins.end();

    if (containsDuplicates) {
      throw std::runtime_error(
          "The same plugin instance is being used multiple times in the same "
          "chain of plugins, which would cause undefined results. Please "
          "ensure that no duplicate plugins are present before calling.");
    }

    std::vector<std::unique_ptr<std::scoped_lock<std::mutex>>> pluginLocks;
    for (auto plugin : allPlugins) {
      pluginLocks.push_back(
          std::make_unique<std::scoped_lock<std::mutex>>(plugin->mutex));
    }

    if (reset) {
      for (auto plugin : plugins) {
        if (!plugin)
          continue;
        plugin->reset();
      }
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(bufferSize);
    spec.numChannels = static_cast<juce::uint32>(ioBuffer.getNumChannels());

    for (auto plugin : plugins) {
      if (!plugin)
        continue;
      plugin->prepare(spec);
    }

    // Actually run the process method of all plugins.
    int samplesReturned = process(ioBuffer, midiSequence, spec, plugins, reset);
    totalOutputLatencySamples = ioBuffer.getNumSamples() - samplesReturned;
  }

  return copyJuceBufferIntoPyArray(ioBuffer, inputChannelLayout,
                                   totalOutputLatencySamples,
                                   inputArray.request().ndim);
}
} // namespace Pedalboard