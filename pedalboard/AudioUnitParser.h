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

#if JUCE_MAC

namespace Pedalboard {
// This uses Objective-C++ code, so its implementation can't be defined inline:
std::vector<std::string>
getAudioUnitIdentifiersFromFile(const juce::String &filename);

/**
 * An Audio Unit identifier ("AudioUnit:Effects/aufx,abcd,Manu") together with
 * the component name declared in the bundle's Info.plist (usually
 * "Manufacturer: Plugin Name"). Reading this requires no instantiation, so it
 * can be used to pick a single plugin out of a multi-plugin bundle cheaply.
 */
struct AudioUnitComponentInfo {
  std::string identifier;
  std::string name;
};

std::vector<AudioUnitComponentInfo>
getAudioUnitComponentInfosFromFile(const juce::String &filename);
} // namespace Pedalboard

#endif