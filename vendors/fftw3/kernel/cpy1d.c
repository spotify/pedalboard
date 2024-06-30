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

/* out of place 1D copy routine */
#include "kernel/ifftw.h"

void X(cpy1d)(R *I, R *O, INT n0, INT is0, INT os0, INT vl)
{
     INT i0, v;

     A(I != O);
     switch (vl) {
	 case 1:
	      if ((n0 & 1) || is0 != 1 || os0 != 1) {
		   for (; n0 > 0; --n0, I += is0, O += os0)
			*O = *I;
		   break;
	      }
	      n0 /= 2; is0 = 2; os0 = 2;
	      /* fall through */
	 case 2:
	      if ((n0 & 1) || is0 != 2 || os0 != 2) {
		   for (; n0 > 0; --n0, I += is0, O += os0) {
			R x0 = I[0];
			R x1 = I[1];
			O[0] = x0;
			O[1] = x1;
		   }
		   break;
	      }
	      n0 /= 2; is0 = 4; os0 = 4;
	      /* fall through */
	 case 4:
	      for (; n0 > 0; --n0, I += is0, O += os0) {
		   R x0 = I[0];
		   R x1 = I[1];
		   R x2 = I[2];
		   R x3 = I[3];
		   O[0] = x0;
		   O[1] = x1;
		   O[2] = x2;
		   O[3] = x3;
	      }
	      break;
	 default:
	      for (i0 = 0; i0 < n0; ++i0)
		   for (v = 0; v < vl; ++v) {
			R x0 = I[i0 * is0 + v];
			O[i0 * os0 + v] = x0;
		   }
	      break;
     }
}
