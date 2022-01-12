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

#ifndef RUBBERBAND_FFT_H
#define RUBBERBAND_FFT_H

#include "../system/sysutils.h"

#include <string>
#include <set>

namespace RubberBand {

class FFTImpl;

/**
 * Provide the basic FFT computations we need, using one of a set of
 * candidate FFT implementations (depending on compile flags).
 *
 * Implements real->complex FFTs of power-of-two sizes only.  Note
 * that only the first half of the output signal is returned (the
 * complex conjugates half is omitted), so the "complex" arrays need
 * room for size/2+1 elements.
 *
 * The "interleaved" functions use the format sometimes called CCS --
 * size/2+1 real+imaginary pairs.  So, the array elements at indices 1
 * and size+1 will always be zero (since the signal is real).
 * 
 * All pointer arguments must point to valid data. A NullArgument
 * exception is thrown if any argument is NULL.
 *
 * Neither forward nor inverse transform is scaled.
 *
 * This class is reentrant but not thread safe: use a separate
 * instance per thread, or use a mutex.
 */
class FFT
{
public:
    enum Exception {
        NullArgument, InvalidSize, InvalidImplementation, InternalError
    };

    FFT(int size, int debugLevel = 0); // may throw InvalidSize
    ~FFT();

    int getSize() const;
    
    void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut);
    void forwardInterleaved(const double *R__ realIn, double *R__ complexOut);
    void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut);
    void forwardMagnitude(const double *R__ realIn, double *R__ magOut);

    void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut);
    void forwardInterleaved(const float *R__ realIn, float *R__ complexOut);
    void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut);
    void forwardMagnitude(const float *R__ realIn, float *R__ magOut);

    void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut);
    void inverseInterleaved(const double *R__ complexIn, double *R__ realOut);
    void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut);
    void inverseCepstral(const double *R__ magIn, double *R__ cepOut);

    void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut);
    void inverseInterleaved(const float *R__ complexIn, float *R__ realOut);
    void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut);
    void inverseCepstral(const float *R__ magIn, float *R__ cepOut);

    // Calling one or both of these is optional -- if neither is
    // called, the first call to a forward or inverse method will call
    // init().  You only need call these if you don't want to risk
    // expensive allocations etc happening in forward or inverse.
    void initFloat();
    void initDouble();

    enum Precision {
        SinglePrecision = 0x1,
        DoublePrecision = 0x2
    };
    typedef int Precisions;

    /**
     * Return the OR of all precisions supported by this
     * implementation. All of the functions (float and double) are
     * available regardless of the supported implementations, but they
     * will be calculated at the proper precision only if it is
     * available. (So float functions will be calculated using doubles
     * and then truncated if single-precision is unavailable, and
     * double functions will use single-precision arithmetic if double
     * is unavailable.)
     */
    Precisions getSupportedPrecisions() const;

    static std::set<std::string> getImplementations();
    static std::string getDefaultImplementation();
    static void setDefaultImplementation(std::string);

#ifdef FFT_MEASUREMENT
    static std::string tune();
#endif

protected:
    FFTImpl *d;
    static std::string m_implementation;
    static void pickDefaultImplementation();
    
private:
    FFT(const FFT &); // not provided
    FFT &operator=(const FFT &); // not provided
};

}

#endif

