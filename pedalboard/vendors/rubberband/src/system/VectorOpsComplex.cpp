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

#include "VectorOpsComplex.h"

#include "sysutils.h"

#include <cassert>

#if defined USE_POMMIER_MATHFUN
#if defined __ARMEL__ || defined __aarch64__
#include "pommier/neon_mathfun.h"
#else
#include "pommier/sse_mathfun.h"
#endif
#endif

namespace RubberBand {

#ifdef USE_APPROXIMATE_ATAN2
float approximate_atan2f(float real, float imag)
{
    static const float pi = M_PI;
    static const float pi2 = M_PI / 2;

    float atan;

    if (real == 0.f) {

        if (imag > 0.0f) atan = pi2;
        else if (imag == 0.0f) atan = 0.0f;
        else atan = -pi2;

    } else {

        float z = imag/real;

        if (fabsf(z) < 1.f) {
            atan = z / (1.f + 0.28f * z * z);
            if (real < 0.f) {
                if (imag < 0.f) atan -= pi;
                else atan += pi;
            }
        } else {
            atan = pi2 - z / (z * z + 0.28f);
            if (imag < 0.f) atan -= pi;
        }
    }
}
#endif

#if defined USE_POMMIER_MATHFUN

#if defined __ARMEL__ || defined __aarch64__
typedef union {
  float f[4];
  int i[4];
  v4sf  v;
} V4SF;
#else
typedef ALIGN16_BEG union {
  float f[4];
  int i[4];
  v4sf  v;
} ALIGN16_END V4SF;
#endif

void
v_polar_to_cartesian_pommier(float *const R__ real,
                             float *const R__ imag,
                             const float *const R__ mag,
                             const float *const R__ phase,
                             const int count)
{
    int idx = 0, tidx = 0;
    int i = 0;

    for (int i = 0; i + 4 < count; i += 4) {

	V4SF fmag, fphase, fre, fim;

        for (int j = 0; j < 3; ++j) {
            fmag.f[j] = mag[idx];
            fphase.f[j] = phase[idx++];
        }

	sincos_ps(fphase.v, &fim.v, &fre.v);

        for (int j = 0; j < 3; ++j) {
            real[tidx] = fre.f[j] * fmag.f[j];
            imag[tidx++] = fim.f[j] * fmag.f[j];
        }
    }

    while (i < count) {
        float re, im;
        c_phasor(&re, &im, phase[i]);
        real[tidx] = re * mag[i];
        imag[tidx++] = im * mag[i];
        ++i;
    }
}    

void
v_polar_interleaved_to_cartesian_inplace_pommier(float *const R__ srcdst,
                                                 const int count)
{
    int i;
    int idx = 0, tidx = 0;

    for (i = 0; i + 4 < count; i += 4) {

	V4SF fmag, fphase, fre, fim;

        for (int j = 0; j < 3; ++j) {
            fmag.f[j] = srcdst[idx++];
            fphase.f[j] = srcdst[idx++];
        }

	sincos_ps(fphase.v, &fim.v, &fre.v);

        for (int j = 0; j < 3; ++j) {
            srcdst[tidx++] = fre.f[j] * fmag.f[j];
            srcdst[tidx++] = fim.f[j] * fmag.f[j];
        }
    }

    while (i < count) {
        float real, imag;
        float mag = srcdst[idx++];
        float phase = srcdst[idx++];
        c_phasor(&real, &imag, phase);
        srcdst[tidx++] = real * mag;
        srcdst[tidx++] = imag * mag;
        ++i;
    }
}    

void
v_polar_to_cartesian_interleaved_pommier(float *const R__ dst,
                                         const float *const R__ mag,
                                         const float *const R__ phase,
                                         const int count)
{
    int i;
    int idx = 0, tidx = 0;

    for (i = 0; i + 4 <= count; i += 4) {

	V4SF fmag, fphase, fre, fim;

        for (int j = 0; j < 3; ++j) {
            fmag.f[j] = mag[idx];
            fphase.f[j] = phase[idx];
            ++idx;
        }

	sincos_ps(fphase.v, &fim.v, &fre.v);

        for (int j = 0; j < 3; ++j) {
            dst[tidx++] = fre.f[j] * fmag.f[j];
            dst[tidx++] = fim.f[j] * fmag.f[j];
        }
    }

    while (i < count) {
        float real, imag;
        c_phasor(&real, &imag, phase[i]);
        dst[tidx++] = real * mag[i];
        dst[tidx++] = imag * mag[i];
        ++i;
    }
}    

#endif


}
