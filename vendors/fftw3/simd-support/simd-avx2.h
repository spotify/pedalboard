/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * Modifications by Romain Dolbeau & Erik Lindahl, derived from simd-avx.h
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
#error "AVX2 only works in single or double precision"
#endif

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define SUFF(name) name ## s
#else
#  define DS(d,s) d /* double-precision option */
#  define SUFF(name) name ## d
#endif

#define SIMD_SUFFIX  _avx2  /* for renaming */
#define VL DS(2, 4)        /* SIMD complex vector length */
#define SIMD_VSTRIDE_OKA(x) ((x) == 2) 
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

#if defined(__GNUC__) && !defined(__AVX2__) /* sanity check */
#error "compiling simd-avx2.h without avx2 support"
#endif

#ifdef _MSC_VER
#ifndef inline
#define inline __inline
#endif
#endif

#include <immintrin.h>

typedef DS(__m256d, __m256) V;
#define VADD SUFF(_mm256_add_p)
#define VSUB SUFF(_mm256_sub_p)
#define VMUL SUFF(_mm256_mul_p)
#define VXOR SUFF(_mm256_xor_p)
#define VSHUF SUFF(_mm256_shuffle_p)
#define VPERM1 SUFF(_mm256_permute_p)

#define SHUFVALD(fp0,fp1) \
   (((fp1) << 3) | ((fp0) << 2) | ((fp1) << 1) | ((fp0)))
#define SHUFVALS(fp0,fp1,fp2,fp3) \
   (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

#define VDUPL(x) DS(_mm256_movedup_pd(x), _mm256_moveldup_ps(x))
#define VDUPH(x) DS(_mm256_permute_pd(x,SHUFVALD(1,1)), _mm256_movehdup_ps(x))

#define VLIT(x0, x1) DS(_mm256_set_pd(x0, x1, x0, x1), _mm256_set_ps(x0, x1, x0, x1, x0, x1, x0, x1))
#define DVK(var, val) V var = VLIT(val, val)
#define LDK(x) x

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ivs; /* UNUSED */
     return SUFF(_mm256_loadu_p)(x);
}

static inline void STA(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ovs; /* UNUSED */
     SUFF(_mm256_storeu_p)(x, v);
}

#if FFTW_SINGLE

#  ifdef _MSC_VER
     /* Temporarily disable the warning "uninitialized local variable
	'name' used" and runtime checks for using a variable before it is
	defined which is erroneously triggered by the LOADL0 / LOADH macros
	as they only modify VAL partly each. */
#    ifndef __INTEL_COMPILER
#      pragma warning(disable : 4700)
#      pragma runtime_checks("u", off)
#    endif
#  endif
#  ifdef __INTEL_COMPILER
#    pragma warning(disable : 592)
#  endif

#define LOADH(addr, val) _mm_loadh_pi(val, (const __m64 *)(addr))
#define LOADL(addr, val) _mm_loadl_pi(val, (const __m64 *)(addr))
#define STOREH(addr, val) _mm_storeh_pi((__m64 *)(addr), val)
#define STOREL(addr, val) _mm_storel_pi((__m64 *)(addr), val)

static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
     __m128 l0, l1, h0, h1;
     (void)aligned_like; /* UNUSED */
#if defined(__ICC) || (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
     l0 = LOADL(x, SUFF(_mm_undefined_p)());
     l1 = LOADL(x + ivs, SUFF(_mm_undefined_p)());
     h0 = LOADL(x + 2*ivs, SUFF(_mm_undefined_p)());
     h1 = LOADL(x + 3*ivs, SUFF(_mm_undefined_p)());
#else
     l0 = LOADL(x, l0);
     l1 = LOADL(x + ivs, l1);
     h0 = LOADL(x + 2*ivs, h0);
     h1 = LOADL(x + 3*ivs, h1);
#endif
     l0 = SUFF(_mm_movelh_p)(l0,l1);
     h0 = SUFF(_mm_movelh_p)(h0,h1);
     return _mm256_insertf128_ps(_mm256_castps128_ps256(l0), h0, 1);
}

#  ifdef _MSC_VER
#    ifndef __INTEL_COMPILER
#      pragma warning(default : 4700)
#      pragma runtime_checks("u", restore)
#    endif
#  endif
#  ifdef __INTEL_COMPILER
#    pragma warning(default : 592)
#  endif

static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
     __m128 h = _mm256_extractf128_ps(v, 1);
     __m128 l = _mm256_castps256_ps128(v);
     (void)aligned_like; /* UNUSED */
     /* WARNING: the extra_iter hack depends upon STOREL occurring
	after STOREH */
     STOREH(x + 3*ovs, h);
     STOREL(x + 2*ovs, h);
     STOREH(x + ovs, l);
     STOREL(x, l);
}

#define STM2(x, v, ovs, aligned_like) /* no-op */
static inline void STN2(R *x, V v0, V v1, INT ovs)
{
    V x0 = VSHUF(v0, v1, SHUFVALS(0, 1, 0, 1));
    V x1 = VSHUF(v0, v1, SHUFVALS(2, 3, 2, 3));
    __m128 h0 = _mm256_extractf128_ps(x0, 1);
    __m128 l0 = _mm256_castps256_ps128(x0);
    __m128 h1 = _mm256_extractf128_ps(x1, 1);
    __m128 l1 = _mm256_castps256_ps128(x1);
    *(__m128 *)(x + 3*ovs) = h1;
    *(__m128 *)(x + 2*ovs) = h0;
    *(__m128 *)(x + 1*ovs) = l1;
    *(__m128 *)(x + 0*ovs) = l0;
}

#define STM4(x, v, ovs, aligned_like) /* no-op */
#define STN4(x, v0, v1, v2, v3, ovs)				\
{								\
     V xxx0, xxx1, xxx2, xxx3;					\
     V yyy0, yyy1, yyy2, yyy3;					\
     xxx0 = _mm256_unpacklo_ps(v0, v2);				\
     xxx1 = _mm256_unpackhi_ps(v0, v2);				\
     xxx2 = _mm256_unpacklo_ps(v1, v3);				\
     xxx3 = _mm256_unpackhi_ps(v1, v3);				\
     yyy0 = _mm256_unpacklo_ps(xxx0, xxx2);			\
     yyy1 = _mm256_unpackhi_ps(xxx0, xxx2);			\
     yyy2 = _mm256_unpacklo_ps(xxx1, xxx3);			\
     yyy3 = _mm256_unpackhi_ps(xxx1, xxx3);			\
     *(__m128 *)(x + 0 * ovs) = _mm256_castps256_ps128(yyy0);	\
     *(__m128 *)(x + 4 * ovs) = _mm256_extractf128_ps(yyy0, 1);	\
     *(__m128 *)(x + 1 * ovs) = _mm256_castps256_ps128(yyy1);	\
     *(__m128 *)(x + 5 * ovs) = _mm256_extractf128_ps(yyy1, 1);	\
     *(__m128 *)(x + 2 * ovs) = _mm256_castps256_ps128(yyy2);	\
     *(__m128 *)(x + 6 * ovs) = _mm256_extractf128_ps(yyy2, 1);	\
     *(__m128 *)(x + 3 * ovs) = _mm256_castps256_ps128(yyy3);	\
     *(__m128 *)(x + 7 * ovs) = _mm256_extractf128_ps(yyy3, 1);	\
}

#else
static inline __m128d VMOVAPD_LD(const R *x)
{
     /* gcc-4.6 miscompiles the combination _mm256_castpd128_pd256(VMOVAPD_LD(x))
	into a 256-bit vmovapd, which requires 32-byte aligment instead of
	16-byte alignment.

	Force the use of vmovapd via asm until compilers stabilize.
     */
#if defined(__GNUC__)
     __m128d var;
     __asm__("vmovapd %1, %0\n" : "=x"(var) : "m"(x[0]));
     return var;
#else
     return *(const __m128d *)x;
#endif
}

static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
     V var;
     (void)aligned_like; /* UNUSED */
     var = _mm256_castpd128_pd256(VMOVAPD_LD(x));
     var = _mm256_insertf128_pd(var, *(const __m128d *)(x+ivs), 1);
     return var;
}

static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     /* WARNING: the extra_iter hack depends upon the store of the low
	part occurring after the store of the high part */
     *(__m128d *)(x + ovs) = _mm256_extractf128_pd(v, 1);
     *(__m128d *)x = _mm256_castpd256_pd128(v);
}


#define STM2 ST
#define STN2(x, v0, v1, ovs) /* nop */
#define STM4(x, v, ovs, aligned_like) /* no-op */

/* STN4 is a macro, not a function, thanks to Visual C++ developers
   deciding "it would be infrequent that people would want to pass more
   than 3 [__m128 parameters] by value."  Even though the comment
   was made about __m128 parameters, it appears to apply to __m256
   parameters as well. */
#define STN4(x, v0, v1, v2, v3, ovs)					\
{									\
     V xxx0, xxx1, xxx2, xxx3;						\
     xxx0 = _mm256_unpacklo_pd(v0, v1);					\
     xxx1 = _mm256_unpackhi_pd(v0, v1);					\
     xxx2 = _mm256_unpacklo_pd(v2, v3);					\
     xxx3 = _mm256_unpackhi_pd(v2, v3);					\
     STA(x,           _mm256_permute2f128_pd(xxx0, xxx2, 0x20), 0, 0); \
     STA(x +     ovs, _mm256_permute2f128_pd(xxx1, xxx3, 0x20), 0, 0); \
     STA(x + 2 * ovs, _mm256_permute2f128_pd(xxx0, xxx2, 0x31), 0, 0); \
     STA(x + 3 * ovs, _mm256_permute2f128_pd(xxx1, xxx3, 0x31), 0, 0); \
}
#endif

static inline V FLIP_RI(V x)
{
     return VPERM1(x, DS(SHUFVALD(1, 0), SHUFVALS(1, 0, 3, 2)));
}

static inline V VCONJ(V x)
{
     /* Produce a SIMD vector[VL] of (0 + -0i). 

        We really want to write this:

           V pmpm = VLIT(-0.0, 0.0);

        but historically some compilers have ignored the distiction
        between +0 and -0.  It looks like 'gcc-8 -fast-math' treats -0
        as 0 too.
      */
     union uvec {
          unsigned u[8];
          V v;
     };
     static const union uvec pmpm = {
#ifdef FFTW_SINGLE
          { 0x00000000, 0x80000000, 0x00000000, 0x80000000,
            0x00000000, 0x80000000, 0x00000000, 0x80000000 }
#else
          { 0x00000000, 0x00000000, 0x00000000, 0x80000000,
            0x00000000, 0x00000000, 0x00000000, 0x80000000 }
#endif
     };
     return VXOR(pmpm.v, x);
}

static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}

/* FMA support */
#define VFMA    SUFF(_mm256_fmadd_p)
#define VFNMS   SUFF(_mm256_fnmadd_p)
#define VFMS    SUFF(_mm256_fmsub_p)
#define VFMAI(b, c) SUFF(_mm256_addsub_p)(c, FLIP_RI(b)) /* VADD(c, VBYI(b)) */
#define VFNMSI(b, c)   VSUB(c, VBYI(b))
#define VFMACONJ(b,c)  VADD(VCONJ(b),c)
#define VFMSCONJ(b,c)  VSUB(VCONJ(b),c)
#define VFNMSCONJ(b,c) SUFF(_mm256_addsub_p)(c, b)  /* VSUB(c, VCONJ(b)) */

static inline V VZMUL(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* tr = VMUL(sr, tr); */
     /* sr = VBYI(sr); */
     /* return VFMA(ti, sr, tr); */
     return SUFF(_mm256_fmaddsub_p)(sr, VDUPL(tx), VMUL(FLIP_RI(sr), VDUPH(tx)));
}

static inline V VZMULJ(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* tr = VMUL(sr, tr); */
     /* sr = VBYI(sr); */
     /* return VFNMS(ti, sr, tr); */
     return SUFF(_mm256_fmsubadd_p)(sr, VDUPL(tx), VMUL(FLIP_RI(sr), VDUPH(tx)));
}

static inline V VZMULI(V tx, V sr)
{
     V tr = VDUPL(tx);
     V ti = VDUPH(tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VFMS(tr, sr, ti);
    /*
     * Keep the old version
     * (2 permute, 1 shuffle, 1 constant load (L1), 1 xor, 2 fp), since the below FMA one
     * would be 2 permute, 1 shuffle, 1 xor (setzero), 3 fp), but with a longer pipeline.
     *
     * Alternative new fma version:
     * return SUFF(_mm256_addsub_p)(SUFF(_mm256_fnmadd_p)(sr, VDUPH(tx), SUFF(_mm256_setzero_p)()),
     * VMUL(FLIP_RI(sr), VDUPL(tx)));
    */
}

static inline V VZMULIJ(V tx, V sr)
{
     /* V tr = VDUPL(tx); */
     /* V ti = VDUPH(tx); */
     /* ti = VMUL(ti, sr); */
     /* sr = VBYI(sr); */
     /* return VFMA(tr, sr, ti); */
     return SUFF(_mm256_fmaddsub_p)(sr, VDUPH(tx), VMUL(FLIP_RI(sr), VDUPL(tx)));
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
# define VTW2(v,x)							\
   {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
   {TW_COS, v+2, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, {TW_COS, v+3, x}, \
   {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}, \
   {TW_SIN, v+2, -x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, -x}, {TW_SIN, v+3, x}
#else
# define VTW2(v,x)							\
   {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
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
# define VTWS(v,x)							\
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x},	\
  {TW_COS, v+4, x}, {TW_COS, v+5, x}, {TW_COS, v+6, x}, {TW_COS, v+7, x}, \
  {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x},	\
  {TW_SIN, v+4, x}, {TW_SIN, v+5, x}, {TW_SIN, v+6, x}, {TW_SIN, v+7, x}
#else
# define VTWS(v,x)							\
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x},	\
  {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}	
#endif
#define TWVLS (2 * VL)

#define VLEAVE _mm256_zeroupper

#include "simd-common.h"
