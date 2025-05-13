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

#ifdef JUCE_WASM
#include "../../JUCE/modules/juce_core/juce_core.h"

namespace juce {

WebInputStream::WebInputStream(const URL &url, const bool usePost) : pimpl() {}

WebInputStream::~WebInputStream() {}

WebInputStream &WebInputStream::withExtraHeaders(const String &extra) {
  return *this;
}
WebInputStream &WebInputStream::withCustomRequestCommand(const String &cmd) {
  return *this;
}
WebInputStream &WebInputStream::withConnectionTimeout(int t) { return *this; }
WebInputStream &WebInputStream::withNumRedirectsToFollow(int num) {
  return *this;
}
StringPairArray WebInputStream::getRequestHeaders() const { return {}; }
StringPairArray WebInputStream::getResponseHeaders() { return {}; }
bool WebInputStream::isError() const { return false; }
void WebInputStream::cancel() {}
bool WebInputStream::isExhausted() { return false; }
int64 WebInputStream::getPosition() { return 0; }
int64 WebInputStream::getTotalLength() { return 0; }
int WebInputStream::read(void *buffer, int bytes) { return 0; }
bool WebInputStream::setPosition(int64 pos) { return false; }
int WebInputStream::getStatusCode() { return 0; }
bool WebInputStream::connect(Listener *listener) { return false; }

StringPairArray WebInputStream::parseHttpHeaders(const String &headerData) {
  return {};
}

void WebInputStream::createHeadersAndPostData(const URL &aURL, String &headers,
                                              MemoryBlock &data,
                                              bool addParametersToBody) {}

class WebInputStream::Pimpl {};

} // namespace juce
#endif