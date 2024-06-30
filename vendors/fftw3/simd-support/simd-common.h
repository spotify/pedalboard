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

/* detection of alignment.  This is complicated because a machine may
   support multiple SIMD extensions (e.g. SSE2 and AVX) but only one
   set of alignment contraints.  So this alignment stuff cannot be
   defined in the SIMD header files.  Rather than defining a separate
   set of "machine" header files, we just do this ugly ifdef here. */
#if defined(HAVE_SSE2) || defined(HAVE_AVX) || defined(HAVE_AVX2) || defined(HAVE_AVX_128_FMA) || defined(HAVE_AVX512)
#  if defined(FFTW_SINGLE)
#    define ALIGNMENT 8     /* Alignment for the LD/ST macros */
#    define ALIGNMENTA 16   /* Alignment for the LDA/STA macros */
#  else
#    define ALIGNMENT 16    /* Alignment for the LD/ST macros */
#    define ALIGNMENTA 16   /* Alignment for the LDA/STA macros */
#  endif
#elif defined(HAVE_ALTIVEC)
#  define ALIGNMENT 8     /* Alignment for the LD/ST macros */
#  define ALIGNMENTA 16   /* Alignment for the LDA/STA macros */
#elif defined(HAVE_NEON) || defined(HAVE_VSX)
#  define ALIGNMENT 8     /* Alignment for the LD/ST macros */
#  define ALIGNMENTA 8    /* Alignment for the LDA/STA macros */
#elif defined(HAVE_KCVI)
#  if defined(FFTW_SINGLE)
#    define ALIGNMENT 8     /* Alignment for the LD/ST macros */
#  else
#    define ALIGNMENT 16     /* Alignment for the LD/ST macros */
#  endif
#  define ALIGNMENTA 64   /* Alignment for the LDA/STA macros */
#elif defined(HAVE_GENERIC_SIMD256)
#  if defined(FFTW_SINGLE)
#    define ALIGNMENT 8
#    define ALIGNMENTA 32
#  else
#    define ALIGNMENT 16
#    define ALIGNMENTA 32
#  endif
#elif defined(HAVE_GENERIC_SIMD128)
#  if defined(FFTW_SINGLE)
#    define ALIGNMENT 8
#    define ALIGNMENTA 16
#  else
#    define ALIGNMENT 16
#    define ALIGNMENTA 16
#  endif
#endif

#if HAVE_SIMD
#  ifndef ALIGNMENT
#  error "ALIGNMENT not defined"
#  endif
#  ifndef ALIGNMENTA
#  error "ALIGNMENTA not defined"
#  endif
#endif

/* rename for precision and for SIMD extensions */
#define XSIMD0(name, suffix) CONCAT(name, suffix)
#define XSIMD(name) XSIMD0(X(name), SIMD_SUFFIX)
#define XSIMD_STRING(x) x STRINGIZE(SIMD_SUFFIX)

/* TAINT_BIT is set if pointers are not guaranteed to be multiples of
   ALIGNMENT */
#define TAINT_BIT 1    

/* TAINT_BITA is set if pointers are not guaranteed to be multiples of
   ALIGNMENTA */
#define TAINT_BITA 2

#define PTRINT(p) ((uintptr_t)(p))

#define ALIGNED(p) \
  (((PTRINT(UNTAINT(p)) % ALIGNMENT) == 0) && !(PTRINT(p) & TAINT_BIT))

#define ALIGNEDA(p) \
  (((PTRINT(UNTAINT(p)) % ALIGNMENTA) == 0) && !(PTRINT(p) & TAINT_BITA))

#define SIMD_STRIDE_OK(x) (!(((x) * sizeof(R)) % ALIGNMENT))
#define SIMD_STRIDE_OKA(x) (!(((x) * sizeof(R)) % ALIGNMENTA))
#define SIMD_VSTRIDE_OK SIMD_STRIDE_OK

