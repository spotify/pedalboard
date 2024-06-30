/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * Double-precision support added by Romain Dolbeau.
 * Romain Dolbeau hereby places his modifications in the public domain.
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

#if !defined(FFTW_SINGLE) && !defined( __aarch64__)
#error "NEON only works in single precision on 32 bits ARM"
#endif
#if defined(FFTW_LDOUBLE) || defined(FFTW_QUAD)
#error "NEON only works in single or double precision"
#endif

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define SUFF(name) name ## _f32
#else
#  define DS(d,s) d /* double-precision option */
#  define SUFF(name) name ## _f64
#endif

/* define these unconditionally, because they are used by
   taint.c which is compiled without neon */
#define SIMD_SUFFIX _neon	/* for renaming */
#define VL DS(1,2)            /* SIMD complex vector length */
#define SIMD_VSTRIDE_OKA(x) DS(SIMD_STRIDE_OKA(x),((x) == 2))
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

#if defined(__GNUC__) && !defined(__ARM_NEON__) && !defined(__ARM_NEON)
#error "compiling simd-neon.h requires -mfpu=neon or equivalent"
#endif

#include <arm_neon.h>

/* FIXME: I am not sure whether this code assumes little-endian
   ordering.  VLIT may or may not be wrong for big-endian systems. */
typedef DS(float64x2_t, float32x4_t) V;

#ifdef FFTW_SINGLE
#  define VLIT(x0, x1) {x0, x1, x0, x1}
#else
#  define VLIT(x0, x1) {x0, x1}
#endif
#define LDK(x) x
#define DVK(var, val) const V var = VLIT(val, val)

/* NEON has FMA, but a three-operand FMA is not too useful
   for FFT purposes.  We normally compute

      t0=a+b*c
      t1=a-b*c

   In a three-operand instruction set this translates into

      t0=a
      t0+=b*c
      t1=a
      t1-=b*c

   At least one move must be implemented, negating the advantage of
   the FMA in the first place.  At least some versions of gcc generate
   both moves.  So we are better off generating t=b*c;t0=a+t;t1=a-t;*/
#if ARCH_PREFERS_FMA
#warning "--enable-fma on NEON is probably a bad idea (see source code)"
#endif

#define VADD(a, b) SUFF(vaddq)(a, b)
#define VSUB(a, b) SUFF(vsubq)(a, b)
#define VMUL(a, b) SUFF(vmulq)(a, b)
#define VFMA(a, b, c) SUFF(vmlaq)(c, a, b)	        /* a*b+c */
#define VFNMS(a, b, c) SUFF(vmlsq)(c, a, b)	/* FNMS=-(a*b-c) in powerpc terminology; MLS=c-a*b
						   in ARM terminology */
#define VFMS(a, b, c) VSUB(VMUL(a, b), c)	/* FMS=a*b-c in powerpc terminology; no equivalent
						   arm instruction (?) */

#define STOREH(a, v) SUFF(vst1)((a), SUFF(vget_high)(v))
#define STOREL(a, v) SUFF(vst1)((a), SUFF(vget_low)(v))

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
     (void) aligned_like;	/* UNUSED */
     return SUFF(vld1q)(x);
}
static inline void STA(R *x, V v, INT ovs, const R *aligned_like)
{
     (void) aligned_like;	/* UNUSED */
     SUFF(vst1q)(x, v);
}


#ifdef FFTW_SINGLE
static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
     (void) aligned_like;	/* UNUSED */
     return SUFF(vcombine)(SUFF(vld1)(x), SUFF(vld1)((x + ivs)));
}
static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
     (void) aligned_like;	/* UNUSED */
     /* WARNING: the extra_iter hack depends upon store-low occurring
	after store-high */
     STOREH(x + ovs, v);
     STOREL(x,v);
}
#else /* !FFTW_SINGLE */
#  define LD LDA
#  define ST STA
#endif

/* 2x2 complex transpose and store */
#define STM2 DS(STA,ST)
#define STN2(x, v0, v1, ovs) /* nop */

#ifdef FFTW_SINGLE
/* store and 4x4 real transpose */
static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
     (void) aligned_like;	/* UNUSED */
     SUFF(vst1_lane)((x)      , SUFF(vget_low)(v), 0);
     SUFF(vst1_lane)((x + ovs), SUFF(vget_low)(v), 1);
     SUFF(vst1_lane)((x + 2 * ovs), SUFF(vget_high)(v), 0);
     SUFF(vst1_lane)((x + 3 * ovs), SUFF(vget_high)(v), 1);
}
#define STN4(x, v0, v1, v2, v3, ovs)	/* use STM4 */
#else /* !FFTW_SINGLE */
static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     STOREL(x, v);
     STOREH(x + ovs, v);
}
#  define STN4(x, v0, v1, v2, v3, ovs) /* nothing */
#endif

#ifdef FFTW_SINGLE
#define FLIP_RI(x) SUFF(vrev64q)(x)
#else
/* FIXME */
#define FLIP_RI(x) SUFF(vcombine)(SUFF(vget_high)(x), SUFF(vget_low)(x))
#endif

static inline V VCONJ(V x)
{
#ifdef FFTW_SINGLE
     static const uint32x4_t pm = {0, 0x80000000u, 0, 0x80000000u};
     return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(x), pm));
#else
    static const uint64x2_t pm = {0, 0x8000000000000000ull};
    /* Gcc-4.9.2 still does not include vreinterpretq_f64_u64, but simple
     * casts generate the correct assembly.
     */
    return (float64x2_t)(veorq_u64((uint64x2_t)(x), (uint64x2_t)(pm)));
#endif
}

static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}

static inline V VFMAI(V b, V c)
{
     const V mp = VLIT(-1.0, 1.0);
     return VFMA(FLIP_RI(b), mp, c);
}

static inline V VFNMSI(V b, V c)
{
     const V mp = VLIT(-1.0, 1.0);
     return VFNMS(FLIP_RI(b), mp, c);
}

static inline V VFMACONJ(V b, V c)
{
     const V pm = VLIT(1.0, -1.0);
     return VFMA(b, pm, c);
}

static inline V VFNMSCONJ(V b, V c)
{
     const V pm = VLIT(1.0, -1.0);
     return VFNMS(b, pm, c);
}

static inline V VFMSCONJ(V b, V c)
{
     return VSUB(VCONJ(b), c);
}

#ifdef FFTW_SINGLE
#if 1
#define VEXTRACT_REIM(tr, ti, tx)                               \
{                                                               \
     tr = SUFF(vcombine)(SUFF(vdup_lane)(SUFF(vget_low)(tx), 0),      \
                       SUFF(vdup_lane)(SUFF(vget_high)(tx), 0));    \
     ti = SUFF(vcombine)(SUFF(vdup_lane)(SUFF(vget_low)(tx), 1),      \
                       SUFF(vdup_lane)(SUFF(vget_high)(tx), 1));    \
}
#else
/* this alternative might be faster in an ideal world, but gcc likes
   to spill VVV onto the stack */
#define VEXTRACT_REIM(tr, ti, tx)               \
{                                               \
     float32x4x2_t vvv = SUFF(vtrnq)(tx, tx);     \
     tr = vvv.val[0];                           \
     ti = vvv.val[1];                           \
}
#endif
#else
#define VEXTRACT_REIM(tr, ti, tx)                               \
{                                                               \
  tr = SUFF(vtrn1q)(tx, tx);                                    \
  ti = SUFF(vtrn2q)(tx, tx);                                    \
}
#endif

static inline V VZMUL(V tx, V sr)
{
     V tr, ti;
     VEXTRACT_REIM(tr, ti, tx);
     tr = VMUL(sr, tr);
     sr = VBYI(sr);
     return VFMA(ti, sr, tr);
}

static inline V VZMULJ(V tx, V sr)
{
     V tr, ti;
     VEXTRACT_REIM(tr, ti, tx);
     tr = VMUL(sr, tr);
     sr = VBYI(sr);
     return VFNMS(ti, sr, tr);
}

static inline V VZMULI(V tx, V sr)
{
     V tr, ti;
     VEXTRACT_REIM(tr, ti, tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VFMS(tr, sr, ti);
}

static inline V VZMULIJ(V tx, V sr)
{
     V tr, ti;
     VEXTRACT_REIM(tr, ti, tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VFMA(tr, sr, ti);
}

/* twiddle storage #1: compact, slower */
#ifdef FFTW_SINGLE
#define VTW1(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#else
#define VTW1(v,x) {TW_CEXP, v, x}
#endif
#define TWVL1 VL
static inline V BYTW1(const R *t, V sr)
{
     V tx = LDA(t, 2, 0);
     return VZMUL(tx, sr);
}

static inline V BYTWJ1(const R *t, V sr)
{
     V tx = LDA(t, 2, 0);
     return VZMULJ(tx, sr);
}

/* twiddle storage #2: twice the space, faster (when in cache) */
#ifdef FFTW_SINGLE
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
  {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
#else
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_SIN, v, -x}, {TW_SIN, v, x}
#endif
#define TWVL2 (2 * VL)

static inline V BYTW2(const R *t, V sr)
{
     V si = FLIP_RI(sr);
     V tr = LDA(t, 2, 0), ti = LDA(t+2*VL, 2, 0);
     return VFMA(ti, si, VMUL(tr, sr));
}

static inline V BYTWJ2(const R *t, V sr)
{
     V si = FLIP_RI(sr);
     V tr = LDA(t, 2, 0), ti = LDA(t+2*VL, 2, 0);
     return VFNMS(ti, si, VMUL(tr, sr));
}

/* twiddle storage #3 */
#ifdef FFTW_SINGLE
#  define VTW3(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#else
#  define VTW3(v,x) {TW_CEXP, v, x}
#endif
#  define TWVL3 (VL)

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

#define VLEAVE()		/* nothing */

#include "simd-common.h"
