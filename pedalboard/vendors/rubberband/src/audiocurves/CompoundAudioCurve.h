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

#ifndef RUBBERBAND_COMPOUND_AUDIO_CURVE_H
#define RUBBERBAND_COMPOUND_AUDIO_CURVE_H

#include "PercussiveAudioCurve.h"
#include "HighFrequencyAudioCurve.h"
#include "../dsp/SampleFilter.h"

namespace RubberBand
{

class CompoundAudioCurve : public AudioCurveCalculator
{
public:
    CompoundAudioCurve(Parameters parameters);

    virtual ~CompoundAudioCurve();

    enum Type {
        PercussiveDetector,
        CompoundDetector,
        SoftDetector
    };
    virtual void setType(Type); // default is CompoundDetector
    
    virtual void setFftSize(int newSize);

    virtual float processFloat(const float *R__ mag, int increment);
    virtual double processDouble(const double *R__ mag, int increment);

    virtual void reset();

protected:
    PercussiveAudioCurve m_percussive;
    HighFrequencyAudioCurve m_hf;

    SampleFilter<double> *m_hfFilter;
    SampleFilter<double> *m_hfDerivFilter;

    Type m_type;

    double m_lastHf;
    double m_lastResult;
    int m_risingCount;

    double processFiltering(double percussive, double hf);
};

}

#endif
