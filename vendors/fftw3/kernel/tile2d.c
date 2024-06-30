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

/* out of place 2D copy routines */
#include "kernel/ifftw.h"

void X(tile2d)(INT n0l, INT n0u, INT n1l, INT n1u, INT tilesz,
	       void (*f)(INT n0l, INT n0u, INT n1l, INT n1u, void *args),
	       void *args)
{
     INT d0, d1;

     A(tilesz > 0); /* infinite loops otherwise */
     
 tail:
     d0 = n0u - n0l;
     d1 = n1u - n1l;

     if (d0 >= d1 && d0 > tilesz) {
	  INT n0m = (n0u + n0l) / 2;
	  X(tile2d)(n0l, n0m, n1l, n1u, tilesz, f, args);
	  n0l = n0m; goto tail;
     } else if (/* d1 >= d0 && */ d1 > tilesz) {
	  INT n1m = (n1u + n1l) / 2;
	  X(tile2d)(n0l, n0u, n1l, n1m, tilesz, f, args);
	  n1l = n1m; goto tail;
     } else {
	  f(n0l, n0u, n1l, n1u, args);
     }
}

INT X(compute_tilesz)(INT vl, int how_many_tiles_in_cache)
{
     return X(isqrt)(CACHESIZE / 
		     (((INT)sizeof(R)) * vl * (INT)how_many_tiles_in_cache));
}
