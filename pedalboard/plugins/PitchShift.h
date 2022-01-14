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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../RubberbandPlugin.h"

namespace Pedalboard
{
    class PitchShift : public RubberbandPlugin
    /*
    Modifies an audios pitch (without affecting duration [confirm if this is the case in RT mode])
    */
    {
    private:
        double _pitchScale = 1.0;

    public:
        void setPitchScale(double scale)
        // Instead of mirroring pitchScale value would it be better
        // to check pointer and then throw error if doesn't exist?
        // Cannot create ptr to rubberband object in constructor because don't
        // have sample rate / num channels. Could use dummy instead however.
        {
            if (scale < 0)
            {
                // Throw error
            }
            _pitchScale = scale;
        }

        double getPitchScale()
        {
            return _pitchScale;
        }

        void prepare(const juce::dsp::ProcessSpec &spec) override final
        {

            bool specChanged = lastSpec.sampleRate != spec.sampleRate ||
                               lastSpec.maximumBlockSize < spec.maximumBlockSize ||
                               spec.numChannels != lastSpec.numChannels;

            if (!rbPtr || specChanged)
            {
                // No need to explicitly call reset if constructing new object

                // Is this appropriate to create a new object every time the spec changes?
                auto rb = new RubberBandStretcher(spec.sampleRate, spec.numChannels, RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionThreadingNever);
                rb->setMaxProcessSize(spec.maximumBlockSize);
                rb->setPitchScale(_pitchScale);
                // what about deallocating?
                delete rbPtr;
                rbPtr = rb;
                // dspBlock.prepare(spec);
                lastSpec = spec;
            }
        }
    };
    
    inline void init_pitch_shift(py::module &m)
    {
        py::class_<PitchShift, Plugin>(m, "PitchShift", "Shift pitch without affecting audio duration")
            .def(py::init([](double scale)
                          {
                    auto plugin = new PitchShift();
                    plugin->setPitchScale(scale);
                    return plugin; }))
            .def_property("pitch_scale", &PitchShift::getPitchScale, &PitchShift::setPitchScale);
    }
}; // namespace Pedalboard