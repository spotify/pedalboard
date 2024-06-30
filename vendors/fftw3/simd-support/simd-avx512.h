/*
 * Copyright (c) 2003, 2007-11 Matteo Frigo
 * Copyright (c) 2003, 2007-11 Massachusetts Institute of Technology
 *
 * AVX-512 support implemented by Romain Dolbeau.
 * Romain Dolbeau hereby places his modifications in the public domain.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#if defined(FFTW_LDOUBLE) || defined(FFTW_QUAD)
#error "AVX-512 vector instructions only works in single or double precision"
#endif

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define SUFF(name) name ## _ps
#  define SCAL(x) x ## f
#else /* !FFTW_SINGLE */
#  define DS(d,s) d /* double-precision option */
#  define SUFF(name) name ## _pd
#  define SCAL(x) x
#endif /* FFTW_SINGLE */

#define SIMD_SUFFIX  _avx512  /* for renaming */
#define VL DS(4, 8)        /* SIMD complex vector length */
#define SIMD_VSTRIDE_OKA(x) ((x) == 2) 
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

#if defined(__GNUC__) && !defined(__AVX512F__) /* sanity check */
#error "compiling simd-avx512.h without avx-512f support"
#endif

#if !defined(HAVE_AVX2)
#warning "You should probably enable AVX2 with --enable-avx2 for AVX-512"
#endif

#include <immintrin.h>

typedef DS(__m512d, __m512) V;

#define VLIT(re, im) DS(SUFF(_mm512_setr)(im, re, im, re, im, re, im, re),SUFF(_mm512_setr)(im, re, im, re, im, re, im, re, im, re, im, re, im, re, im, re))
#define VLIT1(val) SUFF(_mm512_set1)(val)
#define LDK(x) x
#define DVK(var, val) V var = VLIT1(val)
#define VZERO SUFF(_mm512_setzero)()

#define VDUPL(x) DS(_mm512_movedup_pd(x),_mm512_moveldup_ps(x))
#define VDUPH(x) DS(_mm512_unpackhi_pd(x, x),_mm512_movehdup_ps(x))
#define FLIP_RI(x) SUFF(_mm512_shuffle)(x, x, DS(0x55,0xB1))
#define VCONJ(x) SUFF(_mm512_fmsubadd)(VZERO, VZERO, x)
static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}

#define VADD(a,b) SUFF(_mm512_add)(a,b)
#define VSUB(a,b) SUFF(_mm512_sub)(a,b)
#define VMUL(a,b) SUFF(_mm512_mul)(a,b)
#define VFMA(a, b, c)  SUFF(_mm512_fmadd)(a, b, c)
#define VFMS(a, b, c)  SUFF(_mm512_fmsub)(a, b, c)
#define VFNMS(a, b, c) SUFF(_mm512_fnmadd)(a, b, c)
#define VFMAI(b, c)    SUFF(_mm512_fmaddsub)(VLIT1(1.), c, FLIP_RI(b))
#define VFNMSI(b, c)   SUFF(_mm512_fmsubadd)(VLIT1(1.), c, FLIP_RI(b))
#define VFMACONJ(b,c)  SUFF(_mm512_fmsubadd)(VLIT1(1.), c, b)
#define VFMSCONJ(b,c)  SUFF(_mm512_fmsubadd)(VLIT1(-1.), c, b)
#define VFNMSCONJ(b,c) SUFF(_mm512_fmaddsub)(VLIT1(1.), c, b)

static inline V LDA(const R *x, INT ivs, const R *aligned_like) {
  (void)aligned_like; /* UNUSED */
  (void)ivs; /* UNUSED */
  return SUFF(_mm512_loadu)(x);
}
static inline void STA(R *x, V v, INT ovs, const R *aligned_like) {
  (void)aligned_like; /* UNUSED */
  (void)ovs; /* UNUSED */
  SUFF(_mm512_storeu)(x, v);
}

#if FFTW_SINGLE

static inline V LDu(const R *x, INT ivs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m512i index = _mm512_set_epi32(7 * ivs + 1, 7 * ivs,
                                   6 * ivs + 1, 6 * ivs,
                                   5 * ivs + 1, 5 * ivs,
                                   4 * ivs + 1, 4 * ivs,
                                   3 * ivs + 1, 3 * ivs,
                                   2 * ivs + 1, 2 * ivs,
                                   1 * ivs + 1, 1 * ivs,
                                   0 * ivs + 1, 0 * ivs);
  
  return _mm512_i32gather_ps(index, x, 4);
}

static inline void STu(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m512i index = _mm512_set_epi32(7 * ovs + 1, 7 * ovs,
                                   6 * ovs + 1, 6 * ovs,
                                   5 * ovs + 1, 5 * ovs,
                                   4 * ovs + 1, 4 * ovs,
                                   3 * ovs + 1, 3 * ovs,
                                   2 * ovs + 1, 2 * ovs,
                                   1 * ovs + 1, 1 * ovs,
                                   0 * ovs + 1, 0 * ovs);
  
  _mm512_i32scatter_ps(x, index, v, 4);
}

#else /* !FFTW_SINGLE */

static inline V LDu(const R *x, INT ivs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m256i index = _mm256_set_epi32(3 * ivs + 1, 3 * ivs,
                                   2 * ivs + 1, 2 * ivs,
                                   1 * ivs + 1, 1 * ivs,
                                   0 * ivs + 1, 0 * ivs);
  
  return _mm512_i32gather_pd(index, x, 8);
}

static inline void STu(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m256i index = _mm256_set_epi32(3 * ovs + 1, 3 * ovs,
                                   2 * ovs + 1, 2 * ovs,
                                   1 * ovs + 1, 1 * ovs,
                                   0 * ovs + 1, 0 * ovs);
  
  _mm512_i32scatter_pd(x, index, v, 8);
}

#endif /* FFTW_SINGLE */

#define LD LDu
#define ST STu

#ifdef FFTW_SINGLE
#define STM2(x, v, ovs, a) ST(x, v, ovs, a)
#define STN2(x, v0, v1, ovs) /* nop */

static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m512i index = _mm512_set_epi32(15 * ovs, 14 * ovs,
                                   13 * ovs, 12 * ovs,
                                   11 * ovs, 10 * ovs,
                                   9 * ovs, 8 * ovs,
                                   7 * ovs, 6 * ovs,
                                   5 * ovs, 4 * ovs,
                                   3 * ovs, 2 * ovs,
                                   1 * ovs, 0 * ovs);
  
  _mm512_i32scatter_ps(x, index, v, 4);
}
#define STN4(x, v0, v1, v2, v3, ovs)  /* no-op */
#else /* !FFTW_SINGLE */
#define STM2(x, v, ovs, a) ST(x, v, ovs, a)
#define STN2(x, v0, v1, ovs) /* nop */

static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m256i index = _mm256_set_epi32(7 * ovs, 6 * ovs,
                                   5 * ovs, 4 * ovs,
                                   3 * ovs, 2 * ovs,
                                   1 * ovs, 0 * ovs);
  
  _mm512_i32scatter_pd(x, index, v, 8);
}
#define STN4(x, v0, v1, v2, v3, ovs)  /* no-op */
#endif /* FFTW_SINGLE */

static inline V VZMUL(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* tr = VMUL(sr, tr); */
     /* sr = VBYI(sr); */
     /* return VFMA(ti, sr, tr); */
     return SUFF(_mm512_fmaddsub)(sr, VDUPL(tx), VMUL(FLIP_RI(sr), VDUPH(tx)));
}

static inline V VZMULJ(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* tr = VMUL(sr, tr); */
     /* sr = VBYI(sr); */
     /* return VFNMS(ti, sr, tr); */
     return SUFF(_mm512_fmsubadd)(sr, VDUPL(tx), VMUL(FLIP_RI(sr), VDUPH(tx)));
}

static inline V VZMULI(V tx, V sr)
{
     V tr = VDUPL(tx);
     V ti = VDUPH(tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VFMS(tr, sr, ti);
  /* return SUFF(_mm512_addsub)(SUFF(_mm512_fnmadd)(sr, VDUPH(tx), VZERO), VMUL(FLIP_RI(sr), VDUPL(tx))); */
}

static inline V VZMULIJ(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* ti = VMUL(ti, sr); */
     /* sr = VBYI(sr); */
     /* return VFMA(tr, sr, ti); */
     return SUFF(_mm512_fmaddsub)(sr, VDUPH(tx), VMUL(FLIP_RI(sr), VDUPL(tx)));
}

/* twiddle storage #1: compact, slower */
#ifdef FFTW_SINGLE
# define VTW1(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}, {TW_CEXP, v+2, x}, {TW_CEXP, v+3, x}, {TW_CEXP, v+4, x}, {TW_CEXP, v+5, x}, {TW_CEXP, v+6, x}, {TW_CEXP, v+7, x}
#else /* !FFTW_SINGLE */
# define VTW1(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}, {TW_CEXP, v+2, x}, {TW_CEXP, v+3, x}
#endif /* FFTW_SINGLE */
#define TWVL1 (VL)

static inline V BYTW1(const R *t, V sr)
{
     return VZMUL(LDA(t, 2, t), sr);
}

static inline V BYTWJ1(const R *t, V sr)
{
     return VZMULJ(LDA(t, 2, t), sr);
}

/* twiddle storage #2: twice the space, faster (when in cache) */
#ifdef FFTW_SINGLE
# define VTW2(v,x)							     \
   {TW_COS, v  ,  x}, {TW_COS, v  , x}, {TW_COS, v+1,  x}, {TW_COS, v+1, x}, \
   {TW_COS, v+2,  x}, {TW_COS, v+2, x}, {TW_COS, v+3,  x}, {TW_COS, v+3, x}, \
   {TW_COS, v+4,  x}, {TW_COS, v+4, x}, {TW_COS, v+5,  x}, {TW_COS, v+5, x}, \
   {TW_COS, v+6,  x}, {TW_COS, v+6, x}, {TW_COS, v+7,  x}, {TW_COS, v+7, x}, \
   {TW_SIN, v  , -x}, {TW_SIN, v  , x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}, \
   {TW_SIN, v+2, -x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, -x}, {TW_SIN, v+3, x}, \
   {TW_SIN, v+4, -x}, {TW_SIN, v+4, x}, {TW_SIN, v+5, -x}, {TW_SIN, v+5, x}, \
   {TW_SIN, v+6, -x}, {TW_SIN, v+6, x}, {TW_SIN, v+7, -x}, {TW_SIN, v+7, x}
#else /* !FFTW_SINGLE */
# define VTW2(v,x)							     \
   {TW_COS, v  ,  x}, {TW_COS, v  , x}, {TW_COS, v+1,  x}, {TW_COS, v+1, x}, \
   {TW_COS, v+2,  x}, {TW_COS, v+2, x}, {TW_COS, v+3,  x}, {TW_COS, v+3, x}, \
   {TW_SIN, v  , -x}, {TW_SIN, v  , x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}, \
   {TW_SIN, v+2, -x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, -x}, {TW_SIN, v+3, x}
#endif /* FFTW_SINGLE */
#define TWVL2 (2 * VL)

static inline V BYTW2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
/*      V tr = LD(t, 2, t), ti = LD(t + VL, 2, t + VL); */
     return VFMA(tr, sr, VMUL(ti, si));
}

static inline V BYTWJ2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
/*      V tr = LD(t, 2, t), ti = LD(t + VL, 2, t + VL); */
     return VFNMS(ti, si, VMUL(tr, sr));
}

/* twiddle storage #3 */
#define VTW3(v,x) VTW1(v,x)
#define TWVL3 TWVL1

/* twiddle storage for split arrays */
#ifdef FFTW_SINGLE
# define VTWS(v,x)                                                            \
  {TW_COS, v   , x}, {TW_COS, v+1 , x}, {TW_COS, v+2 , x}, {TW_COS, v+3 , x}, \
  {TW_COS, v+4 , x}, {TW_COS, v+5 , x}, {TW_COS, v+6 , x}, {TW_COS, v+7 , x}, \
  {TW_COS, v+8 , x}, {TW_COS, v+9 , x}, {TW_COS, v+10, x}, {TW_COS, v+11, x}, \
  {TW_COS, v+12, x}, {TW_COS, v+13, x}, {TW_COS, v+14, x}, {TW_COS, v+15, x}, \
  {TW_SIN, v   , x}, {TW_SIN, v+1 , x}, {TW_SIN, v+2 , x}, {TW_SIN, v+3 , x}, \
  {TW_SIN, v+4 , x}, {TW_SIN, v+5 , x}, {TW_SIN, v+6 , x}, {TW_SIN, v+7 , x}, \
  {TW_SIN, v+8 , x}, {TW_SIN, v+9 , x}, {TW_SIN, v+10, x}, {TW_SIN, v+11, x}, \
  {TW_SIN, v+12, x}, {TW_SIN, v+13, x}, {TW_SIN, v+14, x}, {TW_SIN, v+15, x}
#else /* !FFTW_SINGLE */
# define VTWS(v,x)							  \
  {TW_COS, v  , x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
  {TW_COS, v+4, x}, {TW_COS, v+5, x}, {TW_COS, v+6, x}, {TW_COS, v+7, x}, \
  {TW_SIN, v  , x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}, \
  {TW_SIN, v+4, x}, {TW_SIN, v+5, x}, {TW_SIN, v+6, x}, {TW_SIN, v+7, x}
#endif /* FFTW_SINGLE */
#define TWVLS (2 * VL)

#define VLEAVE _mm256_zeroupper

#include "simd-common.h"
