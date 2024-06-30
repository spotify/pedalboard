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
#include "dft/dft.h"

#define N0(nembed)((nembed) ? (nembed) : n)

X(plan) X(plan_many_dft)(int rank, const int *n,
			 int howmany,
			 C *in, const int *inembed,
			 int istride, int idist,
			 C *out, const int *onembed,
			 int ostride, int odist, int sign, unsigned flags)
{
     R *ri, *ii, *ro, *io;

     if (!X(many_kosherp)(rank, n, howmany)) return 0;

     EXTRACT_REIM(sign, in, &ri, &ii);
     EXTRACT_REIM(sign, out, &ro, &io);

     return 
	  X(mkapiplan)(sign, flags,
		       X(mkproblem_dft_d)(
			    X(mktensor_rowmajor)(rank, n, 
						 N0(inembed), N0(onembed),
						 2 * istride, 2 * ostride),
			    X(mktensor_1d)(howmany, 2 * idist, 2 * odist),
			    TAINT_UNALIGNED(ri, flags),
			    TAINT_UNALIGNED(ii, flags),
			    TAINT_UNALIGNED(ro, flags),
			    TAINT_UNALIGNED(io, flags)));
}
