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

/*
    This file is usually created by the Projucer, but as we're building a Python
   package, this is custom-managed.
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#define JUCE_PROJUCER_VERSION 0x60008

#if JUCE_PROJUCER_VERSION < JUCE_VERSION
/** If you've hit this error then the version of the Projucer that was used to
   generate this project is older than the version of the JUCE modules being
   included. To fix this error, re-save your project using the latest version of
   the Projucer or, if you aren't using the Projucer to manage your project,
    remove the JUCE_PROJUCER_VERSION define from the AppConfig.h file.
*/
#error                                                                         \
    "This project was last saved using an outdated version of the Projucer! Re-save this project with the latest version to fix this error."
#endif