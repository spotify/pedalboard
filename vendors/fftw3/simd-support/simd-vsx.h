/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * VSX SIMD implementation added 2015 Erik Lindahl.
 * Erik Lindahl places his modifications in the public domain.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if defined(FFTW_LDOUBLE) || defined(FFTW_QUAD)
#  error "VSX only works in single or double precision"
#endif

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define SUFF(name) name ## s
#else
#  define DS(d,s) d /* double-precision option */
#  define SUFF(name) name ## d
#endif

#define SIMD_SUFFIX  _vsx  /* for renaming */
#define VL DS(1,2)         /* SIMD vector length, in term of complex numbers */
#define SIMD_VSTRIDE_OKA(x) DS(SIMD_STRIDE_OKA(x),((x) == 2))
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

#include <altivec.h>
#include <stdio.h>

typedef DS(vector double,vector float) V;

#define VADD(a,b)   vec_add(a,b)
#define VSUB(a,b)   vec_sub(a,b)
#define VMUL(a,b)   vec_mul(a,b)
#define VXOR(a,b)   vec_xor(a,b)
#define UNPCKL(a,b) vec_mergel(a,b)
#define UNPCKH(a,b) vec_mergeh(a,b)
#ifdef FFTW_SINGLE
#    define VDUPL(a)    ({ const vector unsigned char perm = {0,1,2,3,0,1,2,3,8,9,10,11,8,9,10,11}; vec_perm(a,a,perm); })
#    define VDUPH(a)    ({ const vector unsigned char perm = {4,5,6,7,4,5,6,7,12,13,14,15,12,13,14,15}; vec_perm(a,a,perm); })
#else
#    define VDUPL(a)    ({ const vector unsigned char perm = {0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7}; vec_perm(a,a,perm); })
#    define VDUPH(a)    ({ const vector unsigned char perm = {8,9,10,11,12,13,14,15,8,9,10,11,12,13,14,15}; vec_perm(a,a,perm); })
#endif

static inline V LDK(R f) { return vec_splats(f); }

#define DVK(var, val) const R var = K(val)

static inline V VCONJ(V x)
{
  const V pmpm = vec_mergel(vec_splats((R)0.0),-(vec_splats((R)0.0)));
  return vec_xor(x, pmpm);
}

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
#ifdef __ibmxl__
  return vec_xl(0,(DS(double,float) *)x);
#else
  return (*(const V *)(x));
#endif
}

static inline void STA(R *x, V v, INT ovs, const R *aligned_like)
{
#ifdef __ibmxl__
  vec_xst(v,0,x);
#else
  *(V *)x = v;
#endif
}

static inline V FLIP_RI(V x)
{
#ifdef FFTW_SINGLE
  const vector unsigned char perm = { 4,5,6,7,0,1,2,3,12,13,14,15,8,9,10,11 };
#else
  const vector unsigned char perm = { 8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7 };
#endif
  return vec_perm(x,x,perm);
}

#ifdef FFTW_SINGLE

static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
  const vector unsigned char perm = {0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23};

  return vec_perm((vector float)vec_splats(*(double *)(x)),
		  (vector float)vec_splats(*(double *)(x+ivs)),perm);
}

static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
  *(double *)(x+ovs) = vec_extract( (vector double)v, 1 );
  *(double *)x       = vec_extract( (vector double)v, 0 );
}
#else
/* DOUBLE */

#  define LD LDA
#  define ST STA

#endif

#define STM2 DS(STA,ST)
#define STN2(x, v0, v1, ovs) /* nop */

#ifdef FFTW_SINGLE

#  define STM4(x, v, ovs, aligned_like) /* no-op */
static inline void STN4(R *x, V v0, V v1, V v2, V v3, int ovs)
{
    V xxx0, xxx1, xxx2, xxx3;
    xxx0 = vec_mergeh(v0,v1);
    xxx1 = vec_mergel(v0,v1);
    xxx2 = vec_mergeh(v2,v3);
    xxx3 = vec_mergel(v2,v3);
    *(double *)x           = vec_extract( (vector double)xxx0, 0 );
    *(double *)(x+ovs)     = vec_extract( (vector double)xxx0, 1 );
    *(double *)(x+2*ovs)   = vec_extract( (vector double)xxx1, 0 );
    *(double *)(x+3*ovs)   = vec_extract( (vector double)xxx1, 1 );
    *(double *)(x+2)       = vec_extract( (vector double)xxx2, 0 );
    *(double *)(x+ovs+2)   = vec_extract( (vector double)xxx2, 1 );
    *(double *)(x+2*ovs+2) = vec_extract( (vector double)xxx3, 0 );
    *(double *)(x+3*ovs+2) = vec_extract( (vector double)xxx3, 1 );
}
#else /* !FFTW_SINGLE */

static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     x[0]    = vec_extract(v,0);
     x[ovs]  = vec_extract(v,1);
}
#  define STN4(x, v0, v1, v2, v3, ovs) /* nothing */
#endif

static inline V VBYI(V x)
{
     /* FIXME [matteof 2017-09-21] It is possible to use vpermxor(),
        but gcc and xlc treat the permutation bits differently, and
        gcc-6 seems to generate incorrect code when using
        __builtin_crypto_vpermxor() (i.e., VBYI() works for a small
        test case but fails in the large).

        Punt on vpermxor() for now and do the simple thing.
     */
     return FLIP_RI(VCONJ(x));
}

/* FMA support */
#define VFMA(a, b, c)  vec_madd(a,b,c)
#define VFNMS(a, b, c) vec_nmsub(a,b,c)
#define VFMS(a, b, c)  vec_msub(a,b,c)
#define VFMAI(b, c)    VADD(c, VBYI(b))
#define VFNMSI(b, c)   VSUB(c, VBYI(b))
#define VFMACONJ(b,c)  VADD(VCONJ(b),c)
#define VFMSCONJ(b,c)  VSUB(VCONJ(b),c)
#define VFNMSCONJ(b,c) VSUB(c, VCONJ(b))

static inline V VZMUL(V tx, V sr)
{
    V tr = VDUPL(tx);
    V ti = VDUPH(tx);
    tr = VMUL(sr, tr);
    sr = VBYI(sr);
    return VFMA(ti, sr, tr);
}

static inline V VZMULJ(V tx, V sr)
{
    V tr = VDUPL(tx);
    V ti = VDUPH(tx);
    tr = VMUL(sr, tr);
    sr = VBYI(sr);
    return VFNMS(ti, sr, tr);
}

static inline V VZMULI(V tx, V sr)
{
    V tr = VDUPL(tx);
    V ti = VDUPH(tx);
    ti = VMUL(ti, sr);
    sr = VBYI(sr);
    return VFMS(tr, sr, ti);
}

static inline V VZMULIJ(V tx, V sr)
{
    V tr = VDUPL(tx);
    V ti = VDUPH(tx);
    ti = VMUL(ti, sr);
    sr = VBYI(sr);
    return VFMA(tr, sr, ti);
}

/* twiddle storage #1: compact, slower */
#ifdef FFTW_SINGLE
#  define VTW1(v,x)  \
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
static inline V BYTW1(const R *t, V sr)
{
     V tx = LDA(t,0,t);
     V tr = UNPCKH(tx, tx);
     V ti = UNPCKL(tx, tx);
     tr = VMUL(tr, sr);
     sr = VBYI(sr);
     return VFMA(ti, sr, tr);
}
static inline V BYTWJ1(const R *t, V sr)
{
     V tx = LDA(t,0,t);
     V tr = UNPCKH(tx, tx);
     V ti = UNPCKL(tx, tx);
     tr = VMUL(tr, sr);
     sr = VBYI(sr);
     return VFNMS(ti, sr, tr);
}
#else /* !FFTW_SINGLE */
#  define VTW1(v,x) {TW_CEXP, v, x}
static inline V BYTW1(const R *t, V sr)
{
     V tx = LD(t, 1, t);
     return VZMUL(tx, sr);
}
static inline V BYTWJ1(const R *t, V sr)
{
     V tx = LD(t, 1, t);
     return VZMULJ(tx, sr);
}
#endif
#define TWVL1 (VL)

/* twiddle storage #2: twice the space, faster (when in cache) */
#ifdef FFTW_SINGLE
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
  {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
#else /* !FFTW_SINGLE */
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_SIN, v, -x}, {TW_SIN, v, x}
#endif
#define TWVL2 (2 * VL)
static inline V BYTW2(const R *t, V sr)
{
     V si = FLIP_RI(sr);
     V ti = LDA(t+2*VL,0,t);
     V tt = VMUL(ti, si);
     V tr = LDA(t,0,t);
     return VFMA(tr, sr, tt);
}
static inline V BYTWJ2(const R *t, V sr)
{
     V si = FLIP_RI(sr);
     V tr = LDA(t,0,t);
     V tt = VMUL(tr, sr);
     V ti = LDA(t+2*VL,0,t);
     return VFNMS(ti, si, tt);
}

/* twiddle storage #3 */
#ifdef FFTW_SINGLE
#  define VTW3(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#  define TWVL3 (VL)
#else
#  define VTW3(v,x) VTW1(v,x)
#  define TWVL3 TWVL1
#endif

/* twiddle storage for split arrays */
#ifdef FFTW_SINGLE
#  define VTWS(v,x)							  \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
    {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}
#else
#  define VTWS(v,x)							  \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
#endif
#define TWVLS (2 * VL)

#define VLEAVE() /* nothing */

#include "simd-common.h"
