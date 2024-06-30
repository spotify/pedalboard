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

#include "kernel/ifftw.h"

/* in place square transposition, iterative */
void X(transpose)(R *I, INT n, INT s0, INT s1, INT vl)
{
     INT i0, i1, v;

     switch (vl) {
	 case 1:
	      for (i1 = 1; i1 < n; ++i1) {
		   for (i0 = 0; i0 < i1; ++i0) {
			R x0 = I[i1 * s0 + i0 * s1];
			R y0 = I[i1 * s1 + i0 * s0];
			I[i1 * s1 + i0 * s0] = x0;
			I[i1 * s0 + i0 * s1] = y0;
		   }
	      }
	      break;
	 case 2:
	      for (i1 = 1; i1 < n; ++i1) {
		   for (i0 = 0; i0 < i1; ++i0) {
			R x0 = I[i1 * s0 + i0 * s1];
			R x1 = I[i1 * s0 + i0 * s1 + 1];
			R y0 = I[i1 * s1 + i0 * s0];
			R y1 = I[i1 * s1 + i0 * s0 + 1];
			I[i1 * s1 + i0 * s0] = x0;
			I[i1 * s1 + i0 * s0 + 1] = x1;
			I[i1 * s0 + i0 * s1] = y0;
			I[i1 * s0 + i0 * s1 + 1] = y1;
		   }
	      }
	      break;
	 default:
	      for (i1 = 1; i1 < n; ++i1) {
		   for (i0 = 0; i0 < i1; ++i0) {
			for (v = 0; v < vl; ++v) {
			     R x0 = I[i1 * s0 + i0 * s1 + v];
			     R y0 = I[i1 * s1 + i0 * s0 + v];
			     I[i1 * s1 + i0 * s0 + v] = x0;
			     I[i1 * s0 + i0 * s1 + v] = y0;
			}
		   }
	      }
	      break;
     }
}

struct transpose_closure {
     R *I;
     INT s0, s1, vl, tilesz;
     R *buf0, *buf1; 
};

static void dotile(INT n0l, INT n0u, INT n1l, INT n1u, void *args)
{
     struct transpose_closure *k = (struct transpose_closure *)args;
     R *I = k->I;
     INT s0 = k->s0, s1 = k->s1, vl = k->vl;
     INT i0, i1, v;

     switch (vl) {
	 case 1:
	      for (i1 = n1l; i1 < n1u; ++i1) {
		   for (i0 = n0l; i0 < n0u; ++i0) {
			R x0 = I[i1 * s0 + i0 * s1];
			R y0 = I[i1 * s1 + i0 * s0];
			I[i1 * s1 + i0 * s0] = x0;
			I[i1 * s0 + i0 * s1] = y0;
		   }
	      }
	      break;
	 case 2:
	      for (i1 = n1l; i1 < n1u; ++i1) {
		   for (i0 = n0l; i0 < n0u; ++i0) {
			R x0 = I[i1 * s0 + i0 * s1];
			R x1 = I[i1 * s0 + i0 * s1 + 1];
			R y0 = I[i1 * s1 + i0 * s0];
			R y1 = I[i1 * s1 + i0 * s0 + 1];
			I[i1 * s1 + i0 * s0] = x0;
			I[i1 * s1 + i0 * s0 + 1] = x1;
			I[i1 * s0 + i0 * s1] = y0;
			I[i1 * s0 + i0 * s1 + 1] = y1;
		   }
	      }
	      break;
	 default:
	      for (i1 = n1l; i1 < n1u; ++i1) {
		   for (i0 = n0l; i0 < n0u; ++i0) {
			for (v = 0; v < vl; ++v) {
			     R x0 = I[i1 * s0 + i0 * s1 + v];
			     R y0 = I[i1 * s1 + i0 * s0 + v];
			     I[i1 * s1 + i0 * s0 + v] = x0;
			     I[i1 * s0 + i0 * s1 + v] = y0;
			}
		   }
	      }
     }
}

static void dotile_buf(INT n0l, INT n0u, INT n1l, INT n1u, void *args)
{
     struct transpose_closure *k = (struct transpose_closure *)args;
     X(cpy2d_ci)(k->I + n0l * k->s0 + n1l * k->s1,
		 k->buf0,
		 n0u - n0l, k->s0, k->vl,
		 n1u - n1l, k->s1, k->vl * (n0u - n0l),
		 k->vl);
     X(cpy2d_ci)(k->I + n0l * k->s1 + n1l * k->s0,
		 k->buf1,
		 n0u - n0l, k->s1, k->vl,
		 n1u - n1l, k->s0, k->vl * (n0u - n0l),
		 k->vl);
     X(cpy2d_co)(k->buf1,
		 k->I + n0l * k->s0 + n1l * k->s1,
		 n0u - n0l, k->vl, k->s0,
		 n1u - n1l, k->vl * (n0u - n0l), k->s1,
		 k->vl);
     X(cpy2d_co)(k->buf0,
		 k->I + n0l * k->s1 + n1l * k->s0,
		 n0u - n0l, k->vl, k->s1,
		 n1u - n1l, k->vl * (n0u - n0l), k->s0,
		 k->vl);
}

static void transpose_rec(R *I, INT n,
			  void (*f)(INT n0l, INT n0u, INT n1l, INT n1u,
				    void *args),
			  struct transpose_closure *k)
{
   tail:
     if (n > 1) {
	  INT n2 = n / 2;
	  k->I = I;
	  X(tile2d)(0, n2, n2, n, k->tilesz, f, k);
	  transpose_rec(I, n2, f, k);
	  I += n2 * (k->s0 + k->s1); n -= n2; goto tail;
     }
}

void X(transpose_tiled)(R *I, INT n, INT s0, INT s1, INT vl) 
{
     struct transpose_closure k;
     k.s0 = s0;
     k.s1 = s1;
     k.vl = vl;
     /* two blocks must be in cache, to be swapped */
     k.tilesz = X(compute_tilesz)(vl, 2);
     k.buf0 = k.buf1 = 0; /* unused */
     transpose_rec(I, n, dotile, &k);
}

void X(transpose_tiledbuf)(R *I, INT n, INT s0, INT s1, INT vl) 
{
     struct transpose_closure k;
     /* Assume that the the rows of I conflict into the same cache
        lines, and therefore we don't need to reserve cache space for
        the input.  If the rows don't conflict, there is no reason
	to use tiledbuf at all.*/
     R buf0[CACHESIZE / (2 * sizeof(R))];
     R buf1[CACHESIZE / (2 * sizeof(R))];
     k.s0 = s0;
     k.s1 = s1;
     k.vl = vl;
     k.tilesz = X(compute_tilesz)(vl, 2);
     k.buf0 = buf0;
     k.buf1 = buf1;
     A(k.tilesz * k.tilesz * vl * sizeof(R) <= sizeof(buf0));
     A(k.tilesz * k.tilesz * vl * sizeof(R) <= sizeof(buf1));
     transpose_rec(I, n, dotile_buf, &k);
}

