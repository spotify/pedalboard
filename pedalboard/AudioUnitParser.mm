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

#include "AudioUnitParser.h"

#if JUCE_MAC
#include <AudioToolbox/AudioUnitUtilities.h>
#include <AVFoundation/AVFAudio.h>

namespace Pedalboard {

static OSType stringToOSType(juce::String s) {
  if (s.trim().length() >= 4) // (to avoid trimming leading spaces)
    s = s.trim();

  s += "    ";

  return (((OSType)(unsigned char)s[0]) << 24) |
         (((OSType)(unsigned char)s[1]) << 16) |
         (((OSType)(unsigned char)s[2]) << 8) | ((OSType)(unsigned char)s[3]);
}

static juce::String osTypeToString(OSType type) noexcept {
  const juce::juce_wchar s[4] = {(juce::juce_wchar)((type >> 24) & 0xff),
                                 (juce::juce_wchar)((type >> 16) & 0xff),
                                 (juce::juce_wchar)((type >> 8) & 0xff),
                                 (juce::juce_wchar)(type & 0xff)};
  return juce::String(s, 4);
}

const char *AU_IDENTIFIER_PREFIX = "AudioUnit:";

static juce::String
createPluginIdentifier(const AudioComponentDescription &desc) {
  juce::String s(AU_IDENTIFIER_PREFIX);

  if (desc.componentType == kAudioUnitType_MusicDevice)
    s << "Synths/";
  else if (desc.componentType == kAudioUnitType_MusicEffect ||
           desc.componentType == kAudioUnitType_Effect)
    s << "Effects/";
  else if (desc.componentType == kAudioUnitType_Generator)
    s << "Generators/";
  else if (desc.componentType == kAudioUnitType_Panner)
    s << "Panners/";
  else if (desc.componentType == kAudioUnitType_Mixer)
    s << "Mixers/";
  else if (desc.componentType == kAudioUnitType_MIDIProcessor)
    s << "MidiEffects/";

  s << osTypeToString(desc.componentType) << ","
    << osTypeToString(desc.componentSubType) << ","
    << osTypeToString(desc.componentManufacturer);

  return s;
}

inline juce::String nsStringToJuce(NSString *s) {
  return juce::CharPointer_UTF8([s UTF8String]);
}

std::vector<std::string>
getAudioUnitIdentifiersFromFile(const juce::String &filename) {
  std::vector<std::string> identifiers;

  const juce::File file(filename);
  if (!file.hasFileExtension(".component") &&
      !file.hasFileExtension(".appex")) {
    return identifiers;
  }

  if (file.hasFileExtension(".appex")) {
    AVAudioUnitComponentManager *manager = [AVAudioUnitComponentManager sharedAudioUnitComponentManager];
    AudioComponentDescription desc = {0};
    NSArray *audioComponents = [manager componentsMatchingDescription:desc];
    NSString *filePathWithoutTrailingSlash = (NSString *)(filename.trimCharactersAtEnd("/") + "/").toCFString();

    for (AVAudioUnitComponent *component in audioComponents) {
      if ([component.componentURL.absoluteString hasSuffix:filePathWithoutTrailingSlash]) {
        identifiers.push_back(createPluginIdentifier(component.audioComponentDescription).toStdString());
        break;
      }
    }
  } else {
    // Use NSBundle to open and extract identifiers:
    NSBundle *bundle =
        [[NSBundle alloc] initWithPath:(NSString *)filename.toCFString()];

    NSArray *audioComponents =
        [bundle objectForInfoDictionaryKey:@"AudioComponents"];
    for (NSDictionary *component in audioComponents) {
      AudioComponentDescription desc;
      desc.componentManufacturer = stringToOSType(
          nsStringToJuce((NSString *)[component valueForKey:@"manufacturer"]));
      desc.componentType = stringToOSType(
          nsStringToJuce((NSString *)[component valueForKey:@"type"]));
      desc.componentSubType = stringToOSType(
          nsStringToJuce((NSString *)[component valueForKey:@"subtype"]));
      identifiers.push_back(createPluginIdentifier(desc).toStdString());
    }

    [bundle release];
  }

  return identifiers;
}

}
#endif