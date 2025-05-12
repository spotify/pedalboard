/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/
#pragma once

#ifdef JUCE_MODULE_AVAILABLE_juce_gui_basics
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#ifdef None
#undef None // Defined in X11/X.h, clashes with struct None imported below
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_gui_extra
#include <juce_gui_extra/juce_gui_extra.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_basics
#include <juce_audio_basics/juce_audio_basics.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_formats
#include <juce_audio_formats/juce_audio_formats.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_processors
#include <juce_audio_processors/juce_audio_processors.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_core
#include <juce_core/juce_core.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_data_structures
#include <juce_data_structures/juce_data_structures.h>
#endif

#ifdef JUCE_MODULE_AVAILABLE_juce_dsp
#include <juce_dsp/juce_dsp.h>
#endif

namespace juce {

/**
    Implements a plugin format for VST3s.

    @tags{Audio}
*/
class PatchedVST3PluginFormat : public AudioPluginFormat {
public:
  /** Constructor */
  PatchedVST3PluginFormat();

  /** Destructor */
  ~PatchedVST3PluginFormat() override;

  //==============================================================================
#ifndef DOXYGEN
  /** Attempts to reload a VST3 plugin's state from some preset file data.

      @see VSTPluginFormat::loadFromFXBFile
  */
  [[deprecated("Instead of using this function, use "
               "AudioPluginInstance::getExtensions() "
               "to visit the ExtensionsVisitor::VST3 struct for the instance, "
               "if it exists. "
               "Then, call ExtensionsVisitor::VST3::setPreset() to set the "
               "state using the "
               "contents of a vstpreset file.")]] static bool
  setStateFromVSTPresetFile(AudioPluginInstance *, const MemoryBlock &);
#endif

  //==============================================================================
  static String getFormatName() { return "VST3"; }
  String getName() const override { return getFormatName(); }
  bool canScanForPlugins() const override { return true; }
  bool isTrivialToScan() const override { return false; }

  void findAllTypesForFile(OwnedArray<PluginDescription> &,
                           const String &fileOrIdentifier) override;
  bool fileMightContainThisPluginType(const String &fileOrIdentifier) override;
  String getNameOfPluginFromIdentifier(const String &fileOrIdentifier) override;
  bool pluginNeedsRescanning(const PluginDescription &) override;
  StringArray searchPathsForPlugins(const FileSearchPath &, bool recursive,
                                    bool) override;
  bool doesPluginStillExist(const PluginDescription &) override;
  FileSearchPath getDefaultLocationsToSearch() override;

private:
  //==============================================================================
  void createPluginInstance(const PluginDescription &, double initialSampleRate,
                            int initialBufferSize,
                            PluginCreationCallback) override;
  bool requiresUnblockedMessageThreadDuringCreation(
      const PluginDescription &) const override;
  void recursiveFileSearch(StringArray &, const File &, bool recursive);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchedVST3PluginFormat)
};

} // namespace juce
