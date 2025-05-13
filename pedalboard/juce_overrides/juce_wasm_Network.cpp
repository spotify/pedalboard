/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include "../../JUCE/modules/juce_core/juce_core.h"

namespace juce {

void MACAddress::findAllAddresses(Array<MACAddress> &result) {}

bool JUCE_CALLTYPE Process::openEmailWithAttachments(
    const String & /* targetEmailAddress */, const String & /* emailSubject */,
    const String & /* bodyText */, const StringArray & /* filesToAttach */) {
  return false;
}

} // namespace juce
