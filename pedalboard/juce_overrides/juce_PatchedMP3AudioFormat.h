#pragma once
#include "../JuceHeader.h"
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

namespace juce {

//==============================================================================
/**
    Software-based MP3 decoding format (doesn't currently provide an encoder).

    IMPORTANT DISCLAIMER: By choosing to  compile the MP3 code into your
   software, you do so AT YOUR OWN RISK! By doing so, you are agreeing that Raw
   Material Software Limited is in no way responsible for any patent, copyright,
   or other legal issues that you may suffer as a result.

    The code in juce_PatchedMP3AudioFormat.cpp is NOT guaranteed to be free from
   infringements of 3rd-party intellectual property. If you wish to use it,
   please seek your own independent advice about the legality of doing so. If
   you are not willing to accept full responsibility for the consequences of
   using this code, then do not use this.

    @tags{Audio}
*/
class PatchedMP3AudioFormat : public AudioFormat {
public:
  //==============================================================================
  PatchedMP3AudioFormat();
  ~PatchedMP3AudioFormat() override;

  //==============================================================================
  Array<int> getPossibleSampleRates() override;
  Array<int> getPossibleBitDepths() override;
  bool canDoStereo() override;
  bool canDoMono() override;
  bool isCompressed() override;
  StringArray getQualityOptions() override;

  //==============================================================================
  AudioFormatReader *createReaderFor(InputStream *,
                                     bool deleteStreamIfOpeningFails) override;

  AudioFormatWriter *createWriterFor(OutputStream *, double sampleRateToUse,
                                     unsigned int numberOfChannels,
                                     int bitsPerSample,
                                     const StringPairArray &metadataValues,
                                     int qualityOptionIndex) override;
  using AudioFormat::createWriterFor;
};

class AudioFormatReaderWithPosition : public AudioFormatReader {
public:
  AudioFormatReaderWithPosition(InputStream *sourceStream,
                                const String &formatName)
      : AudioFormatReader(sourceStream, formatName) {}
  virtual int64 getCurrentPosition() const = 0;
  virtual bool lengthIsApproximate() const { return false; };
};

} // namespace juce
