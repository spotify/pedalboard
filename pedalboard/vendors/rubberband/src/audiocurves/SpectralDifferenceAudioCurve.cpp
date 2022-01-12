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

#include "SpectralDifferenceAudioCurve.h"

#include "../system/Allocators.h"
#include "../system/VectorOps.h"

namespace RubberBand
{


SpectralDifferenceAudioCurve::SpectralDifferenceAudioCurve(Parameters parameters) :
    AudioCurveCalculator(parameters)
{
    m_mag = allocate<double>(m_lastPerceivedBin + 1);
    m_tmpbuf = allocate<double>(m_lastPerceivedBin + 1);
    v_zero(m_mag, m_lastPerceivedBin + 1);
}

SpectralDifferenceAudioCurve::~SpectralDifferenceAudioCurve()
{
    deallocate(m_mag);
    deallocate(m_tmpbuf);
}

void
SpectralDifferenceAudioCurve::reset()
{
    v_zero(m_mag, m_lastPerceivedBin + 1);
}

void
SpectralDifferenceAudioCurve::setFftSize(int newSize)
{
    deallocate(m_tmpbuf);
    deallocate(m_mag);
    AudioCurveCalculator::setFftSize(newSize);
    m_mag = allocate<double>(m_lastPerceivedBin + 1);
    m_tmpbuf = allocate<double>(m_lastPerceivedBin + 1);
    reset();
}

float
SpectralDifferenceAudioCurve::processFloat(const float *R__ mag, int)
{
    double result = 0.0;

    const int hs1 = m_lastPerceivedBin + 1;

    v_convert(m_tmpbuf, mag, hs1);
    v_square(m_tmpbuf, hs1);
    v_subtract(m_mag, m_tmpbuf, hs1);
    v_abs(m_mag, hs1);
    v_sqrt(m_mag, hs1);
    
    for (int i = 0; i < hs1; ++i) {
        result += m_mag[i];
    }

    v_copy(m_mag, m_tmpbuf, hs1);
    return result;
}

double
SpectralDifferenceAudioCurve::processDouble(const double *R__ mag, int)
{
    double result = 0.0;

    const int hs1 = m_lastPerceivedBin + 1;

    v_convert(m_tmpbuf, mag, hs1);
    v_square(m_tmpbuf, hs1);
    v_subtract(m_mag, m_tmpbuf, hs1);
    v_abs(m_mag, hs1);
    v_sqrt(m_mag, hs1);
    
    for (int i = 0; i < hs1; ++i) {
        result += m_mag[i];
    }

    v_copy(m_mag, m_tmpbuf, hs1);
    return result;
}

}

