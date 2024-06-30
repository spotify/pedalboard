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


#include "rdft/rdft.h"

static const solvtab s =
{
     SOLVTAB(X(rdft_indirect_register)),
     SOLVTAB(X(rdft_rank0_register)),
     SOLVTAB(X(rdft_vrank3_transpose_register)),
     SOLVTAB(X(rdft_vrank_geq1_register)),

     SOLVTAB(X(rdft_nop_register)),
     SOLVTAB(X(rdft_buffered_register)),
     SOLVTAB(X(rdft_generic_register)),
     SOLVTAB(X(rdft_rank_geq2_register)),

     SOLVTAB(X(dft_r2hc_register)),

     SOLVTAB(X(rdft_dht_register)),
     SOLVTAB(X(dht_r2hc_register)),
     SOLVTAB(X(dht_rader_register)),

     SOLVTAB(X(rdft2_vrank_geq1_register)),
     SOLVTAB(X(rdft2_nop_register)),
     SOLVTAB(X(rdft2_rank0_register)),
     SOLVTAB(X(rdft2_buffered_register)),
     SOLVTAB(X(rdft2_rank_geq2_register)),
     SOLVTAB(X(rdft2_rdft_register)),

     SOLVTAB(X(hc2hc_generic_register)),

     SOLVTAB_END
};

void X(rdft_conf_standard)(planner *p)
{
     X(solvtab_exec)(s, p);
     X(solvtab_exec)(X(solvtab_rdft_r2cf), p);
     X(solvtab_exec)(X(solvtab_rdft_r2cb), p);
     X(solvtab_exec)(X(solvtab_rdft_r2r), p);

#if HAVE_SSE2
     if (X(have_simd_sse2)())
	  X(solvtab_exec)(X(solvtab_rdft_sse2), p);
#endif
#if HAVE_AVX
     if (X(have_simd_avx)())
	  X(solvtab_exec)(X(solvtab_rdft_avx), p);
#endif
#if HAVE_AVX_128_FMA
     if (X(have_simd_avx_128_fma)())
          X(solvtab_exec)(X(solvtab_rdft_avx_128_fma), p);
#endif
#if HAVE_AVX2
     if (X(have_simd_avx2)())
         X(solvtab_exec)(X(solvtab_rdft_avx2), p);
     if (X(have_simd_avx2_128)())
         X(solvtab_exec)(X(solvtab_rdft_avx2_128), p);
#endif
#if HAVE_AVX512
     if (X(have_simd_avx512)())
	  X(solvtab_exec)(X(solvtab_rdft_avx512), p);
#endif
#if HAVE_KCVI
     if (X(have_simd_kcvi)())
	  X(solvtab_exec)(X(solvtab_rdft_kcvi), p);
#endif
#if HAVE_ALTIVEC
     if (X(have_simd_altivec)())
	  X(solvtab_exec)(X(solvtab_rdft_altivec), p);
#endif
#if HAVE_VSX
     if (X(have_simd_vsx)())
       X(solvtab_exec)(X(solvtab_rdft_vsx), p);
#endif
#if HAVE_NEON
     if (X(have_simd_neon)())
	  X(solvtab_exec)(X(solvtab_rdft_neon), p);
#endif
#if HAVE_GENERIC_SIMD128
     X(solvtab_exec)(X(solvtab_rdft_generic_simd128), p);
#endif
#if HAVE_GENERIC_SIMD256
     X(solvtab_exec)(X(solvtab_rdft_generic_simd256), p);
#endif
}
