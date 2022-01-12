/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2021 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#include "../rubberband/rubberband-c.h"
#include "../rubberband/RubberBandStretcher.h"

struct RubberBandState_
{
    RubberBand::RubberBandStretcher *m_s;
};

RubberBandState rubberband_new(unsigned int sampleRate,
                               unsigned int channels,
                               RubberBandOptions options,
                               double initialTimeRatio,
                               double initialPitchScale)
{
    RubberBandState_ *state = new RubberBandState_();
    state->m_s = new RubberBand::RubberBandStretcher
        (sampleRate, channels, options,
         initialTimeRatio, initialPitchScale);
    return state;
}

void rubberband_delete(RubberBandState state)
{
    delete state->m_s;
    delete state;
}

void rubberband_reset(RubberBandState state)
{
    state->m_s->reset();
}

void rubberband_set_time_ratio(RubberBandState state, double ratio)
{
    state->m_s->setTimeRatio(ratio);
}

void rubberband_set_pitch_scale(RubberBandState state, double scale)
{
    state->m_s->setPitchScale(scale);
}

double rubberband_get_time_ratio(const RubberBandState state) 
{
    return state->m_s->getTimeRatio();
}

double rubberband_get_pitch_scale(const RubberBandState state)
{
    return state->m_s->getPitchScale();
}

unsigned int rubberband_get_latency(const RubberBandState state) 
{
    return state->m_s->getLatency();
}

void rubberband_set_transients_option(RubberBandState state, RubberBandOptions options)
{
    state->m_s->setTransientsOption(options);
}

void rubberband_set_detector_option(RubberBandState state, RubberBandOptions options)
{
    state->m_s->setDetectorOption(options);
}

void rubberband_set_phase_option(RubberBandState state, RubberBandOptions options)
{
    state->m_s->setPhaseOption(options);
}

void rubberband_set_formant_option(RubberBandState state, RubberBandOptions options)
{
    state->m_s->setFormantOption(options);
}

void rubberband_set_pitch_option(RubberBandState state, RubberBandOptions options)
{
    state->m_s->setPitchOption(options);
}

void rubberband_set_expected_input_duration(RubberBandState state, unsigned int samples)
{
    state->m_s->setExpectedInputDuration(samples);
}

unsigned int rubberband_get_samples_required(const RubberBandState state)
{
    return state->m_s->getSamplesRequired();
}

void rubberband_set_max_process_size(RubberBandState state, unsigned int samples)
{
    state->m_s->setMaxProcessSize(samples);
}

void rubberband_set_key_frame_map(RubberBandState state, unsigned int keyframecount, unsigned int *from, unsigned int *to)
{
    std::map<size_t, size_t> kfm;
    for (unsigned int i = 0; i < keyframecount; ++i) {
        kfm[from[i]] = to[i];
    }
    state->m_s->setKeyFrameMap(kfm);
}

void rubberband_study(RubberBandState state, const float *const *input, unsigned int samples, int final)
{
    state->m_s->study(input, samples, final != 0);
}

void rubberband_process(RubberBandState state, const float *const *input, unsigned int samples, int final)
{
    state->m_s->process(input, samples, final != 0);
}

int rubberband_available(const RubberBandState state)
{
    return state->m_s->available();
}

unsigned int rubberband_retrieve(const RubberBandState state, float *const *output, unsigned int samples)
{
    return state->m_s->retrieve(output, samples);
}

unsigned int rubberband_get_channel_count(const RubberBandState state)
{
    return state->m_s->getChannelCount();
}

void rubberband_calculate_stretch(RubberBandState state)
{
    state->m_s->calculateStretch();
}

void rubberband_set_debug_level(RubberBandState state, int level)
{
    state->m_s->setDebugLevel(level);
}

void rubberband_set_default_debug_level(int level)
{
    RubberBand::RubberBandStretcher::setDefaultDebugLevel(level);
}

