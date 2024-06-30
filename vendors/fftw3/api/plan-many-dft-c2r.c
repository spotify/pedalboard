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

#include "api/api.h"
#include "rdft/rdft.h"

X(plan) X(plan_many_dft_c2r)(int rank, const int *n,
			     int howmany,
			     C *in, const int *inembed,
			     int istride, int idist,
			     R *out, const int *onembed,
			     int ostride, int odist, unsigned flags)
{
     R *ri, *ii;
     int *nfi, *nfo;
     int inplace;
     X(plan) p;

     if (!X(many_kosherp)(rank, n, howmany)) return 0;

     EXTRACT_REIM(FFT_SIGN, in, &ri, &ii);
     inplace = out == ri;

     if (!inplace)
	  flags |= FFTW_DESTROY_INPUT;
     p = X(mkapiplan)(
	  0, flags,
	  X(mkproblem_rdft2_d_3pointers)(
	       X(mktensor_rowmajor)(
		    rank, n, 
		    X(rdft2_pad)(rank, n, inembed, inplace, 1, &nfi),
		    X(rdft2_pad)(rank, n, onembed, inplace, 0, &nfo),
		    2 * istride, ostride),
	       X(mktensor_1d)(howmany, 2 * idist, odist),
	       TAINT_UNALIGNED(out, flags),
	       TAINT_UNALIGNED(ri, flags), TAINT_UNALIGNED(ii, flags),
	       HC2R));

     X(ifree0)(nfi);
     X(ifree0)(nfo);
     return p;
}
