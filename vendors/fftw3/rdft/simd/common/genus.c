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

#include "rdft/codelet-rdft.h"
#include SIMD_HEADER

#define EXTERN_CONST(t, x) extern const t x; const t x

static int hc2cbv_okp(const R *Rp, const R *Ip, const R *Rm, const R *Im, 
		      INT rs, INT mb, INT me, INT ms, 
		      const planner *plnr)
{
     return (1
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(rs)
	     && SIMD_VSTRIDE_OK(ms)
             && ((me - mb) % VL) == 0
             && ((mb - 1) % VL) == 0 /* twiddle factors alignment */
	     && ALIGNED(Rp)
	     && ALIGNED(Rm)
	     && Ip == Rp + 1
	     && Im == Rm + 1);
}

EXTERN_CONST(hc2c_genus, XSIMD(rdft_hc2cbv_genus)) = { hc2cbv_okp, HC2R, VL };

static int hc2cfv_okp(const R *Rp, const R *Ip, const R *Rm, const R *Im, 
		      INT rs, INT mb, INT me, INT ms, 
		      const planner *plnr)
{
     return (1
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(rs)
	     && SIMD_VSTRIDE_OK(ms)
             && ((me - mb) % VL) == 0
             && ((mb - 1) % VL) == 0 /* twiddle factors alignment */
	     && ALIGNED(Rp)
	     && ALIGNED(Rm)
	     && Ip == Rp + 1
	     && Im == Rm + 1);
}

EXTERN_CONST(hc2c_genus, XSIMD(rdft_hc2cfv_genus)) = { hc2cfv_okp, R2HC, VL };
