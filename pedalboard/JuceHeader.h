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

// NOTE(psobot): We include these modules conditionally to avoid unnecessary
// dependencies on modules that are not available.

#ifdef JUCE_MODULE_AVAILABLE_juce_audio_basics
#include <juce_audio_basics/juce_audio_basics.h>
#endif
#ifdef JUCE_MODULE_AVAILABLE_juce_audio_devices
#include <juce_audio_devices/juce_audio_devices.h>
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
#ifdef JUCE_MODULE_AVAILABLE_juce_events
#include <juce_events/juce_events.h>
#endif
#ifdef JUCE_MODULE_AVAILABLE_juce_graphics
#include <juce_graphics/juce_graphics.h>
#endif
#ifdef JUCE_MODULE_AVAILABLE_juce_gui_basics
#include <juce_gui_basics/juce_gui_basics.h>
#endif
#ifdef JUCE_MODULE_AVAILABLE_juce_gui_extra
#include <juce_gui_extra/juce_gui_extra.h>
#endif
