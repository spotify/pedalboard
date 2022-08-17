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

#include <mutex>
#include <optional>

#include "JuceHeader.h"
#if JUCE_LINUX
#include <sys/utsname.h>
#endif

#include "AudioUnitParser.h"
#include "Plugin.h"
#include <pybind11/stl.h>

#if JUCE_MAC
#include <AudioToolbox/AudioUnitUtilities.h>
#endif

namespace Pedalboard {

// JUCE external plugins use some global state; here we lock that state
// to play nicely with the Python interpreter.
static std::mutex EXTERNAL_PLUGIN_MUTEX;
static int NUM_ACTIVE_EXTERNAL_PLUGINS = 0;

static const std::string AUDIO_UNIT_NOT_INSTALLED_ERROR =
    "macOS requires plugin files to be moved to "
    "/Library/Audio/Plug-Ins/Components/ or "
    "~/Library/Audio/Plug-Ins/Components/ before loading.";

inline std::vector<std::string> findInstalledVSTPluginPaths() {
  // Ensure we have a MessageManager, which is required by the VST wrapper
  // Without this, we get an assert(false) from JUCE at runtime
  juce::MessageManager::getInstance();
  juce::VST3PluginFormat format;
  std::vector<std::string> pluginPaths;
  for (juce::String pluginIdentifier : format.searchPathsForPlugins(
           format.getDefaultLocationsToSearch(), true, false)) {
    pluginPaths.push_back(
        format.getNameOfPluginFromIdentifier(pluginIdentifier).toStdString());
  }
  return pluginPaths;
}

/**
 * The VST3 and Audio Unit format managers differ in how they look up plugins
 * that are already installed on the current machine. This approach allows us to
 * return file paths from both.
 */
#if JUCE_PLUGINHOST_AU && JUCE_MAC
class AudioUnitPathFinder {
public:
  static std::vector<std::string> findInstalledAudioUnitPaths() {
    // Ensure we have a MessageManager, which is required by the VST wrapper
    // Without this, we get an assert(false) from JUCE at runtime
    juce::MessageManager::getInstance();

    juce::AudioUnitPluginFormat format;

    std::vector<std::string> pluginPaths;
    for (juce::String pluginPath : searchPathsForPlugins(
             juce::FileSearchPath(
                 "/Library/Audio/Plug-Ins/Components;~/Library/"
                 "Audio/Plug-Ins/Components"),
             true, format)) {
      pluginPaths.push_back(pluginPath.toStdString());
    }
    return pluginPaths;
  }

private:
  static juce::StringArray
  searchPathsForPlugins(const juce::FileSearchPath &directoriesToSearch,
                        const bool recursive,
                        juce::AudioUnitPluginFormat &format) {
    juce::StringArray results;

    for (int i = 0; i < directoriesToSearch.getNumPaths(); ++i) {
      recursiveFileSearch(results, directoriesToSearch[i], recursive, format);
    }

    return results;
  }

  static void recursiveFileSearch(juce::StringArray &results,
                                  const juce::File &directory,
                                  const bool recursive,
                                  juce::AudioUnitPluginFormat &format) {
    for (const auto &iter : juce::RangedDirectoryIterator(
             directory, false, "*", juce::File::findFilesAndDirectories)) {
      auto f = iter.getFile();
      bool isPlugin = false;

      if (format.fileMightContainThisPluginType(f.getFullPathName())) {
        isPlugin = true;
        results.add(f.getFullPathName());
      }

      if (recursive && (!isPlugin) && f.isDirectory())
        recursiveFileSearch(results, f, true, format);
    }
  }
};
#endif

static bool audioUnitIsInstalled(const juce::String audioUnitFilePath) {
  return audioUnitFilePath.contains("/Library/Audio/Plug-Ins/Components/");
}

template <typename ExternalPluginType>
static std::vector<std::string> getPluginNamesForFile(std::string filename) {
  juce::MessageManager::getInstance();

  ExternalPluginType format;
  juce::OwnedArray<juce::PluginDescription> typesFound;

  std::string errorMessage = "Unable to scan plugin " + filename +
                             ": unsupported plugin format or scan failure.";

#if JUCE_PLUGINHOST_AU && JUCE_MAC
  if constexpr (std::is_same<ExternalPluginType,
                             juce::AudioUnitPluginFormat>::value) {
    auto identifiers = getAudioUnitIdentifiersFromFile(filename);
    // For each plugin in the identified bundle, scan using its AU identifier:
    for (int i = 0; i < identifiers.size(); i++) {
      format.findAllTypesForFile(typesFound, identifiers[i]);
    }

    if (typesFound.isEmpty() && !audioUnitIsInstalled(filename)) {
      errorMessage += " " + AUDIO_UNIT_NOT_INSTALLED_ERROR;
    }
  } else {
    format.findAllTypesForFile(typesFound, filename);
  }
#else
  format.findAllTypesForFile(typesFound, filename);
#endif

  if (typesFound.isEmpty()) {
    throw pybind11::import_error(errorMessage);
  }

  std::vector<std::string> pluginNames;
  for (int i = 0; i < typesFound.size(); i++) {
    pluginNames.push_back(typesFound[i]->name.toStdString());
  }
  return pluginNames;
}

class StandalonePluginWindow : public juce::DocumentWindow {
public:
  StandalonePluginWindow(juce::AudioProcessor &processor)
      : DocumentWindow("Pedalboard",
                       juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                           juce::ResizableWindow::backgroundColourId),
                       juce::DocumentWindow::minimiseButton |
                           juce::DocumentWindow::closeButton),
        processor(processor) {
    setUsingNativeTitleBar(true);

    if (processor.hasEditor()) {
      if (auto *editor = processor.createEditorIfNeeded()) {
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
      } else {
        throw std::runtime_error("Failed to create plugin editor UI.");
      }
    } else {
      throw std::runtime_error("Plugin has no available editor UI.");
    }
  }

  /**
   * Open a native window to show a given AudioProcessor's editor UI,
   * pumping the juce::MessageManager run loop as necessary to service
   * UI events.
   */
  static void openWindowAndWait(juce::AudioProcessor &processor) {
    bool shouldThrowErrorAlreadySet = false;

    JUCE_AUTORELEASEPOOL {
      StandalonePluginWindow window(processor);
      window.show();

      // Run in a tight loop so that we don't have to call ->stopDispatchLoop(),
      // which causes the MessageManager to become unusable in the future.
      // The window can be closed by sending a KeyboardInterrupt or closing
      // the window in the UI.
      while (window.isVisible()) {
        if (PyErr_CheckSignals() != 0) {
          window.closeButtonPressed();
          shouldThrowErrorAlreadySet = true;
          break;
        }

        {
          // Release the GIL to allow other Python threads to run in the
          // background while we the UI is running:
          py::gil_scoped_release release;
          juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        }
      }
    }

    // Once the Autorelease pool has been drained, pump the dispatch loop one
    // more time to process any window close events:
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);

    if (shouldThrowErrorAlreadySet) {
      throw py::error_already_set();
    }
  }

  void closeButtonPressed() override { setVisible(false); }

  ~StandalonePluginWindow() override { clearContentComponent(); }

  void show() {
    setVisible(true);
    toFront(true);
    juce::Process::makeForegroundProcess();
  }

private:
  juce::AudioProcessor &processor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandalonePluginWindow)
};

enum class ExternalPluginReloadType {
  /**
   * Unknown: we need to determine the reload type.
   */
  Unknown,

  /**
   * Most plugins are of this type: calling .reset() on them will clear their
   * internal state. This is quick and easy: to start processing a new buffer,
   * all we need to do is call .reset() and optionally prepareToPlay().
   */
  ClearsAudioOnReset,

  /**
   * This plugin type is a bit more of a pain to deal with; it could be argued
   * that plugins that don't clear their internal buffers when reset() is called
   * are buggy. To start processing a new buffer, we'll have to find another way
   * to clear the buffer, usually by reloading the plugin from scratch and
   * persisting its parameters somehow.
   */
  PersistsAudioOnReset,
};

template <typename ExternalPluginType> class ExternalPlugin : public Plugin {
public:
  ExternalPlugin(std::string &_pathToPluginFile,
                 std::optional<std::string> pluginName = {})
      : pathToPluginFile(_pathToPluginFile) {
    py::gil_scoped_release release;
    // Ensure we have a MessageManager, which is required by the VST wrapper
    // Without this, we get an assert(false) from JUCE at runtime
    juce::MessageManager::getInstance();

    juce::OwnedArray<juce::PluginDescription> typesFound;
    ExternalPluginType format;

    pluginFormatManager.addDefaultFormats();

    auto pluginFileStripped =
        pathToPluginFile.trimCharactersAtEnd(juce::File::getSeparatorString());
    auto fileExists =
        juce::File::createFileWithoutCheckingPath(pluginFileStripped).exists();

    if (!fileExists) {
      throw pybind11::import_error("Unable to load plugin " +
                                   pathToPluginFile.toStdString() +
                                   ": plugin file not found.");
    }

#if JUCE_PLUGINHOST_AU && JUCE_MAC
    if constexpr (std::is_same<ExternalPluginType,
                               juce::AudioUnitPluginFormat>::value) {
      auto identifiers = getAudioUnitIdentifiersFromFile(pluginFileStripped);
      // For each plugin in the identified bundle, scan using its AU identifier:
      for (int i = 0; i < identifiers.size(); i++) {
        format.findAllTypesForFile(typesFound, identifiers[i]);
      }
    } else {
      format.findAllTypesForFile(typesFound, pluginFileStripped);
    }
#else
    format.findAllTypesForFile(typesFound, pluginFileStripped);
#endif

    if (!typesFound.isEmpty()) {
      if (typesFound.size() == 1) {
        foundPluginDescription = *typesFound[0];
      } else if (typesFound.size() > 1) {
        std::string errorMessage =
            "Plugin file " + pathToPluginFile.toStdString() + " contains " +
            std::to_string(typesFound.size()) + " plugins";

        // Use the provided plugin name to disambiguate:
        if (pluginName) {
          for (int i = 0; i < typesFound.size(); i++) {
            if (typesFound[i]->name.toStdString() == *pluginName) {
              foundPluginDescription = *typesFound[i];
              break;
            }
          }

          if (foundPluginDescription.name.isEmpty()) {
            errorMessage += ", and the provided plugin_name \"" + *pluginName +
                            "\" matched no plugins. ";
          }
        } else {
          errorMessage += ". ";
        }

        if (foundPluginDescription.name.isEmpty()) {
          juce::StringArray pluginNames;
          for (int i = 0; i < typesFound.size(); i++) {
            pluginNames.add(typesFound[i]->name);
          }

          errorMessage +=
              ("To open a specific plugin within this file, pass a "
               "\"plugin_name\" parameter with one of the following "
               "values:\n\t\"" +
               pluginNames.joinIntoString("\"\n\t\"").toStdString() + "\"");
          throw std::domain_error(errorMessage);
        }
      }

      reinstantiatePlugin();
    } else {
      std::string errorMessage = "Unable to load plugin " +
                                 pathToPluginFile.toStdString() +
                                 ": unsupported plugin format or load failure.";
#if JUCE_LINUX
      auto machineName = []() -> juce::String {
        struct utsname unameData;
        auto res = uname(&unameData);

        if (res != 0)
          return {};

        return unameData.machine;
      }();

      juce::File pluginBundle(pluginFileStripped);

      auto pathToSharedObjectFile =
          pluginBundle.getChildFile("Contents")
              .getChildFile(machineName + "-linux")
              .getChildFile(pluginBundle.getFileNameWithoutExtension() + ".so");

      errorMessage += " Plugin files or shared library dependencies may be "
                      "missing. (Try running `ldd \"" +
                      pathToSharedObjectFile.getFullPathName().toStdString() +
                      "\"` to see which dependencies might be missing.).";
#elif JUCE_PLUGINHOST_AU && JUCE_MAC
      if constexpr (std::is_same<ExternalPluginType,
                                 juce::AudioUnitPluginFormat>::value) {
        if (!audioUnitIsInstalled(pathToPluginFile)) {
          errorMessage += " " + AUDIO_UNIT_NOT_INSTALLED_ERROR;
        }
      }
#endif

      throw pybind11::import_error(errorMessage);
    }
  }

  ~ExternalPlugin() {
    {
      std::lock_guard<std::mutex> lock(EXTERNAL_PLUGIN_MUTEX);
      pluginInstance.reset();
      NUM_ACTIVE_EXTERNAL_PLUGINS--;

      if (NUM_ACTIVE_EXTERNAL_PLUGINS == 0) {
        juce::DeletedAtShutdown::deleteAll();
        juce::MessageManager::deleteInstance();
      }
    }
  }

  struct PresetVisitor : public juce::ExtensionsVisitor {
    const std::string presetFilePath;

    PresetVisitor(const std::string presetFilePath)
        : presetFilePath(presetFilePath) {}

    void visitVST3Client(
        const juce::ExtensionsVisitor::VST3Client &client) override {
      juce::File presetFile(presetFilePath);
      juce::MemoryBlock presetData;

      if (!presetFile.loadFileAsData(presetData)) {
        throw std::runtime_error("Failed to read preset file: " +
                                 presetFilePath);
      }

      if (!client.setPreset(presetData)) {
        throw std::runtime_error(
            "Plugin returned an error when loading data from preset file: " +
            presetFilePath);
      }
    }
  };

  void loadPresetData(std::string presetFilePath) {
    PresetVisitor visitor{presetFilePath};
    pluginInstance->getExtensions(visitor);
  }

  void reinstantiatePlugin() {
    // If we have an existing plugin, save its state and reload its state
    // later:
    juce::MemoryBlock savedState;
    std::map<int, float> currentParameters;

    if (pluginInstance) {
      pluginInstance->getStateInformation(savedState);

      for (auto *parameter : pluginInstance->getParameters()) {
        currentParameters[parameter->getParameterIndex()] =
            parameter->getValue();
      }

      {
        std::lock_guard<std::mutex> lock(EXTERNAL_PLUGIN_MUTEX);
        // Delete the plugin instance itself:
        pluginInstance.reset();
        NUM_ACTIVE_EXTERNAL_PLUGINS--;
      }
    }

    juce::String loadError;
    {
      std::lock_guard<std::mutex> lock(EXTERNAL_PLUGIN_MUTEX);
      pluginInstance = pluginFormatManager.createPluginInstance(
          foundPluginDescription, ExternalLoadSampleRate,
          ExternalLoadMaximumBlockSize, loadError);

      if (!pluginInstance) {
        throw pybind11::import_error("Unable to load plugin " +
                                     pathToPluginFile.toStdString() + ": " +
                                     loadError.toStdString());
      }

      pluginInstance->enableAllBuses();

      auto mainInputBus = pluginInstance->getBus(true, 0);
      auto mainOutputBus = pluginInstance->getBus(false, 0);

      if (!mainOutputBus) {
        auto exception = std::invalid_argument(
            "Plugin '" + pluginInstance->getName().toStdString() +
            "' does not produce audio output.");
        pluginInstance.reset();
        throw exception;
      }

      if (reloadType == ExternalPluginReloadType::Unknown) {
        reloadType = detectReloadType();
        if (reloadType == ExternalPluginReloadType::PersistsAudioOnReset) {
          // Reload again, as we just passed audio into a plugin that
          // we know doesn't reset itself cleanly!
          pluginInstance = pluginFormatManager.createPluginInstance(
              foundPluginDescription, ExternalLoadSampleRate,
              ExternalLoadMaximumBlockSize, loadError);

          if (!pluginInstance) {
            throw pybind11::import_error("Unable to load plugin " +
                                         pathToPluginFile.toStdString() + ": " +
                                         loadError.toStdString());
          }
        }
      }

      NUM_ACTIVE_EXTERNAL_PLUGINS++;
    }

    pluginInstance->setStateInformation(savedState.getData(),
                                        savedState.getSize());

    // Set all of the parameters twice: we may have meta-parameters that
    // change the validity of other `setValue` calls. (i.e.: param1 can't be
    // set until param2 is set.)
    for (int i = 0; i < 2; i++) {
      for (auto *parameter : pluginInstance->getParameters()) {
        if (currentParameters.count(parameter->getParameterIndex()) > 0) {
          parameter->setValue(
              currentParameters[parameter->getParameterIndex()]);
        }
      }
    }

    if (lastSpec.numChannels != 0) {
      const juce::dsp::ProcessSpec _lastSpec = lastSpec;
      // Invalidate lastSpec to force us to update the plugin state:
      lastSpec.numChannels = 0;
      prepare(_lastSpec);
    }

    pluginInstance->reset();
  }

  void setNumChannels(int numChannels) {
    if (!pluginInstance)
      return;

    if (numChannels == 0)
      return;

    auto mainInputBus = pluginInstance->getBus(true, 0);
    auto mainOutputBus = pluginInstance->getBus(false, 0);

    if (!mainInputBus) {
      throw std::invalid_argument(
          "Plugin '" + pluginInstance->getName().toStdString() +
          "' does not accept audio input. It may be an instrument plug-in "
          "and not an audio effect processor.");
    }

    // Try to disable all non-main input buses if possible:
    for (int i = 1; i < pluginInstance->getBusCount(true); i++) {
      auto *bus = pluginInstance->getBus(true, i);
      if (bus->isNumberOfChannelsSupported(0))
        bus->enable(false);
    }

    // ...and all non-main output buses too:
    for (int i = 1; i < pluginInstance->getBusCount(false); i++) {
      auto *bus = pluginInstance->getBus(false, i);
      if (bus->isNumberOfChannelsSupported(0))
        bus->enable(false);
    }

    if (mainInputBus->getNumberOfChannels() == numChannels &&
        mainOutputBus->getNumberOfChannels() == numChannels) {
      return;
    }

    // Cache these values in case the plugin fails to update:
    auto previousInputChannelCount = mainInputBus->getNumberOfChannels();
    auto previousOutputChannelCount = mainOutputBus->getNumberOfChannels();

    // Try to change the input and output bus channel counts...
    mainInputBus->setNumberOfChannels(numChannels);
    mainOutputBus->setNumberOfChannels(numChannels);

    // If, post-reload, we still can't use the right number of channels, let's
    // conclude the plugin doesn't allow this channel count.
    if (mainInputBus->getNumberOfChannels() != numChannels ||
        mainOutputBus->getNumberOfChannels() != numChannels) {

      // Reset the bus configuration to what it was before, so we don't
      // leave one of the buses smaller than the other:
      mainInputBus->setNumberOfChannels(previousInputChannelCount);
      mainOutputBus->setNumberOfChannels(previousOutputChannelCount);

      throw std::invalid_argument(
          "Plugin '" + pluginInstance->getName().toStdString() +
          "' does not support " + std::to_string(numChannels) +
          "-channel input and output. (Main bus currently expects " +
          std::to_string(mainInputBus->getNumberOfChannels()) +
          " input channels and " +
          std::to_string(mainOutputBus->getNumberOfChannels()) +
          " output channels.)");
    }
  }

  const juce::String getName() const {
    return pluginInstance ? pluginInstance->getName() : "<unknown>";
  }

  const int getNumChannels() const {
    // Input and output channel counts should match.
    if (!pluginInstance) {
      return 0;
    }

    auto mainInputBus = pluginInstance->getBus(true, 0);
    if (!mainInputBus) {
      return 0;
    }

    return mainInputBus->getNumberOfChannels();
  }

  /**
   * Send some audio through the plugin to detect if the ->reset() call
   * actually resets internal buffers. This determines how quickly we
   * can reset the plugin and is only called on instantiation.
   */
  ExternalPluginReloadType detectReloadType() {
    const int numInputChannels = pluginInstance->getMainBusNumInputChannels();
    const int bufferSize = 512;
    const float sampleRate = 44100.0f;

    if (numInputChannels == 0) {
      return ExternalPluginReloadType::Unknown;
    }

    // Set input and output busses/channels appropriately:
    setNumChannels(numInputChannels);
    pluginInstance->setNonRealtime(true);
    pluginInstance->prepareToPlay(sampleRate, bufferSize);

    // Send in a buffer full of silence to get a baseline noise level:
    juce::AudioBuffer<float> audioBuffer(numInputChannels, bufferSize);
    juce::MidiBuffer emptyMidiBuffer;

    // Process the silent buffer a couple of times to give the plugin time to
    // "warm up"
    for (int i = 0; i < 5; i++) {
      audioBuffer.clear();
      {
        juce::dsp::AudioBlock<float> block(audioBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        process(context);
      }
    }

    // Measure the noise floor of the plugin:
    auto noiseFloor = audioBuffer.getMagnitude(0, bufferSize);

    // Reset:
    pluginInstance->releaseResources();
    pluginInstance->setNonRealtime(true);
    pluginInstance->prepareToPlay(sampleRate, bufferSize);

    juce::Random random;

    // Send noise into the plugin:
    for (int i = 0; i < 5; i++) {
      for (auto i = 0; i < bufferSize; i++) {
        for (int c = 0; c < numInputChannels; c++) {
          audioBuffer.setSample(c, i, (random.nextFloat() * 2.0f) - 1.0f);
        }
      }
      juce::dsp::AudioBlock<float> block(audioBuffer);
      juce::dsp::ProcessContextReplacing<float> context(block);
      process(context);
    }

    auto signalVolume = audioBuffer.getMagnitude(0, bufferSize);

    // Reset again, and send in silence:
    pluginInstance->releaseResources();
    pluginInstance->setNonRealtime(true);
    pluginInstance->prepareToPlay(sampleRate, bufferSize);
    audioBuffer.clear();
    {
      juce::dsp::AudioBlock<float> block(audioBuffer);
      juce::dsp::ProcessContextReplacing<float> context(block);
      process(context);
    }

    auto magnitudeOfSilentBuffer = audioBuffer.getMagnitude(0, bufferSize);

    // If the silent buffer we passed in post-reset is noticeably louder
    // than the first buffer we passed in, this plugin probably persists
    // internal state across calls to releaseResources().
    bool pluginPersistsAudioOnReset = magnitudeOfSilentBuffer > noiseFloor * 5;

    if (pluginPersistsAudioOnReset) {
      return ExternalPluginReloadType::PersistsAudioOnReset;
    } else {
      return ExternalPluginReloadType::ClearsAudioOnReset;
    }
  }

  /**
   * reset() is only called if reset=True is passed.
   */
  void reset() noexcept override {
    if (pluginInstance) {
      switch (reloadType) {
      case ExternalPluginReloadType::ClearsAudioOnReset:
        pluginInstance->reset();
        pluginInstance->releaseResources();
        break;

      case ExternalPluginReloadType::Unknown:
      case ExternalPluginReloadType::PersistsAudioOnReset:
        pluginInstance->releaseResources();
        reinstantiatePlugin();
        break;
      default:
        throw std::runtime_error("Plugin reload type is an invalid value (" +
                                 std::to_string((int)reloadType) +
                                 ") - this likely indicates a programming "
                                 "error or memory corruption.");
      }

      // Force prepare() to be called again later by invalidating lastSpec:
      lastSpec.maximumBlockSize = 0;
      samplesProvided = 0;
    }
  }

  /**
   * prepare() is called on every render call, regardless of if the plugin has
   * been reset.
   */
  void prepare(const juce::dsp::ProcessSpec &spec) override {
    if (!pluginInstance) {
      return;
    }

    if (lastSpec.sampleRate != spec.sampleRate ||
        lastSpec.maximumBlockSize < spec.maximumBlockSize ||
        lastSpec.numChannels != spec.numChannels) {

      // Changing the number of channels requires releaseResources to be
      // called:
      if (lastSpec.numChannels != spec.numChannels) {
        pluginInstance->releaseResources();
        setNumChannels(spec.numChannels);
      }

      pluginInstance->setNonRealtime(true);
      pluginInstance->prepareToPlay(spec.sampleRate, spec.maximumBlockSize);

      lastSpec = spec;
    }
  }

  int process(
      const juce::dsp::ProcessContextReplacing<float> &context) override {

    if (pluginInstance) {
      juce::MidiBuffer emptyMidiBuffer;
      if (context.usesSeparateInputAndOutputBlocks()) {
        throw std::runtime_error("Not implemented yet - "
                                 "no support for using separate "
                                 "input and output blocks.");
      }

      juce::dsp::AudioBlock<float> &outputBlock = context.getOutputBlock();

      // This should already be true, as prepare() should have been called
      // before this!
      if ((size_t)pluginInstance->getMainBusNumInputChannels() !=
          outputBlock.getNumChannels()) {
        throw std::invalid_argument(
            "Plugin '" + pluginInstance->getName().toStdString() +
            "' was instantiated with " +
            std::to_string(pluginInstance->getMainBusNumInputChannels()) +
            "-channel input, but data provided was " +
            std::to_string(outputBlock.getNumChannels()) + "-channel.");
      }

      if ((size_t)pluginInstance->getMainBusNumOutputChannels() <
          outputBlock.getNumChannels()) {
        throw std::invalid_argument(
            "Plugin '" + pluginInstance->getName().toStdString() +
            "' produces " +
            std::to_string(pluginInstance->getMainBusNumOutputChannels()) +
            "-channel output, but data provided was " +
            std::to_string(outputBlock.getNumChannels()) +
            "-channel. (The number of channels returned must match the "
            "number of channels passed in.)");
      }

      std::vector<float *> channelPointers(
          pluginInstance->getTotalNumInputChannels());

      for (size_t i = 0; i < outputBlock.getNumChannels(); i++) {
        channelPointers[i] = outputBlock.getChannelPointer(i);
      }

      // Depending on the bus layout, we may have to pass extra buffers to the
      // plugin that we don't use. Use vector here to ensure the memory is
      // freed via RAII.
      std::vector<std::vector<float>> dummyChannels;
      for (size_t i = outputBlock.getNumChannels(); i < channelPointers.size();
           i++) {
        std::vector<float> dummyChannel(outputBlock.getNumSamples());
        channelPointers[i] = dummyChannel.data();
        dummyChannels.push_back(dummyChannel);
      }

      // Create an audio buffer that doesn't actually allocate anything, but
      // just points to the data in the ProcessContext.
      juce::AudioBuffer<float> audioBuffer(channelPointers.data(),
                                           channelPointers.size(),
                                           outputBlock.getNumSamples());

      pluginInstance->processBlock(audioBuffer, emptyMidiBuffer);
      samplesProvided += outputBlock.getNumSamples();

      // To compensate for any latency added by the plugin,
      // only tell Pedalboard to use the last _n_ samples.
      long usableSamplesProduced =
          std::max(0L, samplesProvided - pluginInstance->getLatencySamples());
      return static_cast<int>(
          std::min(usableSamplesProduced, (long)outputBlock.getNumSamples()));
    }

    return 0;
  }

  std::vector<juce::AudioProcessorParameter *> getParameters() const {
    std::vector<juce::AudioProcessorParameter *> parameters;
    for (auto *parameter : pluginInstance->getParameters()) {
      parameters.push_back(parameter);
    }
    return parameters;
  }

  juce::AudioProcessorParameter *getParameter(const std::string &name) const {
    for (auto *parameter : pluginInstance->getParameters()) {
      if (parameter->getName(512).toStdString() == name) {
        return parameter;
      }
    }
    return nullptr;
  }

  virtual int getLatencyHint() override {
    if (!pluginInstance)
      return 0;
    return pluginInstance->getLatencySamples();
  }

  void showEditor() {
    if (!pluginInstance) {
      throw std::runtime_error(
          "Editor cannot be shown - plugin not loaded. This is an internal "
          "Pedalboard error and should be reported.");
    }

    if (!juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
      throw std::runtime_error(
          "Editor cannot be shown - no visual display devices available.");
    }

    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
      throw std::runtime_error(
          "Plugin UI windows can only be shown from the main thread.");
    }

    StandalonePluginWindow::openWindowAndWait(*pluginInstance);
  }

private:
  constexpr static int ExternalLoadSampleRate = 44100,
                       ExternalLoadMaximumBlockSize = 8192;
  juce::String pathToPluginFile;
  juce::PluginDescription foundPluginDescription;
  juce::AudioPluginFormatManager pluginFormatManager;
  std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

  long samplesProvided = 0;

  ExternalPluginReloadType reloadType = ExternalPluginReloadType::Unknown;
};

inline void init_external_plugins(py::module &m) {
  py::class_<juce::AudioProcessorParameter>(
      m, "_AudioProcessorParameter",
      "An abstract base class for parameter objects that can be added to an "
      "AudioProcessor.")
      .def("__repr__",
           [](juce::AudioProcessorParameter &parameter) {
             std::ostringstream ss;
             ss << "<pedalboard.AudioProcessorParameter";
             ss << " name=\"" << parameter.getName(512).toStdString() << "\"";
             if (!parameter.getLabel().isEmpty())
               ss << " label=\"" << parameter.getLabel().toStdString() << "\"";
             if (parameter.isBoolean())
               ss << " boolean";
             if (parameter.isDiscrete())
               ss << " discrete";
             ss << " raw_value=" << parameter.getValue();
             ss << ">";
             return ss.str();
           })
      .def_property(
          "raw_value", &juce::AudioProcessorParameter::getValue,
          &juce::AudioProcessorParameter::setValue,
          "The internal value of this parameter. Convention is that this "
          "parameter should be between 0 and 1.0. This may or may not "
          "correspond with the value shown to the user.")
      .def_property_readonly(
          "default_raw_value", &juce::AudioProcessorParameter::getDefaultValue,
          "The default internal value of this parameter. Convention is that "
          "this parameter should be between 0 and 1.0. This may or may not "
          "correspond with the value shown to the user.")
      .def(
          "get_name",
          [](juce::AudioProcessorParameter &param, int length) {
            return param.getName(length).toStdString();
          },
          py::arg("maximum_string_length"),
          "Returns the name to display for this parameter, which is made to "
          "fit within the given string length")
      .def_property_readonly(
          "name",
          [](juce::AudioProcessorParameter &param) {
            return param.getName(512).toStdString();
          },
          "Returns the name to display for this parameter at its longest.")
      .def_property_readonly(
          "label",
          [](juce::AudioProcessorParameter &param) {
            return param.getLabel().toStdString();
          },
          "Some parameters may be able to return a label string for their "
          "units. For example \"Hz\" or \"%\".")
      .def_property_readonly(
          "num_steps", &juce::AudioProcessorParameter::getNumSteps,
          "Returns the number of steps that this parameter's range should be "
          "quantised into. See also: is_discrete, is_boolean.")
      .def_property_readonly("is_discrete",
                             &juce::AudioProcessorParameter::isDiscrete,
                             "Returns whether the parameter uses discrete "
                             "values, based on the result of getNumSteps, or "
                             "allows the host to select values continuously.")
      .def_property_readonly(
          "is_boolean", &juce::AudioProcessorParameter::isBoolean,
          "Returns whether the parameter represents a boolean switch, "
          "typically with \"On\" and \"Off\" states.")
      .def(
          "get_text_for_raw_value",
          [](juce::AudioProcessorParameter &param, float value,
             int maximumStringLength) {
            return param.getText(value, maximumStringLength).toStdString();
          },
          py::arg("raw_value"), py::arg("maximum_string_length") = 512,
          "Returns a textual version of the supplied normalised parameter "
          "value.")
      .def(
          "get_raw_value_for_text",
          [](juce::AudioProcessorParameter &param, std::string &text) {
            return param.getValueForText(text);
          },
          py::arg("string_value"),
          "Returns the raw value of the supplied text. Plugins may handle "
          "errors however they see fit, but will likely not raise "
          "exceptions.")
      .def_property_readonly(
          "is_orientation_inverted",
          &juce::AudioProcessorParameter::isOrientationInverted,
          "If true, this parameter operates in the reverse direction. (Not "
          "all plugin formats will actually use this information).")
      .def_property_readonly("is_automatable",
                             &juce::AudioProcessorParameter::isAutomatable,
                             "Returns true if this parameter can be automated.")
      .def_property_readonly("is_automatable",
                             &juce::AudioProcessorParameter::isAutomatable,
                             "Returns true if this parameter can be "
                             "automated (i.e.: scheduled to "
                             "change over time, in real-time, in a DAW).")
      .def_property_readonly(
          "is_meta_parameter", &juce::AudioProcessorParameter::isMetaParameter,
          "A meta-parameter is a parameter that changes other parameters.")
      .def_property_readonly(
          "index", &juce::AudioProcessorParameter::getParameterIndex,
          "The index of this parameter in its plugin's parameter list.")
      .def_property_readonly(
          "string_value",
          [](juce::AudioProcessorParameter &param) {
            return param.getCurrentValueAsText().toStdString();
          },
          "Returns the current value of the parameter as a string.");

#if JUCE_PLUGINHOST_VST3 && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX)
  py::class_<ExternalPlugin<juce::VST3PluginFormat>, Plugin,
             std::shared_ptr<ExternalPlugin<juce::VST3PluginFormat>>>(
      m, "_VST3Plugin",
      "A wrapper around any Steinberg® VST3 audio effect plugin. Note that "
      "plugins must already support the operating system currently in use "
      "(i.e.: if you're running Linux but trying to open a VST that does not "
      "support Linux, this will fail).")
      .def(py::init([](std::string &pathToPluginFile,
                       std::optional<std::string> pluginName) {
             return std::make_unique<ExternalPlugin<juce::VST3PluginFormat>>(
                 pathToPluginFile, pluginName);
           }),
           py::arg("path_to_plugin_file"), py::arg("plugin_name") = py::none())
      .def("__repr__",
           [](ExternalPlugin<juce::VST3PluginFormat> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.VST3Plugin";
             ss << " \"" << plugin.getName() << "\"";
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def("load_preset",
           &ExternalPlugin<juce::VST3PluginFormat>::loadPresetData,
           "Load a VST3 preset file in .vstpreset format.",
           py::arg("preset_file_path"))
      .def_static(
          "get_plugin_names_for_file",
          [](std::string filename) {
            return getPluginNamesForFile<juce::VST3PluginFormat>(filename);
          },
          "Return a list of plugin names contained within a given VST3 "
          "plugin (i.e.: a \".vst3\"). If the provided file cannot be scanned, "
          "an ImportError will be raised.")
      .def_property_readonly_static(
          "installed_plugins",
          [](py::object /* cls */) { return findInstalledVSTPluginPaths(); },
          "Return a list of paths to VST3 plugins installed in the default "
          "location on this system. This list may not be exhaustive, and "
          "plugins in this list are not guaranteed to be compatible with "
          "Pedalboard.")
      .def_property_readonly(
          "name",
          [](ExternalPlugin<juce::VST3PluginFormat> &plugin) {
            return plugin.getName().toStdString();
          },
          "The name of this plugin.")
      .def_property_readonly(
          "_parameters", &ExternalPlugin<juce::VST3PluginFormat>::getParameters,
          py::return_value_policy::reference_internal)
      .def("_get_parameter",
           &ExternalPlugin<juce::VST3PluginFormat>::getParameter,
           py::return_value_policy::reference_internal)
      .def("show_editor", &ExternalPlugin<juce::VST3PluginFormat>::showEditor,
           "Show the UI of this plugin as a native window. This method will "
           "block until the window is closed or a KeyboardInterrupt is "
           "received.");
#endif

#if JUCE_PLUGINHOST_AU && JUCE_MAC
  py::class_<ExternalPlugin<juce::AudioUnitPluginFormat>, Plugin,
             std::shared_ptr<ExternalPlugin<juce::AudioUnitPluginFormat>>>(
      m, "_AudioUnitPlugin",
      "A wrapper around any Apple Audio Unit audio effect plugin. Only "
      "available on macOS.")
      .def(py::init([](std::string &pathToPluginFile,
                       std::optional<std::string> pluginName) {
             return std::make_unique<
                 ExternalPlugin<juce::AudioUnitPluginFormat>>(pathToPluginFile,
                                                              pluginName);
           }),
           py::arg("path_to_plugin_file"), py::arg("plugin_name") = py::none())
      .def("__repr__",
           [](const ExternalPlugin<juce::AudioUnitPluginFormat> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.AudioUnitPlugin";
             ss << " \"" << plugin.getName() << "\"";
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_static(
          "get_plugin_names_for_file",
          [](std::string filename) {
            return getPluginNamesForFile<juce::AudioUnitPluginFormat>(filename);
          },
          py::arg("filename"),
          "Return a list of plugin names contained within a given Audio Unit "
          "bundle (i.e.: a ``.component`` file). If the provided file cannot "
          "be "
          "scanned, an ``ImportError`` will be raised.\n\nNote that most Audio "
          "Units have a single plugin inside, but this method can be useful to "
          "determine if multiple plugins are present in one bundle, and if so, "
          "what their names are.")
      .def_property_readonly_static(
          "installed_plugins",
          [](py::object /* cls */) {
            return AudioUnitPathFinder::findInstalledAudioUnitPaths();
          },
          "Return a list of paths to Audio Units installed in the default "
          "location on this system. This list may not be exhaustive, and "
          "plugins in this list are not guaranteed to be compatible with "
          "Pedalboard.")
      .def_property_readonly(
          "name",
          [](ExternalPlugin<juce::AudioUnitPluginFormat> &plugin) {
            return plugin.getName().toStdString();
          },
          "The name of this plugin, as reported by the plugin itself.")
      .def_property_readonly(
          "_parameters",
          &ExternalPlugin<juce::AudioUnitPluginFormat>::getParameters,
          py::return_value_policy::reference_internal)
      .def("_get_parameter",
           &ExternalPlugin<juce::AudioUnitPluginFormat>::getParameter,
           py::return_value_policy::reference_internal)
      .def("show_editor",
           &ExternalPlugin<juce::AudioUnitPluginFormat>::showEditor,
           "Show the UI of this plugin as a native window. This method will "
           "block until the window is closed or a KeyboardInterrupt is "
           "received.");
#endif
}

} // namespace Pedalboard