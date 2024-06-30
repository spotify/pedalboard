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

#include "dft/codelet-dft.h"
#include SIMD_HEADER

#define EXTERN_CONST(t, x) extern const t x; const t x

static int n1b_okp(const kdft_desc *d,
		   const R *ri, const R *ii, const R *ro, const R *io,
		   INT is, INT os, INT vl, INT ivs, INT ovs, 
		   const planner *plnr)
{
     return (1
             && ALIGNED(ii)
             && ALIGNED(io)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(is)
	     && SIMD_STRIDE_OK(os)
	     && SIMD_VSTRIDE_OK(ivs)
	     && SIMD_VSTRIDE_OK(ovs)
             && ri == ii + 1
             && ro == io + 1
             && (vl % VL) == 0
             && (!d->is || (d->is == is))
             && (!d->os || (d->os == os))
             && (!d->ivs || (d->ivs == ivs))
             && (!d->ovs || (d->ovs == ovs))
          );
}

EXTERN_CONST(kdft_genus, XSIMD(dft_n1bsimd_genus)) = { n1b_okp, VL };

static int n1f_okp(const kdft_desc *d,
		   const R *ri, const R *ii, const R *ro, const R *io,
		   INT is, INT os, INT vl, INT ivs, INT ovs, 
		   const planner *plnr)
{
     return (1
             && ALIGNED(ri)
             && ALIGNED(ro)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(is)
	     && SIMD_STRIDE_OK(os)
	     && SIMD_VSTRIDE_OK(ivs)
	     && SIMD_VSTRIDE_OK(ovs)
             && ii == ri + 1
             && io == ro + 1
             && (vl % VL) == 0
             && (!d->is || (d->is == is))
             && (!d->os || (d->os == os))
             && (!d->ivs || (d->ivs == ivs))
             && (!d->ovs || (d->ovs == ovs))
          );
}

EXTERN_CONST(kdft_genus, XSIMD(dft_n1fsimd_genus)) = { n1f_okp, VL };

static int n2b_okp(const kdft_desc *d,
		   const R *ri, const R *ii, const R *ro, const R *io,
		   INT is, INT os, INT vl, INT ivs, INT ovs, 
		   const planner *plnr)
{
     return (1
             && ALIGNEDA(ii)
             && ALIGNEDA(io)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OKA(is)
	     && SIMD_VSTRIDE_OKA(ivs)
	     && SIMD_VSTRIDE_OKA(os) /* os == 2 enforced by codelet */
	     && SIMD_STRIDE_OKPAIR(ovs)
             && ri == ii + 1
             && ro == io + 1
             && (vl % VL) == 0
             && (!d->is || (d->is == is))
             && (!d->os || (d->os == os))
             && (!d->ivs || (d->ivs == ivs))
             && (!d->ovs || (d->ovs == ovs))
          );
}

EXTERN_CONST(kdft_genus, XSIMD(dft_n2bsimd_genus)) = { n2b_okp, VL };

static int n2f_okp(const kdft_desc *d,
		   const R *ri, const R *ii, const R *ro, const R *io,
		   INT is, INT os, INT vl, INT ivs, INT ovs, 
		   const planner *plnr)
{
     return (1
             && ALIGNEDA(ri)
             && ALIGNEDA(ro)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OKA(is)
	     && SIMD_VSTRIDE_OKA(ivs)
	     && SIMD_VSTRIDE_OKA(os) /* os == 2 enforced by codelet */
	     && SIMD_STRIDE_OKPAIR(ovs)
             && ii == ri + 1
             && io == ro + 1
             && (vl % VL) == 0
             && (!d->is || (d->is == is))
             && (!d->os || (d->os == os))
             && (!d->ivs || (d->ivs == ivs))
             && (!d->ovs || (d->ovs == ovs))
          );
}

EXTERN_CONST(kdft_genus, XSIMD(dft_n2fsimd_genus)) = { n2f_okp, VL };

static int n2s_okp(const kdft_desc *d,
		   const R *ri, const R *ii, const R *ro, const R *io,
		   INT is, INT os, INT vl, INT ivs, INT ovs, 
		   const planner *plnr)
{
     return (1
	     && !NO_SIMDP(plnr)
	     && ALIGNEDA(ri)
	     && ALIGNEDA(ii)
	     && ALIGNEDA(ro)
	     && ALIGNEDA(io)
	     && SIMD_STRIDE_OKA(is)
	     && ivs == 1
	     && os == 1
	     && SIMD_STRIDE_OKA(ovs)
	     && (vl % (2 * VL)) == 0
	     && (!d->is || (d->is == is))
	     && (!d->os || (d->os == os))
	     && (!d->ivs || (d->ivs == ivs))
	     && (!d->ovs || (d->ovs == ovs))
	  );
}

EXTERN_CONST(kdft_genus, XSIMD(dft_n2ssimd_genus)) = { n2s_okp, 2 * VL };

static int q1b_okp(const ct_desc *d,
		   const R *rio, const R *iio, 
		   INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		   const planner *plnr)
{
     return (1
	     && ALIGNED(iio)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(rs)
	     && SIMD_STRIDE_OK(vs)
	     && SIMD_VSTRIDE_OK(ms)
	     && rio == iio + 1
	     && (m % VL) == 0
	     && (mb % VL) == 0
	     && (me % VL) == 0
	     && (!d->rs || (d->rs == rs))
	     && (!d->vs || (d->vs == vs))
	     && (!d->ms || (d->ms == ms))
	  );
}
EXTERN_CONST(ct_genus,  XSIMD(dft_q1bsimd_genus)) = { q1b_okp, VL };

static int q1f_okp(const ct_desc *d,
		   const R *rio, const R *iio, 
		   INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		   const planner *plnr)
{
     return (1
	     && ALIGNED(rio)
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(rs)
	     && SIMD_STRIDE_OK(vs)
	     && SIMD_VSTRIDE_OK(ms)
	     && iio == rio + 1
	     && (m % VL) == 0
	     && (mb % VL) == 0
	     && (me % VL) == 0
	     && (!d->rs || (d->rs == rs))
	     && (!d->vs || (d->vs == vs))
	     && (!d->ms || (d->ms == ms))
	  );
}
EXTERN_CONST(ct_genus,  XSIMD(dft_q1fsimd_genus)) = { q1f_okp, VL };

static int t_okp_common(const ct_desc *d,
			const R *rio, const R *iio, 
			INT rs, INT vs, INT m, INT mb, INT me, INT ms,
			const planner *plnr)
{
     UNUSED(rio); UNUSED(iio);
     return (1
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OKA(rs)
	     && SIMD_VSTRIDE_OKA(ms)
	     && (m % VL) == 0
	     && (mb % VL) == 0
	     && (me % VL) == 0
	     && (!d->rs || (d->rs == rs))
	     && (!d->vs || (d->vs == vs))
	     && (!d->ms || (d->ms == ms))
	  );
}

static int t_okp_commonu(const ct_desc *d,
			 const R *rio, const R *iio, 
			 INT rs, INT vs, INT m, INT mb, INT me, INT ms,
			 const planner *plnr)
{
     UNUSED(rio); UNUSED(iio); UNUSED(m);
     return (1
	     && !NO_SIMDP(plnr)
	     && SIMD_STRIDE_OK(rs)
	     && SIMD_VSTRIDE_OK(ms)
	     && (mb % VL) == 0
	     && (me % VL) == 0
	     && (!d->rs || (d->rs == rs))
	     && (!d->vs || (d->vs == vs))
	     && (!d->ms || (d->ms == ms))
	  );
}

static int t_okp_t1f(const ct_desc *d,
		     const R *rio, const R *iio, 
		     INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		     const planner *plnr)
{
     return  t_okp_common(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && iio == rio + 1
	  && ALIGNEDA(rio);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t1fsimd_genus)) = { t_okp_t1f, VL };

static int t_okp_t1fu(const ct_desc *d,
		      const R *rio, const R *iio, 
		      INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		      const planner *plnr)
{
     return  t_okp_commonu(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && iio == rio + 1
	  && ALIGNED(rio);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t1fusimd_genus)) = { t_okp_t1fu, VL };

static int t_okp_t1b(const ct_desc *d,
		     const R *rio, const R *iio, 
		     INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		     const planner *plnr)
{
     return  t_okp_common(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && rio == iio + 1
	  && ALIGNEDA(iio);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t1bsimd_genus)) = { t_okp_t1b, VL };

static int t_okp_t1bu(const ct_desc *d,
		      const R *rio, const R *iio,
		      INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		      const planner *plnr)
{									
     return  t_okp_commonu(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && rio == iio + 1
	  && ALIGNED(iio);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t1busimd_genus)) = { t_okp_t1bu, VL };

/* use t2* codelets only when n = m*radix is small, because
   t2* codelets use ~2n twiddle factors (instead of ~n) */
static int small_enough(const ct_desc *d, INT m)
{
     return m * d->radix <= 16384;
}

static int t_okp_t2f(const ct_desc *d,
		     const R *rio, const R *iio, 
		     INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		     const planner *plnr)
{
     return  t_okp_t1f(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && small_enough(d, m);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t2fsimd_genus)) = { t_okp_t2f, VL };

static int t_okp_t2b(const ct_desc *d,
		     const R *rio, const R *iio, 
		     INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		     const planner *plnr)
{
     return  t_okp_t1b(d, rio, iio, rs, vs, m, mb, me, ms, plnr)
	  && small_enough(d, m);
}

EXTERN_CONST(ct_genus,  XSIMD(dft_t2bsimd_genus)) = { t_okp_t2b, VL };

static int ts_okp(const ct_desc *d,
		  const R *rio, const R *iio, 
		  INT rs, INT vs, INT m, INT mb, INT me, INT ms,
		  const planner *plnr)
{
     UNUSED(rio);
     UNUSED(iio);
     return (1
	     && !NO_SIMDP(plnr)
	     && ALIGNEDA(rio)
	     && ALIGNEDA(iio)
	     && SIMD_STRIDE_OKA(rs)
	     && ms == 1
	     && (m % (2 * VL)) == 0
	     && (mb % (2 * VL)) == 0
	     && (me % (2 * VL)) == 0
	     && (!d->rs || (d->rs == rs))
	     && (!d->vs || (d->vs == vs))
	     && (!d->ms || (d->ms == ms))
	  );
}

EXTERN_CONST(ct_genus,  XSIMD(dft_tssimd_genus)) = { ts_okp, 2 * VL };
