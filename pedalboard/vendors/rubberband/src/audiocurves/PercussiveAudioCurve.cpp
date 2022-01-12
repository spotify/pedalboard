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

#include "PercussiveAudioCurve.h"

#include "../system/Allocators.h"
#include "../system/VectorOps.h"

#include <cmath>
#include <iostream>
namespace RubberBand
{


PercussiveAudioCurve::PercussiveAudioCurve(Parameters parameters) :
    AudioCurveCalculator(parameters)
{
    m_prevMag = allocate_and_zero<double>(m_fftSize/2 + 1);
}

PercussiveAudioCurve::~PercussiveAudioCurve()
{
    deallocate(m_prevMag);
}

void
PercussiveAudioCurve::reset()
{
    v_zero(m_prevMag, m_fftSize/2 + 1);
}

void
PercussiveAudioCurve::setFftSize(int newSize)
{
    m_prevMag = reallocate(m_prevMag, m_fftSize/2 + 1, newSize/2 + 1);
    AudioCurveCalculator::setFftSize(newSize);
    reset();
}

float
PercussiveAudioCurve::processFloat(const float *R__ mag, int)
{
    static float threshold = powf(10.f, 0.15f); // 3dB rise in square of magnitude
    static float zeroThresh = powf(10.f, -8);

    int count = 0;
    int nonZeroCount = 0;

    const int sz = m_lastPerceivedBin;

    for (int n = 1; n <= sz; ++n) {
        float v = 0.f;
        if (m_prevMag[n] > zeroThresh) v = mag[n] / m_prevMag[n];
        else if (mag[n] > zeroThresh) v = threshold;
        bool above = (v >= threshold);
        if (above) ++count;
        if (mag[n] > zeroThresh) ++nonZeroCount;
    }

    v_convert(m_prevMag, mag, sz + 1);

    if (nonZeroCount == 0) return 0;
    else return float(count) / float(nonZeroCount);
}

double
PercussiveAudioCurve::processDouble(const double *R__ mag, int)
{
    static double threshold = pow(10., 0.15); // 3dB rise in square of magnitude
    static double zeroThresh = pow(10., -8);

    int count = 0;
    int nonZeroCount = 0;

    const int sz = m_lastPerceivedBin;

    for (int n = 1; n <= sz; ++n) {
        double v = 0.0;
        if (m_prevMag[n] > zeroThresh) v = mag[n] / m_prevMag[n];
        else if (mag[n] > zeroThresh) v = threshold;
        bool above = (v >= threshold);
        if (above) ++count;
        if (mag[n] > zeroThresh) ++nonZeroCount;
    }

    v_copy(m_prevMag, mag, sz + 1);

    if (nonZeroCount == 0) return 0;
    else return double(count) / double(nonZeroCount);
}


}

