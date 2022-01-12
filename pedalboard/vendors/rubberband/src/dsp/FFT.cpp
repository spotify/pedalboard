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

#include "FFT.h"
#include "../system/Thread.h"
#include "../base/Profiler.h"
#include "../system/Allocators.h"
#include "../system/VectorOps.h"
#include "../system/VectorOpsComplex.h"

// Define USE_FFTW_WISDOM if you are defining HAVE_FFTW3 and you want
// to use FFTW_MEASURE mode with persistent wisdom files. This will
// make things much slower on first use if no suitable wisdom has been
// saved, but may be faster during subsequent use.
//#define USE_FFTW_WISDOM 1

// Define FFT_MEASUREMENT to include timing measurement code callable
// via the static method FFT::tune(). Must be defined when the header
// is included as well.
//#define FFT_MEASUREMENT 1

#ifdef FFT_MEASUREMENT
#define FFT_MEASUREMENT_RETURN_RESULT_TEXT 1
#include <sstream>
#endif

#ifdef HAVE_IPP
#include <ippversion.h>
#include <ipps.h>
#endif

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#ifdef HAVE_VDSP
#include <Accelerate/Accelerate.h>
#endif

#ifdef HAVE_KISSFFT
#include "kiss_fftr.h"
#endif

#ifndef HAVE_IPP
#ifndef HAVE_FFTW3
#ifndef HAVE_KISSFFT
#ifndef USE_BUILTIN_FFT
#ifndef HAVE_VDSP
#error No FFT implementation selected!
#endif
#endif
#endif
#endif
#endif

#include <cmath>
#include <iostream>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef FFT_MEASUREMENT
#ifndef _WIN32
#include <unistd.h>
#endif
#endif

#define BQ_R__ R__

namespace RubberBand {

class FFTImpl
{
public:
    virtual ~FFTImpl() { }

    virtual FFT::Precisions getSupportedPrecisions() const = 0;

    virtual int getSize() const = 0;
    
    virtual void initFloat() = 0;
    virtual void initDouble() = 0;

    virtual void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) = 0;
    virtual void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) = 0;
    virtual void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) = 0;
    virtual void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) = 0;

    virtual void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) = 0;
    virtual void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) = 0;
    virtual void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) = 0;
    virtual void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) = 0;

    virtual void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) = 0;
    virtual void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) = 0;
    virtual void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) = 0;
    virtual void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) = 0;

    virtual void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) = 0;
    virtual void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) = 0;
    virtual void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) = 0;
    virtual void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) = 0;
};    

namespace FFTs {

#ifdef HAVE_IPP

class D_IPP : public FFTImpl
{
public:
    D_IPP(int size) :
        m_size(size), m_fspec(0), m_dspec(0)
    { 
        for (int i = 0; ; ++i) {
            if (m_size & (1 << i)) {
                m_order = i;
                break;
            }
        }
    }

    ~D_IPP() {
        if (m_fspec) {
#if (IPP_VERSION_MAJOR >= 9)
            ippsFree(m_fspecbuf);
#else
            ippsFFTFree_R_32f(m_fspec);
#endif
            ippsFree(m_fbuf);
            ippsFree(m_fpacked);
            ippsFree(m_fspare);
        }
        if (m_dspec) {
#if (IPP_VERSION_MAJOR >= 9)
            ippsFree(m_dspecbuf);
#else
            ippsFFTFree_R_64f(m_dspec);
#endif
            ippsFree(m_dbuf);
            ippsFree(m_dpacked);
            ippsFree(m_dspare);
        }
    }

    int getSize() const {
        return m_size;
    }
    
    FFT::Precisions
    getSupportedPrecisions() const {
        return FFT::SinglePrecision | FFT::DoublePrecision;
    }

    //!!! rv check

    void initFloat() {
        if (m_fspec) return;
#if (IPP_VERSION_MAJOR >= 9)
        int specSize, specBufferSize, bufferSize;
        ippsFFTGetSize_R_32f(m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                             &specSize, &specBufferSize, &bufferSize);
        m_fspecbuf = ippsMalloc_8u(specSize);
        Ipp8u *tmp = ippsMalloc_8u(specBufferSize);
        m_fbuf = ippsMalloc_8u(bufferSize);
        m_fpacked = ippsMalloc_32f(m_size + 2);
        m_fspare = ippsMalloc_32f(m_size / 2 + 1);
        ippsFFTInit_R_32f(&m_fspec,
                          m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                          m_fspecbuf, tmp);
        ippsFree(tmp);
#else
        int specSize, specBufferSize, bufferSize;
        ippsFFTGetSize_R_32f(m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                             &specSize, &specBufferSize, &bufferSize);
        m_fbuf = ippsMalloc_8u(bufferSize);
        m_fpacked = ippsMalloc_32f(m_size + 2);
        m_fspare = ippsMalloc_32f(m_size / 2 + 1);
        ippsFFTInitAlloc_R_32f(&m_fspec, m_order, IPP_FFT_NODIV_BY_ANY, 
                               ippAlgHintFast);
#endif
    }

    void initDouble() {
        if (m_dspec) return;
#if (IPP_VERSION_MAJOR >= 9)
        int specSize, specBufferSize, bufferSize;
        ippsFFTGetSize_R_64f(m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                             &specSize, &specBufferSize, &bufferSize);
        m_dspecbuf = ippsMalloc_8u(specSize);
        Ipp8u *tmp = ippsMalloc_8u(specBufferSize);
        m_dbuf = ippsMalloc_8u(bufferSize);
        m_dpacked = ippsMalloc_64f(m_size + 2);
        m_dspare = ippsMalloc_64f(m_size / 2 + 1);
        ippsFFTInit_R_64f(&m_dspec,
                          m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                          m_dspecbuf, tmp);
        ippsFree(tmp);
#else
        int specSize, specBufferSize, bufferSize;
        ippsFFTGetSize_R_64f(m_order, IPP_FFT_NODIV_BY_ANY, ippAlgHintFast,
                             &specSize, &specBufferSize, &bufferSize);
        m_dbuf = ippsMalloc_8u(bufferSize);
        m_dpacked = ippsMalloc_64f(m_size + 2);
        m_dspare = ippsMalloc_64f(m_size / 2 + 1);
        ippsFFTInitAlloc_R_64f(&m_dspec, m_order, IPP_FFT_NODIV_BY_ANY, 
                               ippAlgHintFast);
#endif
    }

    void packFloat(const float *BQ_R__ re, const float *BQ_R__ im) {
        int index = 0;
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[index++] = re[i];
            index++;
        }
        index = 0;
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                index++;
                m_fpacked[index++] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                index++;
                m_fpacked[index++] = 0.f;
            }
        }
    }

    void packDouble(const double *BQ_R__ re, const double *BQ_R__ im) {
        int index = 0;
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_dpacked[index++] = re[i];
            index++;
        }
        index = 0;
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                index++;
                m_dpacked[index++] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                index++;
                m_dpacked[index++] = 0.0;
            }
        }
    }

    void unpackFloat(float *re, float *BQ_R__ im) { // re may be equal to m_fpacked
        int index = 0;
        const int hs = m_size/2;
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                index++;
                im[i] = m_fpacked[index++];
            }
        }
        index = 0;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_fpacked[index++];
            index++;
        }
    }        

    void unpackDouble(double *re, double *BQ_R__ im) { // re may be equal to m_dpacked
        int index = 0;
        const int hs = m_size/2;
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                index++;
                im[i] = m_dpacked[index++];
            }
        }
        index = 0;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_dpacked[index++];
            index++;
        }
    }        

    void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        if (!m_dspec) initDouble();
        ippsFFTFwd_RToCCS_64f(realIn, m_dpacked, m_dspec, m_dbuf);
        unpackDouble(realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) {
        if (!m_dspec) initDouble();
        ippsFFTFwd_RToCCS_64f(realIn, complexOut, m_dspec, m_dbuf);
    }

    void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        if (!m_dspec) initDouble();
        ippsFFTFwd_RToCCS_64f(realIn, m_dpacked, m_dspec, m_dbuf);
        unpackDouble(m_dpacked, m_dspare);
        ippsCartToPolar_64f(m_dpacked, m_dspare, magOut, phaseOut, m_size/2+1);
    }

    void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) {
        if (!m_dspec) initDouble();
        ippsFFTFwd_RToCCS_64f(realIn, m_dpacked, m_dspec, m_dbuf);
        unpackDouble(m_dpacked, m_dspare);
        ippsMagnitude_64f(m_dpacked, m_dspare, magOut, m_size/2+1);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) {
        if (!m_fspec) initFloat();
        ippsFFTFwd_RToCCS_32f(realIn, m_fpacked, m_fspec, m_fbuf);
        unpackFloat(realOut, imagOut);
    }

    void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) {
        if (!m_fspec) initFloat();
        ippsFFTFwd_RToCCS_32f(realIn, complexOut, m_fspec, m_fbuf);
    }

    void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        if (!m_fspec) initFloat();
        ippsFFTFwd_RToCCS_32f(realIn, m_fpacked, m_fspec, m_fbuf);
        unpackFloat(m_fpacked, m_fspare);
        ippsCartToPolar_32f(m_fpacked, m_fspare, magOut, phaseOut, m_size/2+1);
    }

    void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) {
        if (!m_fspec) initFloat();
        ippsFFTFwd_RToCCS_32f(realIn, m_fpacked, m_fspec, m_fbuf);
        unpackFloat(m_fpacked, m_fspare);
        ippsMagnitude_32f(m_fpacked, m_fspare, magOut, m_size/2+1);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        packDouble(realIn, imagIn);
        ippsFFTInv_CCSToR_64f(m_dpacked, realOut, m_dspec, m_dbuf);
    }

    void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        ippsFFTInv_CCSToR_64f(complexIn, realOut, m_dspec, m_dbuf);
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        ippsPolarToCart_64f(magIn, phaseIn, realOut, m_dspare, m_size/2+1);
        packDouble(realOut, m_dspare); // to m_dpacked
        ippsFFTInv_CCSToR_64f(m_dpacked, realOut, m_dspec, m_dbuf);
    }

    void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) {
        if (!m_dspec) initDouble();
        const int hs1 = m_size/2 + 1;
        ippsCopy_64f(magIn, m_dspare, hs1);
        ippsAddC_64f_I(0.000001, m_dspare, hs1);
        ippsLn_64f_I(m_dspare, hs1);
        packDouble(m_dspare, 0);
        ippsFFTInv_CCSToR_64f(m_dpacked, cepOut, m_dspec, m_dbuf);
    }
    
    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();
        packFloat(realIn, imagIn);
        ippsFFTInv_CCSToR_32f(m_fpacked, realOut, m_fspec, m_fbuf);
    }

    void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();
        ippsFFTInv_CCSToR_32f(complexIn, realOut, m_fspec, m_fbuf);
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();
        ippsPolarToCart_32f(magIn, phaseIn, realOut, m_fspare, m_size/2+1);
        packFloat(realOut, m_fspare); // to m_fpacked
        ippsFFTInv_CCSToR_32f(m_fpacked, realOut, m_fspec, m_fbuf);
    }

    void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) {
        if (!m_fspec) initFloat();
        const int hs1 = m_size/2 + 1;
        ippsCopy_32f(magIn, m_fspare, hs1);
        ippsAddC_32f_I(0.000001f, m_fspare, hs1);
        ippsLn_32f_I(m_fspare, hs1);
        packFloat(m_fspare, 0);
        ippsFFTInv_CCSToR_32f(m_fpacked, cepOut, m_fspec, m_fbuf);
    }

private:
    const int m_size;
    int m_order;
    IppsFFTSpec_R_32f *m_fspec;
    IppsFFTSpec_R_64f *m_dspec;
    Ipp8u *m_fspecbuf;
    Ipp8u *m_dspecbuf;
    Ipp8u *m_fbuf;
    Ipp8u *m_dbuf;
    float *m_fpacked;
    float *m_fspare;
    double *m_dpacked;
    double *m_dspare;
};

#endif /* HAVE_IPP */

#ifdef HAVE_VDSP

class D_VDSP : public FFTImpl
{
public:
    D_VDSP(int size) :
        m_size(size), m_fspec(0), m_dspec(0),
        m_fpacked(0), m_fspare(0),
        m_dpacked(0), m_dspare(0)
    { 
        for (int i = 0; ; ++i) {
            if (m_size & (1 << i)) {
                m_order = i;
                break;
            }
        }
    }

    ~D_VDSP() {
        if (m_fspec) {
            vDSP_destroy_fftsetup(m_fspec);
            deallocate(m_fspare);
            deallocate(m_fspare2);
            deallocate(m_fbuf->realp);
            deallocate(m_fbuf->imagp);
            delete m_fbuf;
            deallocate(m_fpacked->realp);
            deallocate(m_fpacked->imagp);
            delete m_fpacked;
        }
        if (m_dspec) {
            vDSP_destroy_fftsetupD(m_dspec);
            deallocate(m_dspare);
            deallocate(m_dspare2);
            deallocate(m_dbuf->realp);
            deallocate(m_dbuf->imagp);
            delete m_dbuf;
            deallocate(m_dpacked->realp);
            deallocate(m_dpacked->imagp);
            delete m_dpacked;
        }
    }

    int getSize() const {
        return m_size;
    }

    FFT::Precisions
    getSupportedPrecisions() const {
        return FFT::SinglePrecision | FFT::DoublePrecision;
    }

    //!!! rv check

    void initFloat() {
        if (m_fspec) return;
        m_fspec = vDSP_create_fftsetup(m_order, FFT_RADIX2);
        m_fbuf = new DSPSplitComplex;
        //!!! "If possible, tempBuffer->realp and tempBuffer->imagp should be 32-byte aligned for best performance."
        m_fbuf->realp = allocate<float>(m_size);
        m_fbuf->imagp = allocate<float>(m_size);
        m_fpacked = new DSPSplitComplex;
        m_fpacked->realp = allocate<float>(m_size / 2 + 1);
        m_fpacked->imagp = allocate<float>(m_size / 2 + 1);
        m_fspare = allocate<float>(m_size + 2);
        m_fspare2 = allocate<float>(m_size + 2);
    }

    void initDouble() {
        if (m_dspec) return;
        m_dspec = vDSP_create_fftsetupD(m_order, FFT_RADIX2);
        m_dbuf = new DSPDoubleSplitComplex;
        //!!! "If possible, tempBuffer->realp and tempBuffer->imagp should be 32-byte aligned for best performance."
        m_dbuf->realp = allocate<double>(m_size);
        m_dbuf->imagp = allocate<double>(m_size);
        m_dpacked = new DSPDoubleSplitComplex;
        m_dpacked->realp = allocate<double>(m_size / 2 + 1);
        m_dpacked->imagp = allocate<double>(m_size / 2 + 1);
        m_dspare = allocate<double>(m_size + 2);
        m_dspare2 = allocate<double>(m_size + 2);
    }

    void packReal(const float *BQ_R__ const re) {
        // Pack input for forward transform 
        vDSP_ctoz((DSPComplex *)re, 2, m_fpacked, 1, m_size/2);
    }
    void packComplex(const float *BQ_R__ const re, const float *BQ_R__ const im) {
        // Pack input for inverse transform 
        if (re) v_copy(m_fpacked->realp, re, m_size/2 + 1);
        else v_zero(m_fpacked->realp, m_size/2 + 1);
        if (im) v_copy(m_fpacked->imagp, im, m_size/2 + 1);
        else v_zero(m_fpacked->imagp, m_size/2 + 1);
        fnyq();
    }

    void unpackReal(float *BQ_R__ const re) {
        // Unpack output for inverse transform
        vDSP_ztoc(m_fpacked, 1, (DSPComplex *)re, 2, m_size/2);
    }
    void unpackComplex(float *BQ_R__ const re, float *BQ_R__ const im) {
        // Unpack output for forward transform
        // vDSP forward FFTs are scaled 2x (for some reason)
        float two = 2.f;
        vDSP_vsdiv(m_fpacked->realp, 1, &two, re, 1, m_size/2 + 1);
        vDSP_vsdiv(m_fpacked->imagp, 1, &two, im, 1, m_size/2 + 1);
    }
    void unpackComplex(float *BQ_R__ const cplx) {
        // Unpack output for forward transform
        // vDSP forward FFTs are scaled 2x (for some reason)
        const int hs1 = m_size/2 + 1;
        for (int i = 0; i < hs1; ++i) {
            cplx[i*2] = m_fpacked->realp[i] * 0.5f;
            cplx[i*2+1] = m_fpacked->imagp[i] * 0.5f;
        }
    }

    void packReal(const double *BQ_R__ const re) {
        // Pack input for forward transform
        vDSP_ctozD((DSPDoubleComplex *)re, 2, m_dpacked, 1, m_size/2);
    }
    void packComplex(const double *BQ_R__ const re, const double *BQ_R__ const im) {
        // Pack input for inverse transform
        if (re) v_copy(m_dpacked->realp, re, m_size/2 + 1);
        else v_zero(m_dpacked->realp, m_size/2 + 1);
        if (im) v_copy(m_dpacked->imagp, im, m_size/2 + 1);
        else v_zero(m_dpacked->imagp, m_size/2 + 1);
        dnyq();
    }

    void unpackReal(double *BQ_R__ const re) {
        // Unpack output for inverse transform
        vDSP_ztocD(m_dpacked, 1, (DSPDoubleComplex *)re, 2, m_size/2);
    }
    void unpackComplex(double *BQ_R__ const re, double *BQ_R__ const im) {
        // Unpack output for forward transform
        // vDSP forward FFTs are scaled 2x (for some reason)
        double two = 2.0;
        vDSP_vsdivD(m_dpacked->realp, 1, &two, re, 1, m_size/2 + 1);
        vDSP_vsdivD(m_dpacked->imagp, 1, &two, im, 1, m_size/2 + 1);
    }
    void unpackComplex(double *BQ_R__ const cplx) {
        // Unpack output for forward transform
        // vDSP forward FFTs are scaled 2x (for some reason)
        const int hs1 = m_size/2 + 1;
        for (int i = 0; i < hs1; ++i) {
            cplx[i*2] = m_dpacked->realp[i] * 0.5;
            cplx[i*2+1] = m_dpacked->imagp[i] * 0.5;
        }
    }

    void fdenyq() {
        // for fft result in packed form, unpack the DC and Nyquist bins
        const int hs = m_size/2;
        m_fpacked->realp[hs] = m_fpacked->imagp[0];
        m_fpacked->imagp[hs] = 0.f;
        m_fpacked->imagp[0] = 0.f;
    }
    void ddenyq() {
        // for fft result in packed form, unpack the DC and Nyquist bins
        const int hs = m_size/2;
        m_dpacked->realp[hs] = m_dpacked->imagp[0];
        m_dpacked->imagp[hs] = 0.;
        m_dpacked->imagp[0] = 0.;
    }

    void fnyq() {
        // for ifft input in packed form, pack the DC and Nyquist bins
        const int hs = m_size/2;
        m_fpacked->imagp[0] = m_fpacked->realp[hs];
        m_fpacked->realp[hs] = 0.f;
        m_fpacked->imagp[hs] = 0.f;
    }
    void dnyq() {
        // for ifft input in packed form, pack the DC and Nyquist bins
        const int hs = m_size/2;
        m_dpacked->imagp[0] = m_dpacked->realp[hs];
        m_dpacked->realp[hs] = 0.;
        m_dpacked->imagp[hs] = 0.;
    }

    void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        if (!m_dspec) initDouble();
        packReal(realIn);
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_FORWARD);
        ddenyq();
        unpackComplex(realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) {
        if (!m_dspec) initDouble();
        packReal(realIn);
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_FORWARD);
        ddenyq();
        unpackComplex(complexOut);
    }

    void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        if (!m_dspec) initDouble();
        const int hs1 = m_size/2+1;
        packReal(realIn);
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_FORWARD);
        ddenyq();
        // vDSP forward FFTs are scaled 2x (for some reason)
        for (int i = 0; i < hs1; ++i) m_dpacked->realp[i] *= 0.5;
        for (int i = 0; i < hs1; ++i) m_dpacked->imagp[i] *= 0.5;
        v_cartesian_to_polar(magOut, phaseOut,
                             m_dpacked->realp, m_dpacked->imagp, hs1);
    }

    void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) {
        if (!m_dspec) initDouble();
        packReal(realIn);
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_FORWARD);
        ddenyq();
        const int hs1 = m_size/2+1;
        vDSP_zvmagsD(m_dpacked, 1, m_dspare, 1, hs1);
        vvsqrt(m_dspare2, m_dspare, &hs1);
        // vDSP forward FFTs are scaled 2x (for some reason)
        double two = 2.0;
        vDSP_vsdivD(m_dspare2, 1, &two, magOut, 1, hs1);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) {
        if (!m_fspec) initFloat();
        packReal(realIn);
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_FORWARD);
        fdenyq();
        unpackComplex(realOut, imagOut);
    }

    void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) {
        if (!m_fspec) initFloat();
        packReal(realIn);
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_FORWARD);
        fdenyq();
        unpackComplex(complexOut);
    }

    void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        if (!m_fspec) initFloat();
        const int hs1 = m_size/2+1;
        packReal(realIn);
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_FORWARD);
        fdenyq();
        // vDSP forward FFTs are scaled 2x (for some reason)
        for (int i = 0; i < hs1; ++i) m_fpacked->realp[i] *= 0.5f;
        for (int i = 0; i < hs1; ++i) m_fpacked->imagp[i] *= 0.5f;
        v_cartesian_to_polar(magOut, phaseOut,
                             m_fpacked->realp, m_fpacked->imagp, hs1);
    }

    void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) {
        if (!m_fspec) initFloat();
        packReal(realIn);
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_FORWARD);
        fdenyq();
        const int hs1 = m_size/2 + 1;
        vDSP_zvmags(m_fpacked, 1, m_fspare, 1, hs1);
        vvsqrtf(m_fspare2, m_fspare, &hs1);
        // vDSP forward FFTs are scaled 2x (for some reason)
        float two = 2.f;
        vDSP_vsdiv(m_fspare2, 1, &two, magOut, 1, hs1);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        packComplex(realIn, imagIn);
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        double *d[2] = { m_dpacked->realp, m_dpacked->imagp };
        v_deinterleave(d, complexIn, 2, m_size/2 + 1);
        dnyq();
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) {
        if (!m_dspec) initDouble();
        const int hs1 = m_size/2+1;
        vvsincos(m_dpacked->imagp, m_dpacked->realp, phaseIn, &hs1);
        double *const rp = m_dpacked->realp;
        double *const ip = m_dpacked->imagp;
        for (int i = 0; i < hs1; ++i) rp[i] *= magIn[i];
        for (int i = 0; i < hs1; ++i) ip[i] *= magIn[i];
        dnyq();
        vDSP_fft_zriptD(m_dspec, m_dpacked, 1, m_dbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) {
        if (!m_dspec) initDouble();
        const int hs1 = m_size/2 + 1;
        v_copy(m_dspare, magIn, hs1);
        for (int i = 0; i < hs1; ++i) m_dspare[i] += 0.000001;
        vvlog(m_dspare2, m_dspare, &hs1);
        inverse(m_dspare2, 0, cepOut);
    }
    
    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();
        packComplex(realIn, imagIn);
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();
        float *f[2] = { m_fpacked->realp, m_fpacked->imagp };
        v_deinterleave(f, complexIn, 2, m_size/2 + 1);
        fnyq();
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) {
        if (!m_fspec) initFloat();

        const int hs1 = m_size/2+1;
        vvsincosf(m_fpacked->imagp, m_fpacked->realp, phaseIn, &hs1);
        float *const rp = m_fpacked->realp;
        float *const ip = m_fpacked->imagp;
        for (int i = 0; i < hs1; ++i) rp[i] *= magIn[i];
        for (int i = 0; i < hs1; ++i) ip[i] *= magIn[i];
        fnyq();
        vDSP_fft_zript(m_fspec, m_fpacked, 1, m_fbuf, m_order, FFT_INVERSE);
        unpackReal(realOut);
    }

    void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) {
        if (!m_fspec) initFloat();
        const int hs1 = m_size/2 + 1;
        v_copy(m_fspare, magIn, hs1);
        for (int i = 0; i < hs1; ++i) m_fspare[i] += 0.000001f;
        vvlogf(m_fspare2, m_fspare, &hs1);
        inverse(m_fspare2, 0, cepOut);
    }

private:
    const int m_size;
    int m_order;
    FFTSetup m_fspec;
    FFTSetupD m_dspec;
    DSPSplitComplex *m_fbuf;
    DSPDoubleSplitComplex *m_dbuf;
    DSPSplitComplex *m_fpacked;
    float *m_fspare;
    float *m_fspare2;
    DSPDoubleSplitComplex *m_dpacked;
    double *m_dspare;
    double *m_dspare2;
};

#endif /* HAVE_VDSP */

#ifdef HAVE_FFTW3

/*
 Define FFTW_DOUBLE_ONLY to make all uses of FFTW functions be
 double-precision (so "float" FFTs are calculated by casting to
 doubles and using the double-precision FFTW function).

 Define FFTW_SINGLE_ONLY to make all uses of FFTW functions be
 single-precision (so "double" FFTs are calculated by casting to
 floats and using the single-precision FFTW function).

 Neither of these flags is desirable for either performance or
 precision. The main reason to define either flag is to avoid linking
 against both fftw3 and fftw3f libraries.
*/

//#define FFTW_DOUBLE_ONLY 1
//#define FFTW_SINGLE_ONLY 1

#if defined(FFTW_DOUBLE_ONLY) && defined(FFTW_SINGLE_ONLY)
// Can't meaningfully define both
#error Can only define one of FFTW_DOUBLE_ONLY and FFTW_SINGLE_ONLY
#endif

#if defined(FFTW_FLOAT_ONLY)
#warning FFTW_FLOAT_ONLY is deprecated, use FFTW_SINGLE_ONLY instead
#define FFTW_SINGLE_ONLY 1
#endif

#ifdef FFTW_DOUBLE_ONLY
#define fft_float_type double
#define fftwf_complex fftw_complex
#define fftwf_plan fftw_plan
#define fftwf_plan_dft_r2c_1d fftw_plan_dft_r2c_1d
#define fftwf_plan_dft_c2r_1d fftw_plan_dft_c2r_1d
#define fftwf_destroy_plan fftw_destroy_plan
#define fftwf_malloc fftw_malloc
#define fftwf_free fftw_free
#define fftwf_execute fftw_execute
#define atan2f atan2
#define sqrtf sqrt
#define cosf cos
#define sinf sin
#else
#define fft_float_type float
#endif /* FFTW_DOUBLE_ONLY */

#ifdef FFTW_SINGLE_ONLY
#define fft_double_type float
#define fftw_complex fftwf_complex
#define fftw_plan fftwf_plan
#define fftw_plan_dft_r2c_1d fftwf_plan_dft_r2c_1d
#define fftw_plan_dft_c2r_1d fftwf_plan_dft_c2r_1d
#define fftw_destroy_plan fftwf_destroy_plan
#define fftw_malloc fftwf_malloc
#define fftw_free fftwf_free
#define fftw_execute fftwf_execute
#define atan2 atan2f
#define sqrt sqrtf
#define cos cosf
#define sin sinf
#else
#define fft_double_type double
#endif /* FFTW_SINGLE_ONLY */

class D_FFTW : public FFTImpl
{
public:
    D_FFTW(int size) :
        m_fplanf(0), m_dplanf(0), m_size(size)
    {
    }

    ~D_FFTW() {
        if (m_fplanf) {
            lock();
            bool save = false;
            if (m_extantf > 0 && --m_extantf == 0) save = true;
            (void)save; // avoid compiler warning
#ifdef USE_FFTW_WISDOM
#ifndef FFTW_DOUBLE_ONLY
            if (save) saveWisdom('f');
#endif
#endif
            fftwf_destroy_plan(m_fplanf);
            fftwf_destroy_plan(m_fplani);
            fftwf_free(m_fbuf);
            fftwf_free(m_fpacked);
            unlock();
        }
        if (m_dplanf) {
            lock();
            bool save = false;
            if (m_extantd > 0 && --m_extantd == 0) save = true;
            (void)save; // avoid compiler warning
#ifdef USE_FFTW_WISDOM
#ifndef FFTW_SINGLE_ONLY
            if (save) saveWisdom('d');
#endif
#endif
            fftw_destroy_plan(m_dplanf);
            fftw_destroy_plan(m_dplani);
            fftw_free(m_dbuf);
            fftw_free(m_dpacked);
            unlock();
        }
        lock();
        if (m_extantf <= 0 && m_extantd <= 0) {
#ifndef FFTW_DOUBLE_ONLY
            fftwf_cleanup();
#endif
#ifndef FFTW_SINGLE_ONLY
            fftw_cleanup();
#endif
        }
        unlock();
    }

    int getSize() const {
        return m_size;
    }

    FFT::Precisions
    getSupportedPrecisions() const {
#ifdef FFTW_SINGLE_ONLY
        return FFT::SinglePrecision;
#else
#ifdef FFTW_DOUBLE_ONLY
        return FFT::DoublePrecision;
#else
        return FFT::SinglePrecision | FFT::DoublePrecision;
#endif
#endif
    }

    void initFloat() {
        if (m_fplanf) return;
        bool load = false;
        lock();
        if (m_extantf++ == 0) load = true;
        (void)load; // avoid compiler warning
#ifdef USE_FFTW_WISDOM
#ifdef FFTW_DOUBLE_ONLY
        if (load) loadWisdom('d');
#else
        if (load) loadWisdom('f');
#endif
#endif
        m_fbuf = (fft_float_type *)fftw_malloc(m_size * sizeof(fft_float_type));
        m_fpacked = (fftwf_complex *)fftw_malloc
            ((m_size/2 + 1) * sizeof(fftwf_complex));
#ifdef USE_FFTW_WISDOM
        m_fplanf = fftwf_plan_dft_r2c_1d
            (m_size, m_fbuf, m_fpacked, FFTW_MEASURE);
        m_fplani = fftwf_plan_dft_c2r_1d
            (m_size, m_fpacked, m_fbuf, FFTW_MEASURE);
#else
        m_fplanf = fftwf_plan_dft_r2c_1d
            (m_size, m_fbuf, m_fpacked, FFTW_ESTIMATE);
        m_fplani = fftwf_plan_dft_c2r_1d
            (m_size, m_fpacked, m_fbuf, FFTW_ESTIMATE);
#endif
        unlock();
    }

    void initDouble() {
        if (m_dplanf) return;
        bool load = false;
        lock();
        if (m_extantd++ == 0) load = true;
        (void)load; // avoid compiler warning
#ifdef USE_FFTW_WISDOM
#ifdef FFTW_SINGLE_ONLY
        if (load) loadWisdom('f');
#else
        if (load) loadWisdom('d');
#endif
#endif
        m_dbuf = (fft_double_type *)fftw_malloc(m_size * sizeof(fft_double_type));
        m_dpacked = (fftw_complex *)fftw_malloc
            ((m_size/2 + 1) * sizeof(fftw_complex));
#ifdef USE_FFTW_WISDOM
        m_dplanf = fftw_plan_dft_r2c_1d
            (m_size, m_dbuf, m_dpacked, FFTW_MEASURE);
        m_dplani = fftw_plan_dft_c2r_1d
            (m_size, m_dpacked, m_dbuf, FFTW_MEASURE);
#else
        m_dplanf = fftw_plan_dft_r2c_1d
            (m_size, m_dbuf, m_dpacked, FFTW_ESTIMATE);
        m_dplani = fftw_plan_dft_c2r_1d
            (m_size, m_dpacked, m_dbuf, FFTW_ESTIMATE);
#endif
        unlock();
    }

    void loadWisdom(char type) { wisdom(false, type); }
    void saveWisdom(char type) { wisdom(true, type); }

    void wisdom(bool save, char type) {
#ifdef USE_FFTW_WISDOM
#ifdef FFTW_DOUBLE_ONLY
        if (type == 'f') return;
#endif
#ifdef FFTW_SINGLE_ONLY
        if (type == 'd') return;
#endif

        const char *home = getenv("HOME");
        if (!home) return;

        char fn[256];
        snprintf(fn, 256, "%s/%s.%c", home, ".bqfft.wisdom", type);

        FILE *f = fopen(fn, save ? "wb" : "rb");
        if (!f) return;

        if (save) {
            switch (type) {
#ifdef FFTW_DOUBLE_ONLY
            case 'f': break;
#else
            case 'f': fftwf_export_wisdom_to_file(f); break;
#endif
#ifdef FFTW_SINGLE_ONLY
            case 'd': break;
#else
            case 'd': fftw_export_wisdom_to_file(f); break;
#endif
            default: break;
            }
        } else {
            switch (type) {
#ifdef FFTW_DOUBLE_ONLY
            case 'f': break;
#else
            case 'f': fftwf_import_wisdom_from_file(f); break;
#endif
#ifdef FFTW_SINGLE_ONLY
            case 'd': break;
#else
            case 'd': fftw_import_wisdom_from_file(f); break;
#endif
            default: break;
            }
        }

        fclose(f);
#else
        (void)save;
        (void)type;
#endif
    }

    void packFloat(const float *BQ_R__ re, const float *BQ_R__ im) {
        const int hs = m_size/2;
        fftwf_complex *const BQ_R__ fpacked = m_fpacked; 
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][0] = re[i];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                fpacked[i][1] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                fpacked[i][1] = 0.f;
            }
        }                
    }

    void packDouble(const double *BQ_R__ re, const double *BQ_R__ im) {
        const int hs = m_size/2;
        fftw_complex *const BQ_R__ dpacked = m_dpacked; 
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][0] = re[i];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                dpacked[i][1] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                dpacked[i][1] = 0.0;
            }
        }
    }

    void unpackFloat(float *BQ_R__ re, float *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_fpacked[i][0];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = m_fpacked[i][1];
            }
        }
    }        

    void unpackDouble(double *BQ_R__ re, double *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_dpacked[i][0];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = m_dpacked[i][1];
            }
        }
    }        

    void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        if (!m_dplanf) initDouble();
        const int sz = m_size;
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
#ifndef FFTW_SINGLE_ONLY
        if (realIn != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        unpackDouble(realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) {
        if (!m_dplanf) initDouble();
        const int sz = m_size;
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
#ifndef FFTW_SINGLE_ONLY
        if (realIn != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        v_convert(complexOut, (const fft_double_type *)m_dpacked, sz + 2);
    }

    void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
        const int sz = m_size;
#ifndef FFTW_SINGLE_ONLY
        if (realIn != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        v_cartesian_interleaved_to_polar
            (magOut, phaseOut, (const fft_double_type *)m_dpacked, m_size/2+1);
    }

    void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
        const int sz = m_size;
#ifndef FFTW_SINGLE_ONLY
        if (realIn != m_dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        v_cartesian_interleaved_to_magnitudes
            (magOut, (const fft_double_type *)m_dpacked, m_size/2+1);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        unpackFloat(realOut, imagOut);
    }

    void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        v_convert(complexOut, (const fft_float_type *)m_fpacked, sz + 2);
    }

    void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        v_cartesian_interleaved_to_polar
            (magOut, phaseOut, (const fft_float_type *)m_fpacked, m_size/2+1);
    }

    void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        v_cartesian_interleaved_to_magnitudes
            (magOut, (const fft_float_type *)m_fpacked, m_size/2+1);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) {
        if (!m_dplanf) initDouble();
        packDouble(realIn, imagIn);
        fftw_execute(m_dplani);
        const int sz = m_size;
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
#ifndef FFTW_SINGLE_ONLY
        if (realOut != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = dbuf[i];
            }
    }

    void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) {
        if (!m_dplanf) initDouble();
        v_convert((fft_double_type *)m_dpacked, complexIn, m_size + 2);
        fftw_execute(m_dplani);
        const int sz = m_size;
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
#ifndef FFTW_SINGLE_ONLY
        if (realOut != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = dbuf[i];
            }
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) {
        if (!m_dplanf) initDouble();
        v_polar_to_cartesian_interleaved
            ((fft_double_type *)m_dpacked, magIn, phaseIn, m_size/2+1);
        fftw_execute(m_dplani);
        const int sz = m_size;
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
#ifndef FFTW_SINGLE_ONLY
        if (realOut != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = dbuf[i];
            }
    }

    void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const BQ_R__ dbuf = m_dbuf;
        fftw_complex *const BQ_R__ dpacked = m_dpacked;
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][0] = log(magIn[i] + 0.000001);
        }
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][1] = 0.0;
        }
        fftw_execute(m_dplani);
        const int sz = m_size;
#ifndef FFTW_SINGLE_ONLY
        if (cepOut != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                cepOut[i] = dbuf[i];
            }
    }

    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) {
        if (!m_fplanf) initFloat();
        packFloat(realIn, imagIn);
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = fbuf[i];
            }
    }

    void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) {
        if (!m_fplanf) initFloat();
        v_convert((fft_float_type *)m_fpacked, complexIn, m_size + 2);
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = fbuf[i];
            }
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) {
        if (!m_fplanf) initFloat();
        v_polar_to_cartesian_interleaved
            ((fft_float_type *)m_fpacked, magIn, phaseIn, m_size/2+1);
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = fbuf[i];
            }
    }

    void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) {
        if (!m_fplanf) initFloat();
        const int hs = m_size/2;
        fftwf_complex *const BQ_R__ fpacked = m_fpacked;
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][0] = logf(magIn[i] + 0.000001f);
        }
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][1] = 0.f;
        }
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const BQ_R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (cepOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                cepOut[i] = fbuf[i];
            }
    }

private:
    fftwf_plan m_fplanf;
    fftwf_plan m_fplani;
#ifdef FFTW_DOUBLE_ONLY
    double *m_fbuf;
#else
    float *m_fbuf;
#endif
    fftwf_complex *m_fpacked;
    fftw_plan m_dplanf;
    fftw_plan m_dplani;
#ifdef FFTW_SINGLE_ONLY
    float *m_dbuf;
#else
    double *m_dbuf;
#endif
    fftw_complex *m_dpacked;
    const int m_size;
    static int m_extantf;
    static int m_extantd;
#ifdef NO_THREADING
    void lock() {}
    void unlock() {}
#else
#ifdef _WIN32
    static HANDLE m_commonMutex;
    void lock() { WaitForSingleObject(m_commonMutex, INFINITE); }
    void unlock() { ReleaseMutex(m_commonMutex); }
#else
    static pthread_mutex_t m_commonMutex;
    static bool m_haveMutex;
    void lock() { pthread_mutex_lock(&m_commonMutex); }
    void unlock() { pthread_mutex_unlock(&m_commonMutex); }
#endif
#endif
};

int
D_FFTW::m_extantf = 0;

int
D_FFTW::m_extantd = 0;

#ifndef NO_THREADING
#ifdef _WIN32
HANDLE D_FFTW::m_commonMutex = CreateMutex(NULL, FALSE, NULL);
#else
pthread_mutex_t D_FFTW::m_commonMutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#endif

#undef fft_float_type
#undef fft_double_type

#ifdef FFTW_DOUBLE_ONLY
#undef fftwf_complex
#undef fftwf_plan 
#undef fftwf_plan_dft_r2c_1d
#undef fftwf_plan_dft_c2r_1d
#undef fftwf_destroy_plan 
#undef fftwf_malloc 
#undef fftwf_free 
#undef fftwf_execute
#undef atan2f 
#undef sqrtf 
#undef cosf 
#undef sinf
#endif /* FFTW_DOUBLE_ONLY */

#ifdef FFTW_SINGLE_ONLY
#undef fftw_complex
#undef fftw_plan
#undef fftw_plan_dft_r2c_1d
#undef fftw_plan_dft_c2r_1d 
#undef fftw_destroy_plan
#undef fftw_malloc
#undef fftw_free
#undef fftw_execute
#undef atan2
#undef sqrt
#undef cos
#undef sin
#endif /* FFTW_SINGLE_ONLY */

#endif /* HAVE_FFTW3 */

#ifdef HAVE_KISSFFT

class D_KISSFFT : public FFTImpl
{
public:
    D_KISSFFT(int size) :
        m_size(size),
        m_fplanf(0),  
        m_fplani(0)
    {
#ifdef FIXED_POINT
#error KISSFFT is not configured for float values
#endif
        if (sizeof(kiss_fft_scalar) != sizeof(float)) {
            std::cerr << "ERROR: KISSFFT is not configured for float values"
                      << std::endl;
        }

        m_fbuf = new kiss_fft_scalar[m_size + 2];
        m_fpacked = new kiss_fft_cpx[m_size + 2];
        m_fplanf = kiss_fftr_alloc(m_size, 0, NULL, NULL);
        m_fplani = kiss_fftr_alloc(m_size, 1, NULL, NULL);
    }

    ~D_KISSFFT() {
        kiss_fftr_free(m_fplanf);
        kiss_fftr_free(m_fplani);

        delete[] m_fbuf;
        delete[] m_fpacked;
    }

    int getSize() const {
        return m_size;
    }

    FFT::Precisions
    getSupportedPrecisions() const {
        return FFT::SinglePrecision;
    }

    void initFloat() { }
    void initDouble() { }

    void packFloat(const float *BQ_R__ re, const float *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = re[i];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                m_fpacked[i].i = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                m_fpacked[i].i = 0.f;
            }
        }
    }

    void unpackFloat(float *BQ_R__ re, float *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_fpacked[i].r;
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = m_fpacked[i].i;
            }
        }
    }        

    void packDouble(const double *BQ_R__ re, const double *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = float(re[i]);
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                m_fpacked[i].i = float(im[i]);
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                m_fpacked[i].i = 0.f;
            }
        }
    }

    void unpackDouble(double *BQ_R__ re, double *BQ_R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = double(m_fpacked[i].r);
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = double(m_fpacked[i].i);
            }
        }
    }        

    void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        v_convert(m_fbuf, realIn, m_size);
        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);
        unpackDouble(realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) {
        v_convert(m_fbuf, realIn, m_size);
        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);
        v_convert(complexOut, (float *)m_fpacked, m_size + 2);
    }

    void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        v_convert(m_fbuf, realIn, m_size);
        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);
        v_cartesian_interleaved_to_polar
            (magOut, phaseOut, (float *)m_fpacked, m_size/2+1);
    }

    void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) {
        v_convert(m_fbuf, realIn, m_size);
        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);
        v_cartesian_interleaved_to_magnitudes
            (magOut, (float *)m_fpacked, m_size/2+1);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) {
        kiss_fftr(m_fplanf, realIn, m_fpacked);
        unpackFloat(realOut, imagOut);
    }

    void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) {
        kiss_fftr(m_fplanf, realIn, (kiss_fft_cpx *)complexOut);
    }

    void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        kiss_fftr(m_fplanf, realIn, m_fpacked);
        v_cartesian_interleaved_to_polar
            (magOut, phaseOut, (float *)m_fpacked, m_size/2+1);
    }

    void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) {
        kiss_fftr(m_fplanf, realIn, m_fpacked);
        v_cartesian_interleaved_to_magnitudes
            (magOut, (float *)m_fpacked, m_size/2+1);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) {
        packDouble(realIn, imagIn);
        kiss_fftri(m_fplani, m_fpacked, m_fbuf);
        v_convert(realOut, m_fbuf, m_size);
    }

    void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) {
        v_convert((float *)m_fpacked, complexIn, m_size + 2);
        kiss_fftri(m_fplani, m_fpacked, m_fbuf);
        v_convert(realOut, m_fbuf, m_size);
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) {
        v_polar_to_cartesian_interleaved
            ((float *)m_fpacked, magIn, phaseIn, m_size/2+1);
        kiss_fftri(m_fplani, m_fpacked, m_fbuf);
        v_convert(realOut, m_fbuf, m_size);
    }

    void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = float(log(magIn[i] + 0.000001));
            m_fpacked[i].i = 0.0f;
        }
        kiss_fftri(m_fplani, m_fpacked, m_fbuf);
        v_convert(cepOut, m_fbuf, m_size);
    }
    
    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) {
        packFloat(realIn, imagIn);
        kiss_fftri(m_fplani, m_fpacked, realOut);
    }

    void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) {
        v_copy((float *)m_fpacked, complexIn, m_size + 2);
        kiss_fftri(m_fplani, m_fpacked, realOut);
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) {
        v_polar_to_cartesian_interleaved
            ((float *)m_fpacked, magIn, phaseIn, m_size/2+1);
        kiss_fftri(m_fplani, m_fpacked, realOut);
    }

    void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = logf(magIn[i] + 0.000001f);
            m_fpacked[i].i = 0.0f;
        }
        kiss_fftri(m_fplani, m_fpacked, cepOut);
    }

private:
    const int m_size;
    kiss_fftr_cfg m_fplanf;
    kiss_fftr_cfg m_fplani;
    kiss_fft_scalar *m_fbuf;
    kiss_fft_cpx *m_fpacked;
};

#endif /* HAVE_KISSFFT */

#ifdef USE_BUILTIN_FFT

class D_Builtin : public FFTImpl
{
public:
    D_Builtin(int size) :
        m_size(size),
        m_half(size/2),
        m_blockTableSize(16),
        m_maxTabledBlock(1 << m_blockTableSize)
    {
        m_table = allocate_and_zero<int>(m_half);
        m_sincos = allocate_and_zero<double>(m_blockTableSize * 4);
        m_sincos_r = allocate_and_zero<double>(m_half);
        m_vr = allocate_and_zero<double>(m_half);
        m_vi = allocate_and_zero<double>(m_half);
        m_a = allocate_and_zero<double>(m_half + 1);
        m_b = allocate_and_zero<double>(m_half + 1);
        m_c = allocate_and_zero<double>(m_half + 1);
        m_d = allocate_and_zero<double>(m_half + 1);
        m_a_and_b[0] = m_a;
        m_a_and_b[1] = m_b;
        m_c_and_d[0] = m_c;
        m_c_and_d[1] = m_d;
        makeTables();
    }

    ~D_Builtin() {
        deallocate(m_table);
        deallocate(m_sincos);
        deallocate(m_sincos_r);
        deallocate(m_vr);
        deallocate(m_vi);
        deallocate(m_a);
        deallocate(m_b);
        deallocate(m_c);
        deallocate(m_d);
    }

    int getSize() const {
        return m_size;
    }

    FFT::Precisions
    getSupportedPrecisions() const {
        return FFT::DoublePrecision;
    }

    void initFloat() { }
    void initDouble() { }

    void forward(const double *BQ_R__ realIn,
                 double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        transformF(realIn, realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn,
                            double *BQ_R__ complexOut) {
        transformF(realIn, m_c, m_d);
        v_interleave(complexOut, m_c_and_d, 2, m_half + 1);
    }

    void forwardPolar(const double *BQ_R__ realIn,
                      double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        transformF(realIn, m_c, m_d);
        v_cartesian_to_polar(magOut, phaseOut, m_c, m_d, m_half + 1);
    }

    void forwardMagnitude(const double *BQ_R__ realIn,
                          double *BQ_R__ magOut) {
        transformF(realIn, m_c, m_d);
        v_cartesian_to_magnitudes(magOut, m_c, m_d, m_half + 1);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut,
                 float *BQ_R__ imagOut) {
        transformF(realIn, m_c, m_d);
        v_convert(realOut, m_c, m_half + 1);
        v_convert(imagOut, m_d, m_half + 1);
    }

    void forwardInterleaved(const float *BQ_R__ realIn,
                            float *BQ_R__ complexOut) {
        transformF(realIn, m_c, m_d);
        for (int i = 0; i <= m_half; ++i) complexOut[i*2] = m_c[i];
        for (int i = 0; i <= m_half; ++i) complexOut[i*2+1] = m_d[i];
    }

    void forwardPolar(const float *BQ_R__ realIn,
                      float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        transformF(realIn, m_c, m_d);
        v_cartesian_to_polar(magOut, phaseOut, m_c, m_d, m_half + 1);
    }

    void forwardMagnitude(const float *BQ_R__ realIn,
                          float *BQ_R__ magOut) {
        transformF(realIn, m_c, m_d);
        v_cartesian_to_magnitudes(magOut, m_c, m_d, m_half + 1);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn,
                 double *BQ_R__ realOut) {
        transformI(realIn, imagIn, realOut);
    }

    void inverseInterleaved(const double *BQ_R__ complexIn,
                            double *BQ_R__ realOut) {
        v_deinterleave(m_a_and_b, complexIn, 2, m_half + 1);
        transformI(m_a, m_b, realOut);
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn,
                      double *BQ_R__ realOut) {
        v_polar_to_cartesian(m_a, m_b, magIn, phaseIn, m_half + 1);
        transformI(m_a, m_b, realOut);
    }

    void inverseCepstral(const double *BQ_R__ magIn,
                         double *BQ_R__ cepOut) {
        for (int i = 0; i <= m_half; ++i) {
            double real = log(magIn[i] + 0.000001);
            m_a[i] = real;
            m_b[i] = 0.0;
        }
        transformI(m_a, m_b, cepOut);
    }

    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn,
                 float *BQ_R__ realOut) {
        v_convert(m_a, realIn, m_half + 1);
        v_convert(m_b, imagIn, m_half + 1);
        transformI(m_a, m_b, realOut);
    }

    void inverseInterleaved(const float *BQ_R__ complexIn,
                            float *BQ_R__ realOut) {
        for (int i = 0; i <= m_half; ++i) m_a[i] = complexIn[i*2];
        for (int i = 0; i <= m_half; ++i) m_b[i] = complexIn[i*2+1];
        transformI(m_a, m_b, realOut);
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn,
                      float *BQ_R__ realOut) {
        v_polar_to_cartesian(m_a, m_b, magIn, phaseIn, m_half + 1);
        transformI(m_a, m_b, realOut);
    }

    void inverseCepstral(const float *BQ_R__ magIn,
                         float *BQ_R__ cepOut) {
        for (int i = 0; i <= m_half; ++i) {
            float real = logf(magIn[i] + 0.000001);
            m_a[i] = real;
            m_b[i] = 0.0;
        }
        transformI(m_a, m_b, cepOut);
    }

private:
    const int m_size;
    const int m_half;
    const int m_blockTableSize;
    const int m_maxTabledBlock;
    int *m_table;
    double *m_sincos;
    double *m_sincos_r;
    double *m_vr;
    double *m_vi;
    double *m_a;
    double *m_b;
    double *m_c;
    double *m_d;
    double *m_a_and_b[2];
    double *m_c_and_d[2];

    void makeTables() {

        // main table for complex fft - this is of size m_half,
        // because we are at heart a real-complex fft only
        
        int bits;
        int i, j, k, m;

        int n = m_half;
        
        for (i = 0; ; ++i) {
            if (n & (1 << i)) {
                bits = i;
                break;
            }
        }
        
        for (i = 0; i < n; ++i) {
            m = i;
            for (j = k = 0; j < bits; ++j) {
                k = (k << 1) | (m & 1);
                m >>= 1;
            }
            m_table[i] = k;
        }

        // sin and cos tables for complex fft
        int ix = 0;
        for (i = 2; i <= m_maxTabledBlock; i <<= 1) {
            double phase = 2.0 * M_PI / double(i);
            m_sincos[ix++] = sin(phase);
            m_sincos[ix++] = sin(2.0 * phase);
            m_sincos[ix++] = cos(phase);
            m_sincos[ix++] = cos(2.0 * phase);
        }
        
        // sin and cos tables for real-complex transform
        ix = 0;
        for (i = 0; i < n/2; ++i) {
            double phase = M_PI * (double(i + 1) / double(m_half) + 0.5);
            m_sincos_r[ix++] = sin(phase);
            m_sincos_r[ix++] = cos(phase);
        }
    }        

    // Uses m_a and m_b internally; does not touch m_c or m_d
    template <typename T>
    void transformF(const T *BQ_R__ ri,
                    double *BQ_R__ ro, double *BQ_R__ io) {

        int halfhalf = m_half / 2;
        for (int i = 0; i < m_half; ++i) {
            m_a[i] = ri[i * 2];
            m_b[i] = ri[i * 2 + 1];
        }
        transformComplex(m_a, m_b, m_vr, m_vi, false);
        ro[0] = m_vr[0] + m_vi[0];
        ro[m_half] = m_vr[0] - m_vi[0];
        io[0] = io[m_half] = 0.0;
        int ix = 0;
        for (int i = 0; i < halfhalf; ++i) {
            double s = -m_sincos_r[ix++];
            double c =  m_sincos_r[ix++];
            int k = i + 1;
            double r0 = m_vr[k];
            double i0 = m_vi[k];
            double r1 = m_vr[m_half - k];
            double i1 = -m_vi[m_half - k];
            double tw_r = (r0 - r1) * c - (i0 - i1) * s;
            double tw_i = (r0 - r1) * s + (i0 - i1) * c;
            ro[k] = (r0 + r1 + tw_r) * 0.5;
            ro[m_half - k] = (r0 + r1 - tw_r) * 0.5;
            io[k] = (i0 + i1 + tw_i) * 0.5;
            io[m_half - k] = (tw_i - i0 - i1) * 0.5;
        }
    }

    // Uses m_c and m_d internally; does not touch m_a or m_b
    template <typename T>
    void transformI(const double *BQ_R__ ri, const double *BQ_R__ ii,
                    T *BQ_R__ ro) {
        
        int halfhalf = m_half / 2;
        m_vr[0] = ri[0] + ri[m_half];
        m_vi[0] = ri[0] - ri[m_half];
        int ix = 0;
        for (int i = 0; i < halfhalf; ++i) {
            double s = m_sincos_r[ix++];
            double c = m_sincos_r[ix++];
            int k = i + 1;
            double r0 = ri[k];
            double r1 = ri[m_half - k];
            double i0 = ii[k];
            double i1 = -ii[m_half - k];
            double tw_r = (r0 - r1) * c - (i0 - i1) * s;
            double tw_i = (r0 - r1) * s + (i0 - i1) * c;
            m_vr[k] = (r0 + r1 + tw_r);
            m_vr[m_half - k] = (r0 + r1 - tw_r);
            m_vi[k] = (i0 + i1 + tw_i);
            m_vi[m_half - k] = (tw_i - i0 - i1);
        }
        transformComplex(m_vr, m_vi, m_c, m_d, true);
        for (int i = 0; i < m_half; ++i) {
            ro[i*2] = m_c[i];
            ro[i*2+1] = m_d[i];
        }
    }
    
    void transformComplex(const double *BQ_R__ ri, const double *BQ_R__ ii,
                          double *BQ_R__ ro, double *BQ_R__ io,
                          bool inverse) {

        // Following Don Cross's 1998 implementation, described by its
        // author as public domain.
        
        // Because we are at heart a real-complex fft only, and we know that:
        const int n = m_half;

        for (int i = 0; i < n; ++i) {
            int j = m_table[i];
            ro[j] = ri[i];
            io[j] = ii[i];
        }
        
        int ix = 0;
        int blockEnd = 1;
        double ifactor = (inverse ? -1.0 : 1.0);
        
        for (int blockSize = 2; blockSize <= n; blockSize <<= 1) {

            double sm1, sm2, cm1, cm2;

            if (blockSize <= m_maxTabledBlock) {
                sm1 = ifactor * m_sincos[ix++];
                sm2 = ifactor * m_sincos[ix++];
                cm1 = m_sincos[ix++];
                cm2 = m_sincos[ix++];
            } else {
                double phase = 2.0 * M_PI / double(blockSize);
                sm1 = ifactor * sin(phase);
                sm2 = ifactor * sin(2.0 * phase);
                cm1 = cos(phase);
                cm2 = cos(2.0 * phase);
            }
            
            double w = 2 * cm1;
            double ar[3], ai[3];
            
            for (int i = 0; i < n; i += blockSize) {
                
                ar[2] = cm2;
                ar[1] = cm1;
                
                ai[2] = sm2;
                ai[1] = sm1;

                int j = i;
                
                for (int m = 0; m < blockEnd; ++m) {
                    
                    ar[0] = w * ar[1] - ar[2];
                    ar[2] = ar[1];
                    ar[1] = ar[0];

                    ai[0] = w * ai[1] - ai[2];
                    ai[2] = ai[1];
                    ai[1] = ai[0];

                    int k = j + blockEnd;
                    double tr = ar[0] * ro[k] - ai[0] * io[k];
                    double ti = ar[0] * io[k] + ai[0] * ro[k];

                    ro[k] = ro[j] - tr;
                    io[k] = io[j] - ti;

                    ro[j] += tr;
                    io[j] += ti;

                    ++j;
                }
            }

            blockEnd = blockSize;
        }
    }
};

#endif /* USE_BUILTIN_FFT */

class D_DFT : public FFTImpl
{
private:
    template <typename T>
    class DFT
    {
    public:
        DFT(int size) : m_size(size), m_bins(size/2 + 1) {
            
            m_sin = allocate_channels<double>(m_size, m_size);
            m_cos = allocate_channels<double>(m_size, m_size);

            for (int i = 0; i < m_size; ++i) {
                for (int j = 0; j < m_size; ++j) {
                    double arg = (double(i) * double(j) * M_PI * 2.0) / m_size;
                    m_sin[i][j] = sin(arg);
                    m_cos[i][j] = cos(arg);
                }
            }

            m_tmp = allocate_channels<double>(2, m_size);
        }

        ~DFT() {
            deallocate_channels(m_tmp, 2);
            deallocate_channels(m_sin, m_size);
            deallocate_channels(m_cos, m_size);
        }

        void forward(const T *BQ_R__ realIn, T *BQ_R__ realOut, T *BQ_R__ imagOut) {
            for (int i = 0; i < m_bins; ++i) {
                double re = 0.0, im = 0.0;
                for (int j = 0; j < m_size; ++j) re += realIn[j] * m_cos[i][j];
                for (int j = 0; j < m_size; ++j) im -= realIn[j] * m_sin[i][j];
                realOut[i] = T(re);
                imagOut[i] = T(im);
            }
        }

        void forwardInterleaved(const T *BQ_R__ realIn, T *BQ_R__ complexOut) {
            for (int i = 0; i < m_bins; ++i) {
                double re = 0.0, im = 0.0;
                for (int j = 0; j < m_size; ++j) re += realIn[j] * m_cos[i][j];
                for (int j = 0; j < m_size; ++j) im -= realIn[j] * m_sin[i][j];
                complexOut[i*2] = T(re);
                complexOut[i*2 + 1] = T(im);
            }
        }

        void forwardPolar(const T *BQ_R__ realIn, T *BQ_R__ magOut, T *BQ_R__ phaseOut) {
            forward(realIn, magOut, phaseOut); // temporarily
            for (int i = 0; i < m_bins; ++i) {
                T re = magOut[i], im = phaseOut[i];
                c_magphase(magOut + i, phaseOut + i, re, im);
            }
        }

        void forwardMagnitude(const T *BQ_R__ realIn, T *BQ_R__ magOut) {
            for (int i = 0; i < m_bins; ++i) {
                double re = 0.0, im = 0.0;
                for (int j = 0; j < m_size; ++j) re += realIn[j] * m_cos[i][j];
                for (int j = 0; j < m_size; ++j) im -= realIn[j] * m_sin[i][j];
                magOut[i] = T(sqrt(re * re + im * im));
            }
        }

        void inverse(const T *BQ_R__ realIn, const T *BQ_R__ imagIn, T *BQ_R__ realOut) {
            for (int i = 0; i < m_bins; ++i) {
                m_tmp[0][i] = realIn[i];
                m_tmp[1][i] = imagIn[i];
            }
            for (int i = m_bins; i < m_size; ++i) {
                m_tmp[0][i] = realIn[m_size - i];
                m_tmp[1][i] = -imagIn[m_size - i];
            }
            for (int i = 0; i < m_size; ++i) {
                double re = 0.0;
                const double *const cos = m_cos[i];
                const double *const sin = m_sin[i];
                for (int j = 0; j < m_size; ++j) re += m_tmp[0][j] * cos[j];
                for (int j = 0; j < m_size; ++j) re -= m_tmp[1][j] * sin[j];
                realOut[i] = T(re);
            }
        }

        void inverseInterleaved(const T *BQ_R__ complexIn, T *BQ_R__ realOut) {
            for (int i = 0; i < m_bins; ++i) {
                m_tmp[0][i] = complexIn[i*2];
                m_tmp[1][i] = complexIn[i*2+1];
            }
            for (int i = m_bins; i < m_size; ++i) {
                m_tmp[0][i] = complexIn[(m_size - i) * 2];
                m_tmp[1][i] = -complexIn[(m_size - i) * 2 + 1];
            }
            for (int i = 0; i < m_size; ++i) {
                double re = 0.0;
                const double *const cos = m_cos[i];
                const double *const sin = m_sin[i];
                for (int j = 0; j < m_size; ++j) re += m_tmp[0][j] * cos[j];
                for (int j = 0; j < m_size; ++j) re -= m_tmp[1][j] * sin[j];
                realOut[i] = T(re);
            }
        }

        void inversePolar(const T *BQ_R__ magIn, const T *BQ_R__ phaseIn, T *BQ_R__ realOut) {
            T *complexIn = allocate<T>(m_bins * 2);
            v_polar_to_cartesian_interleaved(complexIn, magIn, phaseIn, m_bins);
            inverseInterleaved(complexIn, realOut);
            deallocate(complexIn);
        }

        void inverseCepstral(const T *BQ_R__ magIn, T *BQ_R__ cepOut) {
            T *complexIn = allocate_and_zero<T>(m_bins * 2);
            for (int i = 0; i < m_bins; ++i) {
                complexIn[i*2] = T(log(magIn[i] + 0.000001));
            }
            inverseInterleaved(complexIn, cepOut);
            deallocate(complexIn);
        }

    private:
        const int m_size;
        const int m_bins;
        double **m_sin;
        double **m_cos;
        double **m_tmp;
    };
    
public:
    D_DFT(int size) : m_size(size), m_double(0), m_float(0) { }

    ~D_DFT() {
        delete m_double;
        delete m_float;
    }

    int getSize() const {
        return m_size;
    }

    FFT::Precisions
    getSupportedPrecisions() const {
        return FFT::DoublePrecision;
    }

    void initFloat() {
        if (!m_float) {
            m_float = new DFT<float>(m_size);
        }
    }
        
    void initDouble() {
        if (!m_double) {
            m_double = new DFT<double>(m_size);
        }
    }

    void forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut) {
        initDouble();
        m_double->forward(realIn, realOut, imagOut);
    }

    void forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut) {
        initDouble();
        m_double->forwardInterleaved(realIn, complexOut);
    }

    void forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut) {
        initDouble();
        m_double->forwardPolar(realIn, magOut, phaseOut);
    }

    void forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut) {
        initDouble();
        m_double->forwardMagnitude(realIn, magOut);
    }

    void forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut) {
        initFloat();
        m_float->forward(realIn, realOut, imagOut);
    }

    void forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut) {
        initFloat();
        m_float->forwardInterleaved(realIn, complexOut);
    }

    void forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut) {
        initFloat();
        m_float->forwardPolar(realIn, magOut, phaseOut);
    }

    void forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut) {
        initFloat();
        m_float->forwardMagnitude(realIn, magOut);
    }

    void inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut) {
        initDouble();
        m_double->inverse(realIn, imagIn, realOut);
    }

    void inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut) {
        initDouble();
        m_double->inverseInterleaved(complexIn, realOut);
    }

    void inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut) {
        initDouble();
        m_double->inversePolar(magIn, phaseIn, realOut);
    }

    void inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut) {
        initDouble();
        m_double->inverseCepstral(magIn, cepOut);
    }

    void inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut) {
        initFloat();
        m_float->inverse(realIn, imagIn, realOut);
    }

    void inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut) {
        initFloat();
        m_float->inverseInterleaved(complexIn, realOut);
    }

    void inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut) {
        initFloat();
        m_float->inversePolar(magIn, phaseIn, realOut);
    }

    void inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut) {
        initFloat();
        m_float->inverseCepstral(magIn, cepOut);
    }

private:
    int m_size;
    DFT<double> *m_double;
    DFT<float> *m_float;
};

} /* end namespace FFTs */

enum SizeConstraint {
    SizeConstraintNone           = 0x0,
    SizeConstraintEven           = 0x1,
    SizeConstraintPowerOfTwo     = 0x2,
    SizeConstraintEvenPowerOfTwo = 0x3 // i.e. 0x1 | 0x2. Excludes size 1 obvs
};

typedef std::map<std::string, SizeConstraint> ImplMap;

static std::string defaultImplementation;

static ImplMap
getImplementationDetails()
{
    ImplMap impls;
    
#ifdef HAVE_IPP
    impls["ipp"] = SizeConstraintEvenPowerOfTwo;
#endif
#ifdef HAVE_FFTW3
    impls["fftw"] = SizeConstraintNone;
#endif
#ifdef HAVE_KISSFFT
    impls["kissfft"] = SizeConstraintEven;
#endif
#ifdef HAVE_VDSP
    impls["vdsp"] = SizeConstraintEvenPowerOfTwo;
#endif
#ifdef USE_BUILTIN_FFT
    impls["builtin"] = SizeConstraintEvenPowerOfTwo;
#endif

    impls["dft"] = SizeConstraintNone;

    return impls;
}

static std::string
pickImplementation(int size)
{
    ImplMap impls = getImplementationDetails();

    bool isPowerOfTwo = !(size & (size-1));
    bool isEven = !(size & 1);

    if (defaultImplementation != "") {
        ImplMap::const_iterator itr = impls.find(defaultImplementation);
        if (itr != impls.end()) {
            if (((itr->second & SizeConstraintPowerOfTwo) && !isPowerOfTwo) ||
                ((itr->second & SizeConstraintEven) && !isEven)) {
//                std::cerr << "NOTE: bqfft: Explicitly-set default "
//                          << "implementation \"" << defaultImplementation
//                          << "\" does not support size " << size
//                          << ", trying other compiled-in implementations"
//                          << std::endl;
            } else {
                return defaultImplementation;
            }
        } else {
            std::cerr << "WARNING: bqfft: Default implementation \""
                      << defaultImplementation << "\" is not compiled in"
                      << std::endl;
        }
    } 
    
    std::string preference[] = {
        "ipp", "vdsp", "fftw", "builtin", "kissfft"
    };

    for (int i = 0; i < int(sizeof(preference)/sizeof(preference[0])); ++i) {
        ImplMap::const_iterator itr = impls.find(preference[i]);
        if (itr != impls.end()) {
            if ((itr->second & SizeConstraintPowerOfTwo) &&
                // out of an abundance of caution we don't attempt to
                // use power-of-two implementations with size 2
                // either, as they may involve a half-half
                // complex-complex underneath (which would end up with
                // size 0)
                (!isPowerOfTwo || size < 4)) {
                continue;
            }
            if ((itr->second & SizeConstraintEven) && !isEven) {
                continue;
            }
            return preference[i];
        }
    }

    std::cerr << "WARNING: bqfft: No compiled-in implementation supports size "
              << size << ", falling back to slow DFT" << std::endl;
    
    return "dft";
}

std::set<std::string>
FFT::getImplementations()
{
    ImplMap impls = getImplementationDetails();
    std::set<std::string> toReturn;
    for (ImplMap::const_iterator i = impls.begin(); i != impls.end(); ++i) {
        toReturn.insert(i->first);
    }
    return toReturn;
}

std::string
FFT::getDefaultImplementation()
{
    return defaultImplementation;
}

void
FFT::setDefaultImplementation(std::string i)
{
    if (i == "") {
        defaultImplementation = i;
        return;
    } 
    ImplMap impls = getImplementationDetails();
    ImplMap::const_iterator itr = impls.find(i);
    if (itr == impls.end()) {
        std::cerr << "WARNING: bqfft: setDefaultImplementation: "
                  << "requested implementation \"" << i
                  << "\" is not compiled in" << std::endl;
    } else {
        defaultImplementation = i;
    }
}

FFT::FFT(int size, int debugLevel) :
    d(0)
{
    std::string impl = pickImplementation(size);

    if (debugLevel > 0) {
        std::cerr << "FFT::FFT(" << size << "): using implementation: "
                  << impl << std::endl;
    }

    if (impl == "ipp") {
#ifdef HAVE_IPP
        d = new FFTs::D_IPP(size);
#endif
    } else if (impl == "fftw") {
#ifdef HAVE_FFTW3
        d = new FFTs::D_FFTW(size);
#endif
    } else if (impl == "kissfft") {        
#ifdef HAVE_KISSFFT
        d = new FFTs::D_KISSFFT(size);
#endif
    } else if (impl == "vdsp") {
#ifdef HAVE_VDSP
        d = new FFTs::D_VDSP(size);
#endif
    } else if (impl == "builtin") {
#ifdef USE_BUILTIN_FFT
        d = new FFTs::D_Builtin(size);
#endif
    } else if (impl == "dft") {
        d = new FFTs::D_DFT(size);
    }

    if (!d) {
        std::cerr << "FFT::FFT(" << size << "): ERROR: implementation "
                  << impl << " is not compiled in" << std::endl;
#ifndef NO_EXCEPTIONS
        throw InvalidImplementation;
#else
        abort();
#endif
    }
}

FFT::~FFT()
{
    delete d;
}

#ifndef NO_EXCEPTIONS
#define CHECK_NOT_NULL(x) \
    if (!(x)) { \
        std::cerr << "FFT: ERROR: Null argument " #x << std::endl;  \
        throw NullArgument; \
    }
#else
#define CHECK_NOT_NULL(x) \
    if (!(x)) { \
        std::cerr << "FFT: ERROR: Null argument " #x << std::endl;  \
        std::cerr << "FFT: Would be throwing NullArgument here, if exceptions were not disabled" << std::endl;  \
        return; \
    }
#endif

void
FFT::forward(const double *BQ_R__ realIn, double *BQ_R__ realOut, double *BQ_R__ imagOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(realOut);
    CHECK_NOT_NULL(imagOut);
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardInterleaved(const double *BQ_R__ realIn, double *BQ_R__ complexOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(complexOut);
    d->forwardInterleaved(realIn, complexOut);
}

void
FFT::forwardPolar(const double *BQ_R__ realIn, double *BQ_R__ magOut, double *BQ_R__ phaseOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(magOut);
    CHECK_NOT_NULL(phaseOut);
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(const double *BQ_R__ realIn, double *BQ_R__ magOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(magOut);
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::forward(const float *BQ_R__ realIn, float *BQ_R__ realOut, float *BQ_R__ imagOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(realOut);
    CHECK_NOT_NULL(imagOut);
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardInterleaved(const float *BQ_R__ realIn, float *BQ_R__ complexOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(complexOut);
    d->forwardInterleaved(realIn, complexOut);
}

void
FFT::forwardPolar(const float *BQ_R__ realIn, float *BQ_R__ magOut, float *BQ_R__ phaseOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(magOut);
    CHECK_NOT_NULL(phaseOut);
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(const float *BQ_R__ realIn, float *BQ_R__ magOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(magOut);
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::inverse(const double *BQ_R__ realIn, const double *BQ_R__ imagIn, double *BQ_R__ realOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(imagIn);
    CHECK_NOT_NULL(realOut);
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inverseInterleaved(const double *BQ_R__ complexIn, double *BQ_R__ realOut)
{
    CHECK_NOT_NULL(complexIn);
    CHECK_NOT_NULL(realOut);
    d->inverseInterleaved(complexIn, realOut);
}

void
FFT::inversePolar(const double *BQ_R__ magIn, const double *BQ_R__ phaseIn, double *BQ_R__ realOut)
{
    CHECK_NOT_NULL(magIn);
    CHECK_NOT_NULL(phaseIn);
    CHECK_NOT_NULL(realOut);
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::inverseCepstral(const double *BQ_R__ magIn, double *BQ_R__ cepOut)
{
    CHECK_NOT_NULL(magIn);
    CHECK_NOT_NULL(cepOut);
    d->inverseCepstral(magIn, cepOut);
}

void
FFT::inverse(const float *BQ_R__ realIn, const float *BQ_R__ imagIn, float *BQ_R__ realOut)
{
    CHECK_NOT_NULL(realIn);
    CHECK_NOT_NULL(imagIn);
    CHECK_NOT_NULL(realOut);
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inverseInterleaved(const float *BQ_R__ complexIn, float *BQ_R__ realOut)
{
    CHECK_NOT_NULL(complexIn);
    CHECK_NOT_NULL(realOut);
    d->inverseInterleaved(complexIn, realOut);
}

void
FFT::inversePolar(const float *BQ_R__ magIn, const float *BQ_R__ phaseIn, float *BQ_R__ realOut)
{
    CHECK_NOT_NULL(magIn);
    CHECK_NOT_NULL(phaseIn);
    CHECK_NOT_NULL(realOut);
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::inverseCepstral(const float *BQ_R__ magIn, float *BQ_R__ cepOut)
{
    CHECK_NOT_NULL(magIn);
    CHECK_NOT_NULL(cepOut);
    d->inverseCepstral(magIn, cepOut);
}

void
FFT::initFloat() 
{
    d->initFloat();
}

void
FFT::initDouble() 
{
    d->initDouble();
}

int
FFT::getSize() const
{
    return d->getSize();
}

FFT::Precisions
FFT::getSupportedPrecisions() const
{
    return d->getSupportedPrecisions();
}

#ifdef FFT_MEASUREMENT

#ifdef FFT_MEASUREMENT_RETURN_RESULT_TEXT
std::string
#else
void
#endif
FFT::tune()
{
#ifdef FFT_MEASUREMENT_RETURN_RESULT_TEXT
    std::ostringstream os;
#else
#define os std::cerr
#endif
    os << "FFT::tune()..." << std::endl;

    std::vector<int> sizes;
    std::map<std::string, FFTImpl *> candidates;
    std::map<std::string, int> wins;

    sizes.push_back(512);
    sizes.push_back(1024);
    sizes.push_back(2048);
    sizes.push_back(4096);
    
    for (unsigned int si = 0; si < sizes.size(); ++si) {

        int size = sizes[si];

        while (!candidates.empty()) {
            delete candidates.begin()->second;
            candidates.erase(candidates.begin());
        }

        FFTImpl *d;
        
#ifdef HAVE_IPP
        os << "Constructing new IPP FFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_IPP(size);
        d->initFloat();
        d->initDouble();
        candidates["ipp"] = d;
#endif
        
#ifdef HAVE_FFTW3
        os << "Constructing new FFTW3 FFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_FFTW(size);
        d->initFloat();
        d->initDouble();
        candidates["fftw"] = d;
#endif

#ifdef HAVE_KISSFFT
        os << "Constructing new KISSFFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_KISSFFT(size);
        d->initFloat();
        d->initDouble();
        candidates["kissfft"] = d;
#endif        

#ifdef USE_BUILTIN_FFT
        os << "Constructing new Builtin FFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_Builtin(size);
        d->initFloat();
        d->initDouble();
        candidates["builtin"] = d;
#endif
        
#ifdef HAVE_VDSP
        os << "Constructing new vDSP FFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_VDSP(size);
        d->initFloat();
        d->initDouble();
        candidates["vdsp"] = d;
#endif

        os << "Constructing new DFT object for size " << size << "..." << std::endl;
        d = new FFTs::D_DFT(size);
        d->initFloat();
        d->initDouble();
        candidates["dft"] = d;

        os << "CLOCKS_PER_SEC = " << CLOCKS_PER_SEC << std::endl;
        float divisor = float(CLOCKS_PER_SEC) / 1000.f;
        
        os << "Timing order is: ";
        for (std::map<std::string, FFTImpl *>::iterator ci = candidates.begin();
             ci != candidates.end(); ++ci) {
            os << ci->first << " ";
        }
        os << std::endl;

        int iterations = 500;
        os << "Iterations: " << iterations << std::endl;

        double *da = new double[size];
        double *db = new double[size];
        double *dc = new double[size];
        double *dd = new double[size];
        double *di = new double[size + 2];
        double *dj = new double[size + 2];

        float *fa = new float[size];
        float *fb = new float[size];
        float *fc = new float[size];
        float *fd = new float[size];
        float *fi = new float[size + 2];
        float *fj = new float[size + 2];

        for (int type = 0; type < 16; ++type) {
    
            //!!!
            if ((type > 3 && type < 8) ||
                (type > 11)) {
                continue;
            }

            if (type > 7) {
                // inverse transform: bigger inputs, to simulate the
                // fact that the forward transform is unscaled
                for (int i = 0; i < size; ++i) {
                    da[i] = drand48() * size;
                    fa[i] = da[i];
                    db[i] = drand48() * size;
                    fb[i] = db[i];
                }
            } else {    
                for (int i = 0; i < size; ++i) {
                    da[i] = drand48();
                    fa[i] = da[i];
                    db[i] = drand48();
                    fb[i] = db[i];
                }
            }
                
            for (int i = 0; i < size + 2; ++i) {
                di[i] = drand48();
                fi[i] = di[i];
            }

            std::string low;
            clock_t lowscore = 0;

            const char *names[] = {

                "Forward Cartesian Double",
                "Forward Interleaved Double",
                "Forward Polar Double",
                "Forward Magnitude Double",
                "Forward Cartesian Float",
                "Forward Interleaved Float",
                "Forward Polar Float",
                "Forward Magnitude Float",

                "Inverse Cartesian Double",
                "Inverse Interleaved Double",
                "Inverse Polar Double",
                "Inverse Cepstral Double",
                "Inverse Cartesian Float",
                "Inverse Interleaved Float",
                "Inverse Polar Float",
                "Inverse Cepstral Float"
            };
            os << names[type] << " :: ";

            for (std::map<std::string, FFTImpl *>::iterator ci = candidates.begin();
                 ci != candidates.end(); ++ci) {

                FFTImpl *d = ci->second;

                double mean = 0;

                clock_t start = clock();
                
                for (int i = 0; i < iterations; ++i) {

                    if (i == 0) {
                        for (int j = 0; j < size; ++j) {
                            dc[j] = 0;
                            dd[j] = 0;
                            fc[j] = 0;
                            fd[j] = 0;
                            fj[j] = 0;
                            dj[j] = 0;
                        }
                    }

                    switch (type) {
                    case 0: d->forward(da, dc, dd); break;
                    case 1: d->forwardInterleaved(da, dj); break;
                    case 2: d->forwardPolar(da, dc, dd); break;
                    case 3: d->forwardMagnitude(da, dc); break;
                    case 4: d->forward(fa, fc, fd); break;
                    case 5: d->forwardInterleaved(fa, fj); break;
                    case 6: d->forwardPolar(fa, fc, fd); break;
                    case 7: d->forwardMagnitude(fa, fc); break;
                    case 8: d->inverse(da, db, dc); break;
                    case 9: d->inverseInterleaved(di, dc); break;
                    case 10: d->inversePolar(da, db, dc); break;
                    case 11: d->inverseCepstral(da, dc); break;
                    case 12: d->inverse(fa, fb, fc); break;
                    case 13: d->inverseInterleaved(fi, fc); break;
                    case 14: d->inversePolar(fa, fb, fc); break;
                    case 15: d->inverseCepstral(fa, fc); break;
                    }

                    if (i == 0) {
                        mean = 0;
                        for (int j = 0; j < size; ++j) {
                            mean += dc[j];
                            mean += dd[j];
                            mean += fc[j];
                            mean += fd[j];
                            mean += fj[j];
                            mean += dj[j];
                        }
                        mean /= size * 6;
                    }
                }

                clock_t end = clock();

                os << float(end - start)/divisor << " (" << mean << ") ";

                if (low == "" || (end - start) < lowscore) {
                    low = ci->first;
                    lowscore = end - start;
                }
            }

            os << std::endl;

            os << "  size " << size << ", type " << type << ": fastest is " << low << " (time " << float(lowscore)/divisor << ")" << std::endl;

            wins[low]++;
        }
        
        delete[] fa;
        delete[] fb;
        delete[] fc;
        delete[] fd;
        delete[] da;
        delete[] db;
        delete[] dc;
        delete[] dd;
    }

    while (!candidates.empty()) {
        delete candidates.begin()->second;
        candidates.erase(candidates.begin());
    }

    int bestscore = 0;
    std::string best;

    for (std::map<std::string, int>::iterator wi = wins.begin(); wi != wins.end(); ++wi) {
        if (best == "" || wi->second > bestscore) {
            best = wi->first;
            bestscore = wi->second;
        }
    }

    os << "overall winner is " << best << " with " << bestscore << " wins" << std::endl;

#ifdef FFT_MEASUREMENT_RETURN_RESULT_TEXT
    return os.str();
#endif
}

#endif

}
