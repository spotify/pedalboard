/*
 * Copyright (c) 2003, 2007-11 Matteo Frigo
 * Copyright (c) 2003, 2007-11 Massachusetts Institute of Technology
 * 
 * Knights Corner Vector Instruction support added by Romain Dolbeau.
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
#error "Knights Corner vector instructions only works in single or double precision"
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

#define SIMD_SUFFIX  _kcvi  /* for renaming */
#define VL DS(4, 8)        /* SIMD complex vector length */
#define SIMD_VSTRIDE_OKA(x) ((x) == 2) 
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

/* configuration ; KNF 0 0 0 1 0 1 */
#define KCVI_VBYI_SINGLE_USE_MUL 0
#define KCVI_VBYI_DOUBLE_USE_MUL 0
#define KCVI_LD_DOUBLE_USE_UNPACK 1
#define KCVI_ST_DOUBLE_USE_PACK 1
#define KCVI_ST2_DOUBLE_USE_STN2 0
#define KCVI_MULZ_USE_SWIZZLE 1

#include <immintrin.h>

typedef DS(__m512d, __m512) V;

#define VADD(a,b) SUFF(_mm512_add)(a,b)
#define VSUB(a,b) SUFF(_mm512_sub)(a,b)
#define VMUL(a,b) SUFF(_mm512_mul)(a,b)

#define VFMA(a, b, c) SUFF(_mm512_fmadd)(a, b, c) //VADD(c, VMUL(a, b))
#define VFMS(a, b, c) SUFF(_mm512_fmsub)(a, b, c) //VSUB(VMUL(a, b), c)
#define VFNMS(a, b, c) SUFF(_mm512_fnmadd)(a, b, c) //VSUB(c, VMUL(a, b))

#define LDK(x) x
#define VLIT(re, im) SUFF(_mm512_setr4)(im, re, im, re)
#define DVK(var, val) V var = SUFF(_mm512_set1)(val)

static inline V LDA(const R *x, INT ivs, const R *aligned_like) {
  return SUFF(_mm512_load)(x);
}
static inline void STA(R *x, V v, INT ovs, const R *aligned_like) {
  SUFF(_mm512_store)(x, v);
}

#if FFTW_SINGLE
#define VXOR(a,b) _mm512_xor_epi32(a,b)

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
  
  return _mm512_i32gather_ps(index, x, _MM_SCALE_4);
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
  
  _mm512_i32scatter_ps(x, index, v, _MM_SCALE_4);
}

static inline V FLIP_RI(V x)
{
  return (V)_mm512_shuffle_epi32((__m512i)x, _MM_PERM_CDAB);
}

#define VDUPH(a) (V)_mm512_shuffle_epi32((__m512i)a, _MM_PERM_DDBB);
#define VDUPL(a) (V)_mm512_shuffle_epi32((__m512i)a, _MM_PERM_CCAA);

#else /* !FFTW_SINGLE */
#define VXOR(a,b) _mm512_xor_epi64(a,b)

#if defined (KCVI_LD_DOUBLE_USE_UNPACK) && KCVI_LD_DOUBLE_USE_UNPACK
static inline V LDu(const R *x, INT ivs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  V temp;
  /* no need for hq here */
  temp = _mm512_mask_loadunpacklo_pd(temp, 0x0003, x + (0 * ivs));
  temp = _mm512_mask_loadunpacklo_pd(temp, 0x000c, x + (1 * ivs));
  temp = _mm512_mask_loadunpacklo_pd(temp, 0x0030, x + (2 * ivs));
  temp = _mm512_mask_loadunpacklo_pd(temp, 0x00c0, x + (3 * ivs));
  return temp;
}
#else
static inline V LDu(const R *x, INT ivs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __declspec(align(64)) R temp[8]; 
  int i;
  for (i = 0 ; i < 4 ; i++) {
    temp[i*2]   = x[i * ivs];
    temp[i*2+1] = x[i * ivs + 1];
  }
  return _mm512_load_pd(temp);
}
#endif

#if defined(KCVI_ST_DOUBLE_USE_PACK) && KCVI_ST_DOUBLE_USE_PACK
static inline void STu(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  /* no need for hq here */
  _mm512_mask_packstorelo_pd(x + (0 * ovs), 0x0003, v);
  _mm512_mask_packstorelo_pd(x + (1 * ovs), 0x000c, v);
  _mm512_mask_packstorelo_pd(x + (2 * ovs), 0x0030, v);
  _mm512_mask_packstorelo_pd(x + (3 * ovs), 0x00c0, v);
}
#else
static inline void STu(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __declspec(align(64)) R temp[8];
  int i;
  _mm512_store_pd(temp, v);
  for (i = 0 ; i < 4 ; i++) {
    x[i * ovs] = temp[i*2];
    x[i * ovs + 1] = temp[i*2+1];
  } 
}
#endif

static inline V FLIP_RI(V x)
{
  return (V)_mm512_shuffle_epi32((__m512i)x, _MM_PERM_BADC);
}

#define VDUPH(a) (V)_mm512_shuffle_epi32((__m512i)a, _MM_PERM_DCDC);
#define VDUPL(a) (V)_mm512_shuffle_epi32((__m512i)a, _MM_PERM_BABA);

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
  
  _mm512_i32scatter_ps(x, index, v, _MM_SCALE_4);
}
#define STN4(x, v0, v1, v2, v3, ovs)  /* no-op */
#else /* !FFTW_SINGLE */
#if defined(KCVI_ST2_DOUBLE_USE_STN2) && KCVI_ST2_DOUBLE_USE_STN2
#define STM2(x, v, ovs, a) /* no-op */
static inline void STN2(R *x, V v0, V v1, INT ovs) {
  /*  we start
     AB CD EF GH -> *x (2 DBL), ovs between complex
     IJ KL MN OP -> *(x+2) (2DBL), ovs between complex
      and we want
     ABIJ  EFMN -> *x (4 DBL), 2 * ovs between complex pairs
      CDKL  GHOP -> *(x+ovs) (4DBL), 2 * ovs between complex pairs
  */
  V x00 = (V)_mm512_mask_permute4f128_epi32((__m512i)v0, 0xF0F0, (__m512i)v1, _MM_PERM_CDAB);
  V x01 = (V)_mm512_mask_permute4f128_epi32((__m512i)v1, 0x0F0F, (__m512i)v0, _MM_PERM_CDAB);
  _mm512_mask_packstorelo_pd(x + (0 * ovs) + 0, 0x000F, x00);
/*   _mm512_mask_packstorehi_pd(x + (0 * ovs) + 8, 0x000F, x00); */
  _mm512_mask_packstorelo_pd(x + (2 * ovs) + 0, 0x00F0, x00);
/*   _mm512_mask_packstorehi_pd(x + (2 * ovs) + 8, 0x00F0, x00); */
  _mm512_mask_packstorelo_pd(x + (1 * ovs) + 0, 0x000F, x01);
/*   _mm512_mask_packstorehi_pd(x + (1 * ovs) + 8, 0x000F, x01); */
  _mm512_mask_packstorelo_pd(x + (3 * ovs) + 0, 0x00F0, x01);
/*   _mm512_mask_packstorehi_pd(x + (3 * ovs) + 8, 0x00F0, x01); */
}
#else
#define STM2(x, v, ovs, a) ST(x, v, ovs, a)
#define STN2(x, v0, v1, ovs) /* nop */
#endif

static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
  (void)aligned_like; /* UNUSED */
  __m512i index = _mm512_set_epi32(0, 0, 0, 0, 0, 0, 0, 0,
                                   7 * ovs, 6 * ovs,
                                   5 * ovs, 4 * ovs,
                                   3 * ovs, 2 * ovs,
                                   1 * ovs, 0 * ovs);
  
  _mm512_i32loscatter_pd(x, index, v, _MM_SCALE_8);
}
#define STN4(x, v0, v1, v2, v3, ovs)  /* no-op */
#endif /* FFTW_SINGLE */

static inline V VFMAI(V b, V c) {
  V mpmp = VLIT(SCAL(1.0), SCAL(-1.0));
  return SUFF(_mm512_fmadd)(mpmp, SUFF(_mm512_swizzle)(b, _MM_SWIZ_REG_CDAB), c);
}

static inline V VFNMSI(V b, V c) {
  V mpmp = VLIT(SCAL(1.0), SCAL(-1.0));
  return SUFF(_mm512_fnmadd)(mpmp, SUFF(_mm512_swizzle)(b, _MM_SWIZ_REG_CDAB), c);
}

static inline V VFMACONJ(V b, V c) {
  V pmpm = VLIT(SCAL(-1.0), SCAL(1.0));
  return SUFF(_mm512_fmadd)(pmpm, b, c);
}

static inline V VFMSCONJ(V b, V c) {
  V pmpm = VLIT(SCAL(-1.0), SCAL(1.0));
  return SUFF(_mm512_fmsub)(pmpm, b, c);
}

static inline V VFNMSCONJ(V b, V c) {
  V pmpm = VLIT(SCAL(-1.0), SCAL(1.0));
  return SUFF(_mm512_fnmadd)(pmpm, b, c);
}

static inline V VCONJ(V x)
{
     V pmpm = VLIT(SCAL(-0.0), SCAL(0.0));
     return (V)VXOR((__m512i)pmpm, (__m512i)x);
}

#ifdef FFTW_SINGLE
#if defined(KCVI_VBYI_SINGLE_USE_MUL) && KCVI_VBYI_SINGLE_USE_MUL
/* untested */
static inline V VBYI(V x)
{
  V mpmp = VLIT(SCAL(1.0), SCAL(-1.0));
  return _mm512_mul_ps(mpmp, _mm512_swizzle_ps(x, _MM_SWIZ_REG_CDAB));
}
#else
static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}
#endif
#else /* !FFTW_SINGLE */
#if defined(KCVI_VBYI_DOUBLE_USE_MUL) && KCVI_VBYI_DOUBLE_USE_MUL
/* on KNF, using mul_pd is slower than shuf128x32 + xor */
static inline V VBYI(V x)
{
  V mpmp = VLIT(SCAL(1.0), SCAL(-1.0));
  return _mm512_mul_pd(mpmp, _mm512_swizzle_pd(x, _MM_SWIZ_REG_CDAB));
}
#else
static inline V VBYI(V x)
{
     return FLIP_RI(VCONJ(x));
}
#endif
#endif /* FFTW_SINGLE */

#if defined(KCVI_MULZ_USE_SWIZZLE) && KCVI_MULZ_USE_SWIZZLE
static inline V VZMUL(V tx, V sr) /* (a,b) (c,d) */
{
  V ac = SUFF(_mm512_mul)(tx, sr); /* (a*c,b*d) */
  V ad = SUFF(_mm512_mul)(tx, SUFF(_mm512_swizzle)(sr, _MM_SWIZ_REG_CDAB)); /* (a*d,b*c) */
  V acmbd = SUFF(_mm512_sub)(ac, SUFF(_mm512_swizzle)(ac, _MM_SWIZ_REG_CDAB)); /* (a*c-b*d, b*d-a*c) */
  V res = SUFF(_mm512_mask_add)(acmbd, DS(0x00aa,0xaaaa), ad, SUFF(_mm512_swizzle)(ad, _MM_SWIZ_REG_CDAB)); /* ([a*c+b*c] a*c-b*d, b*c+a*d) */
  return res;
}
static inline V VZMULJ(V tx, V sr) /* (a,b) (c,d) */
{
  V ac = SUFF(_mm512_mul)(tx, sr); /* (a*c,b*d) */
  V ad = SUFF(_mm512_mul)(tx, SUFF(_mm512_swizzle)(sr, _MM_SWIZ_REG_CDAB)); /* (a*d,b*c) */
  V acmbd = SUFF(_mm512_add)(ac, SUFF(_mm512_swizzle)(ac, _MM_SWIZ_REG_CDAB)); /* (a*c+b*d, b*d+a*c) */
  V res = SUFF(_mm512_mask_subr)(acmbd, DS(0x00aa,0xaaaa), ad, SUFF(_mm512_swizzle)(ad, _MM_SWIZ_REG_CDAB)); /* ([a*c+b*c] a*c+b*d, a*d-b*c) */
  return res;
}
static inline V VZMULI(V tx, V sr) /* (a,b) (c,d) */
{
  DVK(zero, SCAL(0.0));
  V ac = SUFF(_mm512_mul)(tx, sr); /* (a*c,b*d) */
  V ad = SUFF(_mm512_fnmadd)(tx, SUFF(_mm512_swizzle)(sr, _MM_SWIZ_REG_CDAB), zero); /* (-a*d,-b*c) */
  V acmbd = SUFF(_mm512_subr)(ac, SUFF(_mm512_swizzle)(ac, _MM_SWIZ_REG_CDAB)); /* (b*d-a*c, a*c-b*d) */
  V res = SUFF(_mm512_mask_add)(acmbd, DS(0x0055,0x5555), ad, SUFF(_mm512_swizzle)(ad, _MM_SWIZ_REG_CDAB)); /*  (-a*d-b*c, a*c-b*d) */
  return res;
}
static inline V VZMULIJ(V tx, V sr) /* (a,b) (c,d) */
{
  DVK(zero, SCAL(0.0));
  V ac = SUFF(_mm512_mul)(tx, sr); /* (a*c,b*d) */
  V ad = SUFF(_mm512_fnmadd)(tx, SUFF(_mm512_swizzle)(sr, _MM_SWIZ_REG_CDAB), zero); /* (-a*d,-b*c) */
  V acmbd = SUFF(_mm512_add)(ac, SUFF(_mm512_swizzle)(ac, _MM_SWIZ_REG_CDAB)); /* (b*d+a*c, a*c+b*d) */
  V res = SUFF(_mm512_mask_sub)(acmbd, DS(0x0055,0x5555), ad, SUFF(_mm512_swizzle)(ad, _MM_SWIZ_REG_CDAB)); /*  (-a*d+b*c, a*c-b*d) */
  return res;
}
#else
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
#endif

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

#define VLEAVE() /* nothing */

#include "simd-common.h"
