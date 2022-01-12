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

#ifndef RUBBERBAND_VECTOR_OPS_COMPLEX_H
#define RUBBERBAND_VECTOR_OPS_COMPLEX_H

#include "VectorOps.h"


namespace RubberBand {


template<typename T>
inline void c_phasor(T *real, T *imag, T phase)
{
    //!!! IPP contains ippsSinCos_xxx in ippvm.h -- these are
    //!!! fixed-accuracy, test and compare
#if defined HAVE_VDSP
    int one = 1;
    if (sizeof(T) == sizeof(float)) {
        vvsincosf((float *)imag, (float *)real, (const float *)&phase, &one);
    } else {
        vvsincos((double *)imag, (double *)real, (const double *)&phase, &one);
    }
#elif defined LACK_SINCOS
    if (sizeof(T) == sizeof(float)) {
        *real = cosf(phase);
        *imag = sinf(phase);
    } else {
        *real = cos(phase);
        *imag = sin(phase);
    }
#elif defined __GNUC__
    if (sizeof(T) == sizeof(float)) {
        sincosf(phase, (float *)imag, (float *)real);
    } else {
        sincos(phase, (double *)imag, (double *)real);
    }
#else
    if (sizeof(T) == sizeof(float)) {
        *real = cosf(phase);
        *imag = sinf(phase);
    } else {
        *real = cos(phase);
        *imag = sin(phase);
    }
#endif
}

template<typename T>
inline void c_magphase(T *mag, T *phase, T real, T imag)
{
    *mag = sqrt(real * real + imag * imag);
    *phase = atan2(imag, real);
}

#ifdef USE_APPROXIMATE_ATAN2
// NB arguments in opposite order from usual for atan2f
extern float approximate_atan2f(float real, float imag);
template<>
inline void c_magphase(float *mag, float *phase, float real, float imag)
{
    float atan = approximate_atan2f(real, imag);
    *phase = atan;
    *mag = sqrtf(real * real + imag * imag);
}
#else
template<>
inline void c_magphase(float *mag, float *phase, float real, float imag)
{
    *mag = sqrtf(real * real + imag * imag);
    *phase = atan2f(imag, real);
}
#endif


template<typename S, typename T> // S source, T target
void v_polar_to_cartesian(T *const R__ real,
                          T *const R__ imag,
                          const S *const R__ mag,
                          const S *const R__ phase,
                          const int count)
{
    for (int i = 0; i < count; ++i) {
        c_phasor<T>(real + i, imag + i, phase[i]);
    }
    v_multiply(real, mag, count);
    v_multiply(imag, mag, count);
}

template<typename T>
void v_polar_interleaved_to_cartesian_inplace(T *const R__ srcdst,
                                              const int count)
{
    T real, imag;
    for (int i = 0; i < count*2; i += 2) {
        c_phasor(&real, &imag, srcdst[i+1]);
        real *= srcdst[i];
        imag *= srcdst[i];
        srcdst[i] = real;
        srcdst[i+1] = imag;
    }
}

template<typename S, typename T> // S source, T target
void v_polar_to_cartesian_interleaved(T *const R__ dst,
                                      const S *const R__ mag,
                                      const S *const R__ phase,
                                      const int count)
{
    T real, imag;
    for (int i = 0; i < count; ++i) {
        c_phasor<T>(&real, &imag, phase[i]);
        real *= mag[i];
        imag *= mag[i];
        dst[i*2] = real;
        dst[i*2+1] = imag;
    }
}    

#if defined USE_POMMIER_MATHFUN
void v_polar_to_cartesian_pommier(float *const R__ real,
                                  float *const R__ imag,
                                  const float *const R__ mag,
                                  const float *const R__ phase,
                                  const int count);
void v_polar_interleaved_to_cartesian_inplace_pommier(float *const R__ srcdst,
                                                      const int count);
void v_polar_to_cartesian_interleaved_pommier(float *const R__ dst,
                                              const float *const R__ mag,
                                              const float *const R__ phase,
                                              const int count);

template<>
inline void v_polar_to_cartesian(float *const R__ real,
                                 float *const R__ imag,
                                 const float *const R__ mag,
                                 const float *const R__ phase,
                                 const int count)
{
    v_polar_to_cartesian_pommier(real, imag, mag, phase, count);
}

template<>
inline void v_polar_interleaved_to_cartesian_inplace(float *const R__ srcdst,
                                                     const int count)
{
    v_polar_interleaved_to_cartesian_inplace_pommier(srcdst, count);
}

template<>
inline void v_polar_to_cartesian_interleaved(float *const R__ dst,
                                             const float *const R__ mag,
                                             const float *const R__ phase,
                                             const int count)
{
    v_polar_to_cartesian_interleaved_pommier(dst, mag, phase, count);
}

#endif

template<typename S, typename T> // S source, T target
void v_cartesian_to_polar(T *const R__ mag,
                          T *const R__ phase,
                          const S *const R__ real,
                          const S *const R__ imag,
                          const int count)
{
    for (int i = 0; i < count; ++i) {
        c_magphase<T>(mag + i, phase + i, real[i], imag[i]);
    }
}

template<typename S, typename T> // S source, T target
void v_cartesian_interleaved_to_polar(T *const R__ mag,
                                      T *const R__ phase,
                                      const S *const R__ src,
                                      const int count)
{
    for (int i = 0; i < count; ++i) {
        c_magphase<T>(mag + i, phase + i, src[i*2], src[i*2+1]);
    }
}

#ifdef HAVE_VDSP
template<>
inline void v_cartesian_to_polar(float *const R__ mag,
                                 float *const R__ phase,
                                 const float *const R__ real,
                                 const float *const R__ imag,
                                 const int count)
{
    DSPSplitComplex c;
    c.realp = const_cast<float *>(real);
    c.imagp = const_cast<float *>(imag);
    vDSP_zvmags(&c, 1, phase, 1, count); // using phase as a temporary dest
    vvsqrtf(mag, phase, &count); // using phase as the source
    vvatan2f(phase, imag, real, &count);
}
template<>
inline void v_cartesian_to_polar(double *const R__ mag,
                                 double *const R__ phase,
                                 const double *const R__ real,
                                 const double *const R__ imag,
                                 const int count)
{
    // double precision, this is significantly faster than using vDSP_polar
    DSPDoubleSplitComplex c;
    c.realp = const_cast<double *>(real);
    c.imagp = const_cast<double *>(imag);
    vDSP_zvmagsD(&c, 1, phase, 1, count); // using phase as a temporary dest
    vvsqrt(mag, phase, &count); // using phase as the source
    vvatan2(phase, imag, real, &count);
}
#endif

template<typename T>
void v_cartesian_to_polar_interleaved_inplace(T *const R__ srcdst,
                                              const int count)
{
    T mag, phase;
    for (int i = 0; i < count * 2; i += 2) {
        c_magphase(&mag, &phase, srcdst[i], srcdst[i+1]);
        srcdst[i] = mag;
        srcdst[i+1] = phase;
    }
}

template<typename S, typename T> // S source, T target
void v_cartesian_to_magnitudes(T *const R__ mag,
                               const S *const R__ real,
                               const S *const R__ imag,
                               const int count)
{
    for (int i = 0; i < count; ++i) {
        mag[i] = T(sqrt(real[i] * real[i] + imag[i] * imag[i]));
    }
}

template<typename S, typename T> // S source, T target
void v_cartesian_interleaved_to_magnitudes(T *const R__ mag,
                                           const S *const R__ src,
                                           const int count)
{
    for (int i = 0; i < count; ++i) {
        mag[i] = T(sqrt(src[i*2] * src[i*2] + src[i*2+1] * src[i*2+1]));
    }
}

#ifdef HAVE_IPP
template<>
inline void v_cartesian_to_magnitudes(float *const R__ mag,
                                      const float *const R__ real,
                                      const float *const R__ imag,
                                      const int count)
{
    ippsMagnitude_32f(real, imag, mag, count);
}

template<>
inline void v_cartesian_to_magnitudes(double *const R__ mag,
                                      const double *const R__ real,
                                      const double *const R__ imag,
                                      const int count)
{
    ippsMagnitude_64f(real, imag, mag, count);
}

template<>
inline void v_cartesian_interleaved_to_magnitudes(float *const R__ mag,
                                                  const float *const R__ src,
                                                  const int count)
{
    ippsMagnitude_32fc((const Ipp32fc *)src, mag, count);
}

template<>
inline void v_cartesian_interleaved_to_magnitudes(double *const R__ mag,
                                                  const double *const R__ src,
                                                  const int count)
{
    ippsMagnitude_64fc((const Ipp64fc *)src, mag, count);
}
#endif

}

#endif

