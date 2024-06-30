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

/* like X(tensor_max_index), but takes into account the special n/2+1
   final dimension for the complex output/input of an R2HC/HC2R transform. */
INT X(rdft2_tensor_max_index)(const tensor *sz, rdft_kind k)
{
     int i;
     INT n = 0;

     A(FINITE_RNK(sz->rnk));
     for (i = 0; i + 1 < sz->rnk; ++i) {
          const iodim *p = sz->dims + i;
          n += (p->n - 1) * X(imax)(X(iabs)(p->is), X(iabs)(p->os));
     }
     if (i < sz->rnk) {
	  const iodim *p = sz->dims + i;
	  INT is, os;
	  X(rdft2_strides)(k, p, &is, &os);
	  n += X(imax)((p->n - 1) * X(iabs)(is), (p->n/2) * X(iabs)(os));
     }
     return n;
}
