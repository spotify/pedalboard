/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * Generic128d added by Romain Dolbeau, and turned into simd-generic128.h
 * with single & double precision by Erik Lindahl.
 * Romain Dolbeau hereby places his modifications in the public domain.
 * Erik Lindahl hereby places his modifications in the public domain.
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
#  error "Generic simd128 only works in single or double precision"
#endif

#define SIMD_SUFFIX  _generic_simd128  /* for renaming */

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define VDUPL(x) (V){x[0],x[0],x[2],x[2]}
#  define VDUPH(x) (V){x[1],x[1],x[3],x[3]}
#  define DVK(var, val) V var = {val,val,val,val}
#else
#  define DS(d,s) d /* double-precision option */
#  define VDUPL(x) (V){x[0],x[0]}
#  define VDUPH(x) (V){x[1],x[1]}
#  define DVK(var, val) V var = {val, val}
#endif

#define VL DS(1,2)         /* SIMD vector length, in term of complex numbers */
#define SIMD_VSTRIDE_OKA(x) DS(SIMD_STRIDE_OKA(x),((x) == 2))
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

typedef DS(double,float) V __attribute__ ((vector_size(16)));

#define VADD(a,b) ((a)+(b))
#define VSUB(a,b) ((a)-(b))
#define VMUL(a,b) ((a)*(b))


#define LDK(x) x

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ivs; /* UNUSED */
     return *(const V *)x;
}

static inline void STA(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ovs; /* UNUSED */
     *(V *)x = v;
}

static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
    (void)aligned_like; /* UNUSED */
    V res;
    res[0] = x[0];
    res[1] = x[1];
#ifdef FFTW_SINGLE
    res[2] = x[ivs];
    res[3] = x[ivs+1];
#endif
    return res;
}

#ifdef FFTW_SINGLE
/* ST has to be separate due to the storage hack requiring reverse order */
static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
    (void)aligned_like; /* UNUSED */
    (void)ovs; /* UNUSED */
    *(x + ovs    ) = v[2];
    *(x + ovs + 1) = v[3];
    *(x    ) = v[0];
    *(x + 1) = v[1];
}
#else
/* FFTW_DOUBLE */
#  define ST STA
#endif

#ifdef FFTW_SINGLE
#define STM2 ST
#define STN2(x, v0, v1, ovs) /* nop */

static inline void STN4(R *x, V v0, V v1, V v2, V v3, INT ovs)
{
    *(x              ) = v0[0];
    *(x           + 1) = v1[0];
    *(x           + 2) = v2[0];
    *(x           + 3) = v3[0];
    *(x     + ovs    ) = v0[1];
    *(x     + ovs + 1) = v1[1];
    *(x     + ovs + 2) = v2[1];
    *(x     + ovs + 3) = v3[1];
    *(x + 2 * ovs    ) = v0[2];
    *(x + 2 * ovs + 1) = v1[2];
    *(x + 2 * ovs + 2) = v2[2];
    *(x + 2 * ovs + 3) = v3[2];
    *(x + 3 * ovs    ) = v0[3];
    *(x + 3 * ovs + 1) = v1[3];
    *(x + 3 * ovs + 2) = v2[3];
    *(x + 3 * ovs + 3) = v3[3];
}
#define STM4(x, v, ovs, aligned_like) /* no-op */


#else
/* FFTW_DOUBLE */

#define STM2 STA
#define STN2(x, v0, v1, ovs) /* nop */

static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     *(x) = v[0];
     *(x+ovs) = v[1];
}
#  define STN4(x, v0, v1, v2, v3, ovs) /* nothing */
#endif


static inline V FLIP_RI(V x)
{
#ifdef FFTW_SINGLE
    return (V){x[1],x[0],x[3],x[2]};
#else
    return (V){x[1],x[0]};
#endif
}

static inline V VCONJ(V x)
{
#ifdef FFTW_SINGLE
    return (V){x[0],-x[1],x[2],-x[3]};
#else
    return (V){x[0],-x[1]};
#endif
}

static inline V VBYI(V x)
{
     x = VCONJ(x);
     x = FLIP_RI(x);
     return x;
}

/* FMA support */
#define VFMA(a, b, c) VADD(c, VMUL(a, b))
#define VFNMS(a, b, c) VSUB(c, VMUL(a, b))
#define VFMS(a, b, c) VSUB(VMUL(a, b), c)
#define VFMAI(b, c) VADD(c, VBYI(b))
#define VFNMSI(b, c) VSUB(c, VBYI(b))
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
  {TW_CEXP, v, x}, {TW_CEXP, v+1, x}     
static inline V BYTW1(const R *t, V sr)
{
    return VZMUL(LDA(t, 2, t), sr);
}
static inline V BYTWJ1(const R *t, V sr)
{
    return VZMULJ(LDA(t, 2, t), sr);
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
#  define VTW2(v,x)                                                     \
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},   \
  {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
#else /* !FFTW_SINGLE */
#  define VTW2(v,x)                                                     \
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_SIN, v, -x}, {TW_SIN, v, x}
#endif
#define TWVL2 (2 * VL)
static inline V BYTW2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VFMA(tr, sr, VMUL(ti, si));
}
static inline V BYTWJ2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VFNMS(ti, si, VMUL(tr, sr));
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
#  define VTWS(v,x)                                                       \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
    {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}
#else
#  define VTWS(v,x)                                                       \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
#endif
#define TWVLS (2 * VL)

#define VLEAVE() /* nothing */

#include "simd-common.h"
