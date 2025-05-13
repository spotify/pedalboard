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

MemoryMappedFile::~MemoryMappedFile() {}

bool Process::openDocument(String const &, String const &) { return false; }

void MemoryMappedFile::openInternal(File const &, MemoryMappedFile::AccessMode,
                                    bool) {}

DirectoryIterator::NativeIterator::~NativeIterator() {}

std::unique_ptr<URL::DownloadTask>
URL::downloadToFile(File const &, URL::DownloadTaskOptions const &) {
  return nullptr;
}

DirectoryIterator::NativeIterator::NativeIterator(File const &,
                                                  String const &) {}

bool DirectoryIterator::NativeIterator::next(String &, bool *, bool *,
                                             long long *, Time *, Time *,
                                             bool *) {
  return false;
}

} // namespace juce
