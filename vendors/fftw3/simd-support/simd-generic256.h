/*
 * Copyright (c) 2003, 2007-11 Matteo Frigo
 * Copyright (c) 2003, 2007-11 Massachusetts Institute of Technology
 *
 * Generic256d added by Romain Dolbeau, and turned into simd-generic256.h
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
#  error "Generic simd256 only works in single or double precision"
#endif

#define SIMD_SUFFIX  _generic_simd256  /* for renaming */

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define VDUPL(x) {x[0],x[0],x[2],x[2],x[4],x[4],x[6],x[6]}
#  define VDUPH(x) {x[1],x[1],x[3],x[3],x[5],x[5],x[7],x[7]}
#  define DVK(var, val) V var = {val,val,val,val,val,val,val,val}
#else
#  define DS(d,s) d /* double-precision option */
#  define VDUPL(x) {x[0],x[0],x[2],x[2]}
#  define VDUPH(x) {x[1],x[1],x[3],x[3]}
#  define DVK(var, val) V var = {val, val, val, val}
#endif

#define VL DS(2,4)         /* SIMD vector length, in term of complex numbers */
#define SIMD_VSTRIDE_OKA(x) DS(SIMD_STRIDE_OKA(x),((x) == 2))     
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

typedef DS(double,float) V __attribute__ ((vector_size(32)));

#define VADD(a,b) ((a)+(b))
#define VSUB(a,b) ((a)-(b))
#define VMUL(a,b) ((a)*(b))

#define LDK(x) x

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
    V var;
    (void)aligned_like; /* UNUSED */
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
    V var;
    (void)aligned_like; /* UNUSED */
    var[0] = x[0];
    var[1] = x[1];
    var[2] = x[ivs];
    var[3] = x[ivs+1];
#ifdef FFTW_SINGLE
    var[4] = x[2*ivs];
    var[5] = x[2*ivs+1];
    var[6] = x[3*ivs];
    var[7] = x[3*ivs+1];
#endif
    return var;
}


/* ST has to be separate due to the storage hack requiring reverse order */

static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
#ifdef FFTW_SINGLE
    *(x + 3*ovs    ) = v[6];
    *(x + 3*ovs + 1) = v[7];
    *(x + 2*ovs    ) = v[4];
    *(x + 2*ovs + 1) = v[5];
    *(x + ovs      ) = v[2];
    *(x + ovs   + 1) = v[3];
    *(x            ) = v[0];
    *(x         + 1) = v[1];
#else
    *(x  +  ovs    ) = v[2];
    *(x  +  ovs + 1) = v[3];
    *(x            ) = v[0];
    *(x         + 1) = v[1];
#endif
}

#ifdef FFTW_SINGLE
#define STM2(x, v, ovs, a) /* no-op */
static inline void STN2(R *x, V v0, V v1, INT ovs)
{
    x[        0] = v0[0];
    x[        1] = v0[1];
    x[        2] = v1[0];
    x[        3] = v1[1];
    x[  ovs    ] = v0[2];
    x[  ovs + 1] = v0[3];
    x[  ovs + 2] = v1[2];
    x[  ovs + 3] = v1[3];
    x[2*ovs    ] = v0[4];
    x[2*ovs + 1] = v0[5];
    x[2*ovs + 2] = v1[4];
    x[2*ovs + 3] = v1[5];
    x[3*ovs    ] = v0[6];
    x[3*ovs + 1] = v0[7];
    x[3*ovs + 2] = v1[6];
    x[3*ovs + 3] = v1[7];
}

#  define STM4(x, v, ovs, aligned_like) /* no-op */
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
    *(x + 4 * ovs    ) = v0[4];
    *(x + 4 * ovs + 1) = v1[4];
    *(x + 4 * ovs + 2) = v2[4];
    *(x + 4 * ovs + 3) = v3[4];
    *(x + 5 * ovs    ) = v0[5];
    *(x + 5 * ovs + 1) = v1[5];
    *(x + 5 * ovs + 2) = v2[5];
    *(x + 5 * ovs + 3) = v3[5];
    *(x + 6 * ovs    ) = v0[6];
    *(x + 6 * ovs + 1) = v1[6];
    *(x + 6 * ovs + 2) = v2[6];
    *(x + 6 * ovs + 3) = v3[6];
    *(x + 7 * ovs    ) = v0[7];
    *(x + 7 * ovs + 1) = v1[7];
    *(x + 7 * ovs + 2) = v2[7];
    *(x + 7 * ovs + 3) = v3[7];
}

#else
/* FFTW_DOUBLE */

#define STM2 ST
#define STN2(x, v0, v1, ovs) /* nop */
#define STM4(x, v, ovs, aligned_like) /* no-op */

static inline void STN4(R *x, V v0, V v1, V v2, V v3, INT ovs) {
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
#endif

static inline V FLIP_RI(V x)
{
#ifdef FFTW_SINGLE
    return (V){x[1],x[0],x[3],x[2],x[5],x[4],x[7],x[6]};
#else
    return (V){x[1],x[0],x[3],x[2]};
#endif
}

static inline V VCONJ(V x)
{
#ifdef FFTW_SINGLE
    return (x * (V){1.0,-1.0,1.0,-1.0,1.0,-1.0,1.0,-1.0});
#else
    return (x * (V){1.0,-1.0,1.0,-1.0});
#endif
}

static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
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
# define VTW1(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}, {TW_CEXP, v+2, x}, {TW_CEXP, v+3, x}
#else
# define VTW1(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#endif
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
# define VTW2(v,x)                                                      \
   {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},  \
   {TW_COS, v+2, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, {TW_COS, v+3, x}, \
   {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}, \
   {TW_SIN, v+2, -x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, -x}, {TW_SIN, v+3, x}
#else
# define VTW2(v,x)                                                      \
   {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},  \
   {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
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
#define VTW3 VTW1
#define TWVL3 TWVL1

/* twiddle storage for split arrays */
#ifdef FFTW_SINGLE
# define VTWS(v,x)                                                      \
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
  {TW_COS, v+4, x}, {TW_COS, v+5, x}, {TW_COS, v+6, x}, {TW_COS, v+7, x}, \
  {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}, \
  {TW_SIN, v+4, x}, {TW_SIN, v+5, x}, {TW_SIN, v+6, x}, {TW_SIN, v+7, x}
#else
# define VTWS(v,x)                                                      \
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
  {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}  
#endif
#define TWVLS (2 * VL)

#define VLEAVE() /* nothing */

#include "simd-common.h"
