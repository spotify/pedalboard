/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
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

#ifndef FFTW_SINGLE
#error "ALTIVEC only works in single precision"
#endif

/* define these unconditionally, because they are used by
   taint.c which is compiled without altivec */
#define SIMD_SUFFIX _altivec  /* for renaming */
#define VL 2            /* SIMD complex vector length */
#define SIMD_VSTRIDE_OKA(x) ((x) == 2)
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OKA

#if !defined(__VEC__) && !defined(FAKE__VEC__)
#  error "compiling simd-altivec.h requires -maltivec or equivalent"
#endif

#ifdef HAVE_ALTIVEC_H
#  include <altivec.h>
#endif

typedef vector float V;
#define VLIT(x0, x1, x2, x3) {x0, x1, x2, x3}
#define LDK(x) x
#define DVK(var, val) const V var = VLIT(val, val, val, val)

static inline V VADD(V a, V b) { return vec_add(a, b); }
static inline V VSUB(V a, V b) { return vec_sub(a, b); }
static inline V VFMA(V a, V b, V c) { return vec_madd(a, b, c); }
static inline V VFNMS(V a, V b, V c) { return vec_nmsub(a, b, c); }

static inline V VMUL(V a, V b)
{
     DVK(zero, -0.0);
     return VFMA(a, b, zero);
}

static inline V VFMS(V a, V b, V c) { return VSUB(VMUL(a, b), c); }

static inline V LDA(const R *x, INT ivs, const R *aligned_like) 
{
     UNUSED(ivs);
     UNUSED(aligned_like);
     return vec_ld(0, x);
}

static inline V LD(const R *x, INT ivs, const R *aligned_like) 
{
     /* common subexpressions */
     const INT fivs = sizeof(R) * ivs;
       /* you are not expected to understand this: */
     const vector unsigned int perm = VLIT(0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
     vector unsigned char ml = vec_lvsr(fivs + 8, aligned_like);
     vector unsigned char mh = vec_lvsl(0, aligned_like);
     vector unsigned char msk = 
	  (vector unsigned char)vec_sel((V)mh, (V)ml, perm);
     /* end of common subexpressions */

     return vec_perm(vec_ld(0, x), vec_ld(fivs, x), msk);
}

/* store lower half */
static inline void STH(R *x, V v, R *aligned_like)
{
     v = vec_perm(v, v, vec_lvsr(0, aligned_like));
     vec_ste(v, 0, x);
     vec_ste(v, sizeof(R), x);
}

static inline void STL(R *x, V v, INT ovs, R *aligned_like)
{
     const INT fovs = sizeof(R) * ovs;
     v = vec_perm(v, v, vec_lvsr(fovs + 8, aligned_like));
     vec_ste(v, fovs, x);
     vec_ste(v, sizeof(R) + fovs, x);
}

static inline void STA(R *x, V v, INT ovs, R *aligned_like) 
{
     UNUSED(ovs);
     UNUSED(aligned_like);
     vec_st(v, 0, x);
}

static inline void ST(R *x, V v, INT ovs, R *aligned_like) 
{
     /* WARNING: the extra_iter hack depends upon STH occurring after
	STL */
     STL(x, v, ovs, aligned_like);
     STH(x, v, aligned_like);
}

#define STM2(x, v, ovs, aligned_like) /* no-op */

static inline void STN2(R *x, V v0, V v1, INT ovs)
{
     const INT fovs = sizeof(R) * ovs;
     const vector unsigned int even = 
	  VLIT(0x00010203, 0x04050607, 0x10111213, 0x14151617);
     const vector unsigned int odd = 
	  VLIT(0x08090a0b, 0x0c0d0e0f, 0x18191a1b, 0x1c1d1e1f);
     vec_st(vec_perm(v0, v1, (vector unsigned char)even), 0, x);
     vec_st(vec_perm(v0, v1, (vector unsigned char)odd), fovs, x);
}

#define STM4(x, v, ovs, aligned_like) /* no-op */

static inline void STN4(R *x, V v0, V v1, V v2, V v3, INT ovs)
{
     const INT fovs = sizeof(R) * ovs;
     V x0 = vec_mergeh(v0, v2);
     V x1 = vec_mergel(v0, v2);
     V x2 = vec_mergeh(v1, v3);
     V x3 = vec_mergel(v1, v3);
     V y0 = vec_mergeh(x0, x2);
     V y1 = vec_mergel(x0, x2);
     V y2 = vec_mergeh(x1, x3);
     V y3 = vec_mergel(x1, x3);
     vec_st(y0, 0, x);
     vec_st(y1, fovs, x);
     vec_st(y2, 2 * fovs, x);
     vec_st(y3, 3 * fovs, x);
}

static inline V FLIP_RI(V x)
{
     const vector unsigned int perm = 
	  VLIT(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);
     return vec_perm(x, x, (vector unsigned char)perm);
}

static inline V VCONJ(V x)
{
     const V pmpm = VLIT(0.0, -0.0, 0.0, -0.0);
     return vec_xor(x, pmpm);
}

static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}

static inline V VFMAI(V b, V c)
{
     const V mpmp = VLIT(-1.0, 1.0, -1.0, 1.0);
     return VFMA(FLIP_RI(b), mpmp, c);
}

static inline V VFNMSI(V b, V c)
{
     const V mpmp = VLIT(-1.0, 1.0, -1.0, 1.0);
     return VFNMS(FLIP_RI(b), mpmp, c);
}

static inline V VFMACONJ(V b, V c)
{
     const V pmpm = VLIT(1.0, -1.0, 1.0, -1.0);
     return VFMA(b, pmpm, c);
}

static inline V VFNMSCONJ(V b, V c)
{
     const V pmpm = VLIT(1.0, -1.0, 1.0, -1.0);
     return VFNMS(b, pmpm, c);
}

static inline V VFMSCONJ(V b, V c)
{
     return VSUB(VCONJ(b), c);
}

static inline V VZMUL(V tx, V sr)
{
     const vector unsigned int real = 
	  VLIT(0x00010203, 0x00010203, 0x08090a0b, 0x08090a0b);
     const vector unsigned int imag = 
	  VLIT(0x04050607, 0x04050607, 0x0c0d0e0f, 0x0c0d0e0f);
     V si = VBYI(sr);
     V tr = vec_perm(tx, tx, (vector unsigned char)real);
     V ti = vec_perm(tx, tx, (vector unsigned char)imag);
     return VFMA(ti, si, VMUL(tr, sr));
}

static inline V VZMULJ(V tx, V sr)
{
     const vector unsigned int real = 
	  VLIT(0x00010203, 0x00010203, 0x08090a0b, 0x08090a0b);
     const vector unsigned int imag = 
	  VLIT(0x04050607, 0x04050607, 0x0c0d0e0f, 0x0c0d0e0f);
     V si = VBYI(sr);
     V tr = vec_perm(tx, tx, (vector unsigned char)real);
     V ti = vec_perm(tx, tx, (vector unsigned char)imag);
     return VFNMS(ti, si, VMUL(tr, sr));
}

static inline V VZMULI(V tx, V si)
{
     const vector unsigned int real = 
	  VLIT(0x00010203, 0x00010203, 0x08090a0b, 0x08090a0b);
     const vector unsigned int imag = 
	  VLIT(0x04050607, 0x04050607, 0x0c0d0e0f, 0x0c0d0e0f);
     V sr = VBYI(si);
     V tr = vec_perm(tx, tx, (vector unsigned char)real);
     V ti = vec_perm(tx, tx, (vector unsigned char)imag);
     return VFNMS(ti, si, VMUL(tr, sr));
}

static inline V VZMULIJ(V tx, V si)
{
     const vector unsigned int real = 
	  VLIT(0x00010203, 0x00010203, 0x08090a0b, 0x08090a0b);
     const vector unsigned int imag = 
	  VLIT(0x04050607, 0x04050607, 0x0c0d0e0f, 0x0c0d0e0f);
     V sr = VBYI(si);
     V tr = vec_perm(tx, tx, (vector unsigned char)real);
     V ti = vec_perm(tx, tx, (vector unsigned char)imag);
     return VFMA(ti, si, VMUL(tr, sr));
}

/* twiddle storage #1: compact, slower */
#define VTW1(v,x) \
 {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
#define TWVL1 (VL)

static inline V BYTW1(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = VBYI(sr);
     V tx = twp[0];
     V tr = vec_mergeh(tx, tx);
     V ti = vec_mergel(tx, tx);
     return VFMA(ti, si, VMUL(tr, sr));
}

static inline V BYTWJ1(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = VBYI(sr);
     V tx = twp[0];
     V tr = vec_mergeh(tx, tx);
     V ti = vec_mergel(tx, tx);
     return VFNMS(ti, si, VMUL(tr, sr));
}

/* twiddle storage #2: twice the space, faster (when in cache) */
#define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
  {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
#define TWVL2 (2 * VL)

static inline V BYTW2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VFMA(ti, si, VMUL(tr, sr));
}

static inline V BYTWJ2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VFNMS(ti, si, VMUL(tr, sr));
}

/* twiddle storage #3 */
#define VTW3(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#define TWVL3 (VL)

/* twiddle storage for split arrays */
#define VTWS(v,x)							\
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x},	\
  {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}
#define TWVLS (2 * VL)

#define VLEAVE() /* nothing */

#include "simd-common.h"
