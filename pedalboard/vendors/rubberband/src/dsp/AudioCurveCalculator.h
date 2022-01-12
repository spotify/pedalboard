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

#ifndef RUBBERBAND_AUDIO_CURVE_CALCULATOR_H
#define RUBBERBAND_AUDIO_CURVE_CALCULATOR_H

#include <sys/types.h>

#include "../system/sysutils.h"

namespace RubberBand 
{

/**
 * AudioCurveCalculator turns a sequence of audio "columns" --
 * short-time spectrum magnitude blocks -- into a sequence of numbers
 * representing some quality of the input such as power or likelihood
 * of an onset occurring.
 *
 * These are typically low-level building-blocks: AudioCurveCalculator
 * is a simple causal interface in which each input column corresponds
 * to exactly one output value which is returned immediately.  They
 * have far less power (because of the causal interface and
 * magnitude-only input) and flexibility (because of the limited
 * return types) than for example the Vamp plugin interface.
 *
 * AudioCurveCalculator implementations typically remember the history
 * of their processing data, and the caller must call reset() before
 * resynchronising to an unrelated piece of input audio.
 */
class AudioCurveCalculator
{
public:
    struct Parameters {
        Parameters(int _sampleRate, int _fftSize) :
            sampleRate(_sampleRate),
            fftSize(_fftSize)
        { }
        int sampleRate;
        int fftSize;
    };

    AudioCurveCalculator(Parameters parameters);
    virtual ~AudioCurveCalculator();

    int getSampleRate() const { return m_sampleRate; }
    int getFftSize() const { return m_fftSize; }

    virtual void setSampleRate(int newRate);
    virtual void setFftSize(int newSize);

    Parameters getParameters() const {
        return Parameters(m_sampleRate, m_fftSize);
    }
    void setParameters(Parameters p) {
        setSampleRate(p.sampleRate);
        setFftSize(p.fftSize);
    }

    // You may not mix calls to the various process functions on a
    // given instance


    /**
     * Process the given magnitude spectrum block and return the curve
     * value for it.  The mag input contains (fftSize/2 + 1) values
     * corresponding to the magnitudes of the complex FFT output bins
     * for a windowed input of size fftSize.  The hop (expressed in
     * time-domain audio samples) from the previous to the current
     * input block is given by increment.
     */
    virtual float processFloat(const float *R__ mag, int increment) = 0;

    /**
     * Process the given magnitude spectrum block and return the curve
     * value for it.  The mag input contains (fftSize/2 + 1) values
     * corresponding to the magnitudes of the complex FFT output bins
     * for a windowed input of size fftSize.  The hop (expressed in
     * time-domain audio samples) from the previous to the current
     * input block is given by increment.
     */
    virtual double processDouble(const double *R__ mag, int increment) = 0;

    /**
     * Obtain a confidence for the curve value (if applicable). A
     * value of 1.0 indicates perfect confidence in the curve
     * calculation, 0.0 indicates none.
     */
    virtual double getConfidence() const { return 1.0; }

    /**
     * Reset the calculator, forgetting the history of the audio input
     * so far.
     */
    virtual void reset() = 0;

    /**
     * If the output of this calculator has a known unit, return it as
     * text.  For example, "Hz" or "V".
     */
    virtual const char *getUnit() const { return ""; }

protected:
    int m_sampleRate;
    int m_fftSize;
    int m_lastPerceivedBin;
    void recalculateLastPerceivedBin();
};


}

#endif

