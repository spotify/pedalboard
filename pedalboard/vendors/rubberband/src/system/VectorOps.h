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

#ifndef RUBBERBAND_VECTOR_OPS_H
#define RUBBERBAND_VECTOR_OPS_H

#ifdef HAVE_IPP
#ifndef _MSC_VER
#include <inttypes.h>
#endif
#include <ippversion.h>
#include <ipps.h>
#if (IPP_VERSION_MAJOR <= 7)
// Deprecated in v8, removed in v9
#include <ippac.h>
#endif
#endif

#ifdef HAVE_VDSP
#include <Accelerate/Accelerate.h>
#include <alloca.h>
#endif

#include <cstring>
#include "sysutils.h"

namespace RubberBand {

// Note that all functions with a "target" vector have their arguments
// in the same order as memcpy and friends, i.e. target vector first.
// This is the reverse order from the IPP functions.

// The ideal here is to write the basic loops in such a way as to be
// auto-vectorizable by a sensible compiler (definitely gcc-4.3 on
// Linux, ideally also gcc-4.0 on OS/X).

template<typename T>
inline void v_zero(T *const R__ ptr,
                   const int count)
{
    const T value = T(0);
    for (int i = 0; i < count; ++i) {
        ptr[i] = value;
    }
}

#if defined HAVE_IPP
template<> 
inline void v_zero(float *const R__ ptr, 
                   const int count)
{
    ippsZero_32f(ptr, count);
}
template<> 
inline void v_zero(double *const R__ ptr,
                   const int count)
{
    ippsZero_64f(ptr, count);
}
#elif defined HAVE_VDSP
template<> 
inline void v_zero(float *const R__ ptr, 
                   const int count)
{
    vDSP_vclr(ptr, 1, count);
}
template<> 
inline void v_zero(double *const R__ ptr,
                   const int count)
{
    vDSP_vclrD(ptr, 1, count);
}
#endif

template<typename T>
inline void v_zero_channels(T *const R__ *const R__ ptr,
                            const int channels,
                            const int count)
{
    for (int c = 0; c < channels; ++c) {
        v_zero(ptr[c], count);
    }
}

template<typename T>
inline void v_set(T *const R__ ptr,
                  const T value,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        ptr[i] = value;
    }
}

template<typename T>
inline void v_copy(T *const R__ dst,
                   const T *const R__ src,
                   const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

#if defined HAVE_IPP
template<>
inline void v_copy(float *const R__ dst,
                   const float *const R__ src,
                   const int count)
{
    ippsCopy_32f(src, dst, count);
}
template<>
inline void v_copy(double *const R__ dst,
                   const double *const R__ src,
                   const int count)
{
    ippsCopy_64f(src, dst, count);
}
#endif

template<typename T>
inline void v_copy_channels(T *const R__ *const R__ dst,
                            const T *const R__ *const R__ src,
                            const int channels,
                            const int count)
{
    for (int c = 0; c < channels; ++c) {
        v_copy(dst[c], src[c], count);
    }
}

// src and dst alias by definition, so not restricted
template<typename T>
inline void v_move(T *const dst,
                   const T *const src,
                   const int count)
{
    memmove(dst, src, count * sizeof(T));
}

#if defined HAVE_IPP
template<>
inline void v_move(float *const dst,
                   const float *const src,
                   const int count)
{
    ippsMove_32f(src, dst, count);
}
template<>
inline void v_move(double *const dst,
                   const double *const src,
                   const int count)
{
    ippsMove_64f(src, dst, count);
}
#endif

template<typename T, typename U>
inline void v_convert(U *const R__ dst,
                      const T *const R__ src,
                      const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = U(src[i]);
    }
}

template<>
inline void v_convert(float *const R__ dst,
                      const float *const R__ src,
                      const int count)
{
    v_copy(dst, src, count);
}
template<>
inline void v_convert(double *const R__ dst,
                      const double *const R__ src,
                      const int count)
{
    v_copy(dst, src, count);
}

#if defined HAVE_IPP
template<>
inline void v_convert(double *const R__ dst,
                      const float *const R__ src,
                      const int count)
{
    ippsConvert_32f64f(src, dst, count);
}
template<>
inline void v_convert(float *const R__ dst,
                      const double *const R__ src,
                      const int count)
{
    ippsConvert_64f32f(src, dst, count);
}
#elif defined HAVE_VDSP
template<>
inline void v_convert(double *const R__ dst,
                      const float *const R__ src,
                      const int count)
{
    vDSP_vspdp((float *)src, 1, dst, 1, count);
}
template<>
inline void v_convert(float *const R__ dst,
                      const double *const R__ src,
                      const int count)
{
    vDSP_vdpsp((double *)src, 1, dst, 1, count);
}
#endif

template<typename T, typename U>
inline void v_convert_channels(U *const R__ *const R__ dst,
                               const T *const R__ *const R__ src,
                               const int channels,
                               const int count)
{
    for (int c = 0; c < channels; ++c) {
        v_convert(dst[c], src[c], count);
    }
}

template<typename T>
inline void v_add(T *const R__ dst,
                  const T *const R__ src,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] += src[i];
    }
}

template<typename T>
inline void v_add(T *const R__ dst,
                  const T value,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] += value;
    }
}

#if defined HAVE_IPP
template<>
inline void v_add(float *const R__ dst,
                  const float *const R__ src,
                  const int count)
{
    ippsAdd_32f_I(src, dst, count);
}    
inline void v_add(double *const R__ dst,
                  const double *const R__ src,
                  const int count)
{
    ippsAdd_64f_I(src, dst, count);
}    
#endif

template<typename T>
inline void v_add_channels(T *const R__ *const R__ dst,
                           const T *const R__ *const R__ src,
                           const int channels, const int count)
{
    for (int c = 0; c < channels; ++c) {
        v_add(dst[c], src[c], count);
    }
}

template<typename T, typename G>
inline void v_add_with_gain(T *const R__ dst,
                            const T *const R__ src,
                            const G gain,
                            const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] += src[i] * gain;
    }
}

template<typename T, typename G>
inline void v_add_channels_with_gain(T *const R__ *const R__ dst,
                                     const T *const R__ *const R__ src,
                                     const G gain,
                                     const int channels,
                                     const int count)
{
    for (int c = 0; c < channels; ++c) {
        v_add_with_gain(dst[c], src[c], gain, count);
    }
}

template<typename T>
inline void v_subtract(T *const R__ dst,
                       const T *const R__ src,
                       const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] -= src[i];
    }
}

#if defined HAVE_IPP
template<>
inline void v_subtract(float *const R__ dst,
                       const float *const R__ src,
                       const int count)
{
    ippsSub_32f_I(src, dst, count);
}    
inline void v_subtract(double *const R__ dst,
                       const double *const R__ src,
                       const int count)
{
    ippsSub_64f_I(src, dst, count);
}    
#endif

template<typename T, typename G>
inline void v_scale(T *const R__ dst,
                    const G gain,
                    const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] *= gain;
    }
}

#if defined HAVE_IPP 
template<>
inline void v_scale(float *const R__ dst,
                    const float gain,
                    const int count)
{
    ippsMulC_32f_I(gain, dst, count);
}
template<>
inline void v_scale(double *const R__ dst,
                    const double gain,
                    const int count)
{
    ippsMulC_64f_I(gain, dst, count);
}
#endif

template<typename T, typename S>
inline void v_multiply(T *const R__ srcdst,
                       const S *const R__ src,
                       const int count)
{
    for (int i = 0; i < count; ++i) {
        srcdst[i] *= src[i];
    }
}

#if defined HAVE_IPP 
template<>
inline void v_multiply(float *const R__ srcdst,
                       const float *const R__ src,
                       const int count)
{
    ippsMul_32f_I(src, srcdst, count);
}
template<>
inline void v_multiply(double *const R__ srcdst,
                       const double *const R__ src,
                       const int count)
{
    ippsMul_64f_I(src, srcdst, count);
}
#endif // HAVE_IPP

template<typename T>
inline void v_multiply(T *const R__ dst,
                       const T *const R__ src1,
                       const T *const R__ src2,
                       const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = src1[i] * src2[i];
    }
}

template<typename T>
inline void v_divide(T *const R__ dst,
                     const T *const R__ src,
                     const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] /= src[i];
    }
}

#if defined HAVE_IPP 
template<>
inline void v_divide(float *const R__ dst,
                     const float *const R__ src,
                     const int count)
{
    ippsDiv_32f_I(src, dst, count);
}
template<>
inline void v_divide(double *const R__ dst,
                     const double *const R__ src,
                     const int count)
{
    ippsDiv_64f_I(src, dst, count);
}
#endif

#if defined HAVE_IPP 
template<>
inline void v_multiply(float *const R__ dst,
                       const float *const R__ src1,
                       const float *const R__ src2,
                       const int count)
{
    ippsMul_32f(src1, src2, dst, count);
}    
template<>
inline void v_multiply(double *const R__ dst,
                       const double *const R__ src1,
                       const double *const R__ src2,
                       const int count)
{
    ippsMul_64f(src1, src2, dst, count);
}
#endif

template<typename T>
inline void v_multiply_and_add(T *const R__ dst,
                               const T *const R__ src1,
                               const T *const R__ src2,
                               const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] += src1[i] * src2[i];
    }
}

#if defined HAVE_IPP
template<>
inline void v_multiply_and_add(float *const R__ dst,
                               const float *const R__ src1,
                               const float *const R__ src2,
                               const int count)
{
    ippsAddProduct_32f(src1, src2, dst, count);
}
template<>
inline void v_multiply_and_add(double *const R__ dst,
                               const double *const R__ src1,
                               const double *const R__ src2,
                               const int count)
{
    ippsAddProduct_64f(src1, src2, dst, count);
}
#endif

template<typename T>
inline T v_sum(const T *const R__ src,
               const int count)
{
    T result = T();
    for (int i = 0; i < count; ++i) {
        result += src[i];
    }
    return result;
}

template<typename T>
inline T v_multiply_and_sum(const T *const R__ src1,
                            const T *const R__ src2,
                            const int count)
{
    T result = T();
    for (int i = 0; i < count; ++i) {
        result += src1[i] * src2[i];
    }
    return result;
}

#if defined HAVE_IPP
template<>
inline float v_multiply_and_sum(const float *const R__ src1,
                                const float *const R__ src2,
                                const int count)
{
    float dp;
    ippsDotProd_32f(src1, src2, count, &dp);
    return dp;
}
template<>
inline double v_multiply_and_sum(const double *const R__ src1,
                                 const double *const R__ src2,
                                 const int count)
{
    double dp;
    ippsDotProd_64f(src1, src2, count, &dp);
    return dp;
}
#elif defined HAVE_VDSP
template<>
inline float v_multiply_and_sum(const float *const R__ src1,
                                const float *const R__ src2,
                                const int count)
{
    float dp;
    vDSP_dotpr(src1, 1, src2, 1, &dp, count);
    return dp;
}
template<>
inline double v_multiply_and_sum(const double *const R__ src1,
                                 const double *const R__ src2,
                                 const int count)
{
    double dp;
    vDSP_dotprD(src1, 1, src2, 1, &dp, count);
    return dp;
}
#endif

template<typename T>
inline void v_log(T *const R__ dst,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = log(dst[i]);
    }
}

#if defined HAVE_IPP
template<>
inline void v_log(float *const R__ dst,
                  const int count)
{
    ippsLn_32f_I(dst, count);
}
template<>
inline void v_log(double *const R__ dst,
                  const int count)
{
    ippsLn_64f_I(dst, count);
}
#elif defined HAVE_VDSP
// no in-place vForce functions for these -- can we use the
// out-of-place functions with equal input and output vectors? can we
// use an out-of-place one with temporary buffer and still be faster
// than doing it any other way?
template<>
inline void v_log(float *const R__ dst,
                  const int count)
{
    float *tmp = (float *)alloca(count * sizeof(float));
    vvlogf(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
template<>
inline void v_log(double *const R__ dst,
                  const int count)
{
    double *tmp = (double *)alloca(count * sizeof(double));
    vvlog(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
#endif

template<typename T>
inline void v_exp(T *const R__ dst,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = exp(dst[i]);
    }
}

#if defined HAVE_IPP
template<>
inline void v_exp(float *const R__ dst,
                  const int count)
{
    ippsExp_32f_I(dst, count);
}
template<>
inline void v_exp(double *const R__ dst,
                  const int count)
{
    ippsExp_64f_I(dst, count);
}
#elif defined HAVE_VDSP
// no in-place vForce functions for these -- can we use the
// out-of-place functions with equal input and output vectors? can we
// use an out-of-place one with temporary buffer and still be faster
// than doing it any other way?
template<>
inline void v_exp(float *const R__ dst,
                  const int count)
{
    float *tmp = (float *)alloca(count * sizeof(float));
    vvexpf(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
template<>
inline void v_exp(double *const R__ dst,
                  const int count)
{
    double *tmp = (double *)alloca(count * sizeof(double));
    vvexp(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
#endif

template<typename T>
inline void v_sqrt(T *const R__ dst,
                   const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = sqrt(dst[i]);
    }
}

#if defined HAVE_IPP
template<>
inline void v_sqrt(float *const R__ dst,
                   const int count)
{
    ippsSqrt_32f_I(dst, count);
}
template<>
inline void v_sqrt(double *const R__ dst,
                   const int count)
{
    ippsSqrt_64f_I(dst, count);
}
#elif defined HAVE_VDSP
// no in-place vForce functions for these -- can we use the
// out-of-place functions with equal input and output vectors? can we
// use an out-of-place one with temporary buffer and still be faster
// than doing it any other way?
template<>
inline void v_sqrt(float *const R__ dst,
                   const int count)
{
    float *tmp = (float *)alloca(count * sizeof(float));
    vvsqrtf(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
template<>
inline void v_sqrt(double *const R__ dst,
                   const int count)
{
    double *tmp = (double *)alloca(count * sizeof(double));
    vvsqrt(tmp, dst, &count);
    v_copy(dst, tmp, count);
}
#endif

template<typename T>
inline void v_square(T *const R__ dst,
                   const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = dst[i] * dst[i];
    }
}

#if defined HAVE_IPP
template<>
inline void v_square(float *const R__ dst,
                   const int count)
{
    ippsSqr_32f_I(dst, count);
}
template<>
inline void v_square(double *const R__ dst,
                   const int count)
{
    ippsSqr_64f_I(dst, count);
}
#endif

template<typename T>
inline void v_abs(T *const R__ dst,
                  const int count)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = fabs(dst[i]);
    }
}

#if defined HAVE_IPP
template<>
inline void v_abs(float *const R__ dst,
                  const int count)
{
    ippsAbs_32f_I(dst, count);
}
template<>
inline void v_abs(double *const R__ dst,
                  const int count)
{
    ippsAbs_64f_I(dst, count);
}
#elif defined HAVE_VDSP
template<>
inline void v_abs(float *const R__ dst,
                  const int count)
{
    float *tmp = (float *)alloca(count * sizeof(float));
#if TARGET_OS_IPHONE
    vvfabsf(tmp, dst, &count);
#elif (defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED < 1070)
    vvfabf(tmp, dst, &count);
#else
    vvfabsf(tmp, dst, &count);
#endif
    v_copy(dst, tmp, count);
}
#endif

template<typename T>
inline void v_interleave(T *const R__ dst,
                         const T *const R__ *const R__ src,
                         const int channels, 
                         const int count)
{
    int idx = 0;
    switch (channels) {
    case 2:
        // common case, may be vectorized by compiler if hardcoded
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < 2; ++j) {
                dst[idx++] = src[j][i];
            }
        }
        return;
    case 1:
        v_copy(dst, src[0], count);
        return;
    default:
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < channels; ++j) {
                dst[idx++] = src[j][i];
            }
        }
    }
}

#if defined HAVE_IPP 
#if (IPP_VERSION_MAJOR <= 7)
// Deprecated in v8, removed in v9
template<>
inline void v_interleave(float *const R__ dst,
                         const float *const R__ *const R__ src,
                         const int channels, 
                         const int count)
{
    ippsInterleave_32f((const Ipp32f **)src, channels, count, dst);
}
// IPP does not (currently?) provide double-precision interleave
#endif
#endif

template<typename T>
inline void v_deinterleave(T *const R__ *const R__ dst,
                           const T *const R__ src,
                           const int channels, 
                           const int count)
{
    int idx = 0;
    switch (channels) {
    case 2:
        // common case, may be vectorized by compiler if hardcoded
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < 2; ++j) {
                dst[j][i] = src[idx++];
            }
        }
        return;
    case 1:
        v_copy(dst[0], src, count);
        return;
    default:
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < channels; ++j) {
                dst[j][i] = src[idx++];
            }
        }
    }
}

#if defined HAVE_IPP
#if (IPP_VERSION_MAJOR <= 7)
// Deprecated in v8, removed in v9
template<>
inline void v_deinterleave(float *const R__ *const R__ dst,
                           const float *const R__ src,
                           const int channels, 
                           const int count)
{
    ippsDeinterleave_32f((const Ipp32f *)src, channels, count, (Ipp32f **)dst);
}
// IPP does not (currently?) provide double-precision deinterleave
#endif
#endif

template<typename T>
inline void v_fftshift(T *const R__ ptr,
                       const int count)
{
    const int hs = count/2;
    for (int i = 0; i < hs; ++i) {
        T t = ptr[i];
        ptr[i] = ptr[i + hs];
        ptr[i + hs] = t;
    }
}

template<typename T>
inline T v_mean(const T *const R__ ptr, const int count)
{
    T t = T(0);
    for (int i = 0; i < count; ++i) {
        t += ptr[i];
    }
    t /= T(count);
    return t;
}

template<typename T>
inline T v_mean_channels(const T *const R__ *const R__ ptr,
                         const int channels,
                         const int count)
{
    T t = T(0);
    for (int c = 0; c < channels; ++c) {
        t += v_mean(ptr[c], count);
    }
    t /= T(channels);
    return t;
}

}

#endif
