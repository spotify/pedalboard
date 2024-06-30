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

/* out of place copy routines for pairs of isomorphic 2D arrays */
#include "kernel/ifftw.h"

void X(cpy2d_pair)(R *I0, R *I1, R *O0, R *O1,
		   INT n0, INT is0, INT os0,
		   INT n1, INT is1, INT os1)
{
     INT i0, i1;

     for (i1 = 0; i1 < n1; ++i1)
	  for (i0 = 0; i0 < n0; ++i0) {
	       R x0 = I0[i0 * is0 + i1 * is1];
	       R x1 = I1[i0 * is0 + i1 * is1];
	       O0[i0 * os0 + i1 * os1] = x0;
	       O1[i0 * os0 + i1 * os1] = x1;
	  }
}

void X(zero1d_pair)(R *O0, R *O1, INT n0, INT os0)
{
     INT i0;
     for (i0 = 0; i0 < n0; ++i0) {
          O0[i0 * os0] = 0;
          O1[i0 * os0] = 0;
     }
}

/* like cpy2d_pair, but read input contiguously if possible */
void X(cpy2d_pair_ci)(R *I0, R *I1, R *O0, R *O1,
		      INT n0, INT is0, INT os0,
		      INT n1, INT is1, INT os1)
{
     if (IABS(is0) < IABS(is1))	/* inner loop is for n0 */
	  X(cpy2d_pair) (I0, I1, O0, O1, n0, is0, os0, n1, is1, os1);
     else
	  X(cpy2d_pair) (I0, I1, O0, O1, n1, is1, os1, n0, is0, os0);
}

/* like cpy2d_pair, but write output contiguously if possible */
void X(cpy2d_pair_co)(R *I0, R *I1, R *O0, R *O1,
		      INT n0, INT is0, INT os0,
		      INT n1, INT is1, INT os1)
{
     if (IABS(os0) < IABS(os1))	/* inner loop is for n0 */
	  X(cpy2d_pair) (I0, I1, O0, O1, n0, is0, os0, n1, is1, os1);
     else
	  X(cpy2d_pair) (I0, I1, O0, O1, n1, is1, os1, n0, is0, os0);
}
