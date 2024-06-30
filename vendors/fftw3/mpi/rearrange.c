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

#include "ifftw-mpi.h"

/* common functions for rearrangements of the data for the *-rank1-bigvec
   solvers */

static int div_mult(INT b, INT a) { 
     return (a > b && a % b == 0);
}
static int div_mult2(INT b, INT a, INT n) { 
     return (div_mult(b, a) && div_mult(n, b));
}

int XM(rearrange_applicable)(rearrangement rearrange, 
			     ddim dim0, INT vn, int n_pes)
{
     /* note: it is important that cases other than CONTIG be
	applicable only when the resulting transpose dimension
	is divisible by n_pes; otherwise, the allocation size
	returned by the API will be incorrect */
     return ((rearrange != DISCONTIG || div_mult(n_pes, vn))
	     && (rearrange != SQUARE_BEFORE 
		 || div_mult2(dim0.b[IB], vn, n_pes))
	     && (rearrange != SQUARE_AFTER
		 || (dim0.b[IB] != dim0.b[OB]
		     && div_mult2(dim0.b[OB], vn, n_pes)))
	     && (rearrange != SQUARE_MIDDLE
		 || div_mult(dim0.n * n_pes, vn)));
}

INT XM(rearrange_ny)(rearrangement rearrange, ddim dim0, INT vn, int n_pes)
{
     switch (rearrange) {
	 case CONTIG:
	      return vn;
	 case DISCONTIG:
	      return n_pes;
	 case SQUARE_BEFORE:
	      return dim0.b[IB];
	 case SQUARE_AFTER:
	      return dim0.b[OB];
	 case SQUARE_MIDDLE:
	      return dim0.n * n_pes;
     }
     return 0;
}
