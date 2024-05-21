/*
 * pedalboard
 * Copyright 2022 Spotify AB
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
#include "../plugins/AddLatency.h"
#include <mutex>

namespace Pedalboard {

/**
 * A template class that wraps a Pedalboard plugin,
 * but ensures that its process() function is only ever passed a fixed
 * block size. This block size can be set in the prepare() method, or as a
 * template argument.
 */
template <typename T, unsigned int DefaultBlockSize = 0,
          typename SampleType = float>
class FixedBlockSize : public Plugin {
public:
  virtual ~FixedBlockSize(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    if (lastSpec.sampleRate != spec.sampleRate ||
        lastSpec.maximumBlockSize != spec.maximumBlockSize ||
        lastSpec.numChannels != spec.numChannels) {
      if (spec.maximumBlockSize % blockSize == 0) {
        // We need much less intermediate memory in this case:
        inputBuffer.setSize(spec.numChannels, blockSize);
        outputBuffer.clear();
        inStreamLatency = 0;
      } else {
        inputBuffer.setSize(spec.numChannels,
                            blockSize * 2 + spec.maximumBlockSize * 2);
        outputBuffer.setSize(spec.numChannels,
                             blockSize * 2 + spec.maximumBlockSize * 2);
        // Add enough latency to the stream to allow us to process an entire
        // block:
        inStreamLatency = blockSize;
      }
      lastSpec = spec;
    }

    // Tell the delegate plugin that its maximum block
    // size is the fixed size we'll be sending in:
    juce::dsp::ProcessSpec newSpec = spec;
    newSpec.maximumBlockSize = blockSize;
    plugin.prepare(newSpec);
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<SampleType> &context) {
    auto ioBlock = context.getOutputBlock();

    if (lastSpec.maximumBlockSize % blockSize == 0) {
      // The best case scenario: the input is evenly divisible
      // by the fixed block size, so we need no buffers!
      int samplesOutput = 0;
      for (int i = 0; i < ioBlock.getNumSamples(); i += blockSize) {
        int samplesAvailable =
            std::min((int)blockSize, (int)ioBlock.getNumSamples() - i);
        if (samplesAvailable < blockSize)
          break;

        juce::dsp::AudioBlock<SampleType> subBlock =
            ioBlock.getSubBlock(i, blockSize);
        juce::dsp::ProcessContextReplacing<SampleType> subContext(subBlock);
        int samplesOutputThisBlock = plugin.process(subContext);

        if (samplesOutput > 0 && samplesOutputThisBlock < blockSize) {
          throw std::runtime_error(
              "Plugin that using FixedBlockSize "
              "returned too few samples! This is an internal Pedalboard error "
              "and should be reported.");
        }
        samplesOutput += samplesOutputThisBlock;
      }

      int remainderInSamples = ioBlock.getNumSamples() % blockSize;
      if (remainderInSamples > 0) {
        // We're at the end of our buffer, so pad with zeros.
        int offset = ioBlock.getNumSamples() - remainderInSamples;

        // Copy the remainder into inputBuffer:
        juce::dsp::AudioBlock<SampleType> inputBlock(inputBuffer);
        juce::dsp::AudioBlock<SampleType> subBlock =
            inputBlock.getSubBlock(0, blockSize);

        subBlock.clear();
        subBlock.copyFrom(ioBlock.getSubBlock(offset, remainderInSamples));

        juce::dsp::ProcessContextReplacing<SampleType> subContext(subBlock);
        int samplesOutputThisBlock = plugin.process(subContext);

        // Copy the output back into ioBlock, right-aligned:
        ioBlock
            .getSubBlock(ioBlock.getNumSamples() - remainderInSamples,
                         remainderInSamples)
            .copyFrom(subBlock);

        samplesOutput += remainderInSamples;
      }

      samplesProcessed += samplesOutput;
      return samplesOutput;
    } else {
      // We have to render three parts:
      // 1) Push as many samples as possible into inputBuffer
      if (inputBuffer.getNumSamples() - inputBufferSamples <
          ioBlock.getNumSamples()) {
        throw std::runtime_error("Input buffer overflow! This is an internal "
                                 "Pedalboard error and should be reported.");
      }

      ioBlock.copyTo(inputBuffer, 0, inputBufferSamples,
                     ioBlock.getNumSamples());
      inputBufferSamples += ioBlock.getNumSamples();

      // 2) Copy the output from the previous render call into the ioBlock
      int samplesOutput = 0;

      if (outputBufferSamples >= ioBlock.getNumSamples()) {
        ioBlock.copyFrom(outputBuffer, 0, 0, ioBlock.getNumSamples());
        outputBufferSamples -= ioBlock.getNumSamples();

        // Move the remainder of the output buffer to the left:
        if (outputBufferSamples > 0) {
          for (int i = 0; i < outputBuffer.getNumChannels(); i++) {
            std::memmove(outputBuffer.getWritePointer(i),
                         outputBuffer.getWritePointer(i) +
                             ioBlock.getNumSamples(),
                         sizeof(SampleType) * outputBufferSamples);
          }
        }

        samplesOutput = ioBlock.getNumSamples();
      }

      // 3) If the input buffer is large enough, process!
      int samplesProcessed = 0;
      int inputSamplesConsumed = 0;
      juce::dsp::AudioBlock<SampleType> inputBlock(inputBuffer);
      for (int i = 0; i < inputBufferSamples; i += blockSize) {
        int samplesAvailable = std::min(blockSize, inputBufferSamples - i);
        if (samplesAvailable < blockSize)
          break;

        juce::dsp::AudioBlock<SampleType> subBlock =
            inputBlock.getSubBlock(i, blockSize);
        juce::dsp::ProcessContextReplacing<SampleType> subContext(subBlock);
        int samplesProcessedThisBlock = plugin.process(subContext);
        inputSamplesConsumed += blockSize;

        if (samplesProcessedThisBlock > 0) {
          // Move the output to the left side of the buffer:
          inputBlock.move(i + blockSize - samplesProcessedThisBlock,
                          samplesProcessed, samplesProcessedThisBlock);
        }

        samplesProcessed += samplesProcessedThisBlock;
      }

      // Copy the newly-processed data into the output buffer:
      if (outputBuffer.getNumSamples() <
          outputBufferSamples + samplesProcessed) {
        throw std::runtime_error("Output buffer overflow! This is an internal "
                                 "Pedalboard error and should be reported.");
      }
      inputBlock.copyTo(outputBuffer, 0, outputBufferSamples, samplesProcessed);
      outputBufferSamples += samplesProcessed;

      // ... and move the remaining input data to the left of the input buffer:
      inputBlock.move(inputSamplesConsumed, 0,
                      inputBufferSamples - inputSamplesConsumed);
      inputBufferSamples -= inputSamplesConsumed;

      // ... and try to output the remaining output buffer contents if we now
      // have enough:
      if (samplesOutput == 0 &&
          outputBufferSamples >= ioBlock.getNumSamples()) {
        ioBlock.copyFrom(outputBuffer, 0, 0, ioBlock.getNumSamples());
        outputBufferSamples -= ioBlock.getNumSamples();

        // Move the remainder of the output buffer to the left:
        if (outputBufferSamples > 0) {
          for (int i = 0; i < outputBuffer.getNumChannels(); i++) {
            std::memmove(outputBuffer.getWritePointer(i),
                         outputBuffer.getWritePointer(i) +
                             ioBlock.getNumSamples(),
                         sizeof(SampleType) * outputBufferSamples);
          }
        }

        samplesOutput = ioBlock.getNumSamples();
      }

      samplesProcessed += samplesOutput;
      return samplesOutput;
    }
  }

  virtual void reset() {
    inputBufferSamples = 0;
    outputBufferSamples = 0;

    inStreamLatency = 0;
    samplesProcessed = 0;
    lastSpec = {0};
    plugin.reset();

    inputBuffer.clear();
    outputBuffer.clear();
  }

  T &getNestedPlugin() { return plugin; }

  void setFixedBlockSize(int newBlockSize) {
    blockSize = newBlockSize;
    reset();
  }

  int getFixedBlockSize() const { return blockSize; }

private:
  T plugin;
  unsigned int blockSize = DefaultBlockSize;
  int inStreamLatency = 0;

  juce::AudioBuffer<SampleType> inputBuffer;
  unsigned int inputBufferSamples = 0;

  juce::AudioBuffer<SampleType> outputBuffer;
  unsigned int outputBufferSamples = 0;

  unsigned int samplesProcessed = 0;
};

// TODO: Add plugin wrappers to make mono plugins stereo (and/or multichannel),
// or to mixdown to mono.

/**
 * A test plugin used to verify the behaviour of the FixedBlockSize wrapper.
 */
class ExpectsFixedBlockSize : public AddLatency {
public:
  virtual ~ExpectsFixedBlockSize(){};

  virtual void prepare(const juce::dsp::ProcessSpec &spec) {
    if (spec.maximumBlockSize != expectedBlockSize) {
      throw std::runtime_error("Expected maximum block size of exactly " +
                               std::to_string(expectedBlockSize) + "!");
    }
    AddLatency::prepare(spec);
    this->getDSP().setMaximumDelayInSamples(10);
    this->getDSP().setDelay(10);
  }

  virtual int
  process(const juce::dsp::ProcessContextReplacing<float> &context) {
    if (context.getInputBlock().getNumSamples() != expectedBlockSize) {
      throw std::runtime_error("Expected maximum block size of exactly " +
                               std::to_string(expectedBlockSize) + "!");
    }
    return AddLatency::process(context);
  }

  virtual void reset() { AddLatency::reset(); }

  void setExpectedBlockSize(int newExpectedBlockSize) {
    expectedBlockSize = newExpectedBlockSize;
  }

private:
  int expectedBlockSize = 0;
};

class FixedSizeBlockTestPlugin : public FixedBlockSize<ExpectsFixedBlockSize> {
public:
  void setExpectedBlockSize(int newExpectedBlockSize) {
    setFixedBlockSize(newExpectedBlockSize);
    getNestedPlugin().setExpectedBlockSize(newExpectedBlockSize);
  }

  int getExpectedBlockSize() const { return getFixedBlockSize(); }

private:
  int expectedBlockSize = 0;
};

inline void init_fixed_size_block_test_plugin(py::module &m) {
  py::class_<FixedSizeBlockTestPlugin, Plugin,
             std::shared_ptr<FixedSizeBlockTestPlugin>>(
      m, "FixedSizeBlockTestPlugin")
      .def(py::init([](int expectedBlockSize) {
             auto plugin = new FixedSizeBlockTestPlugin();
             plugin->setExpectedBlockSize(expectedBlockSize);
             return plugin;
           }),
           py::arg("expected_block_size") = 160)
      .def("__repr__", [](const FixedSizeBlockTestPlugin &plugin) {
        std::ostringstream ss;
        ss << "<pedalboard.FixedSizeBlockTestPlugin";
        ss << " expected_block_size=" << plugin.getExpectedBlockSize();
        ss << " at " << &plugin;
        ss << ">";
        return ss.str();
      });
}

} // namespace Pedalboard