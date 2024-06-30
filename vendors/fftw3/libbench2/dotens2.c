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


#include "verify.h"

static void recur(int rnk, const bench_iodim *dims0, const bench_iodim *dims1,
		  dotens2_closure *k, 
		  int indx0, int ondx0, int indx1, int ondx1)
{
     if (rnk == 0)
          k->apply(k, indx0, ondx0, indx1, ondx1);
     else {
          int i, n = dims0[0].n;
          int is0 = dims0[0].is;
          int os0 = dims0[0].os;
          int is1 = dims1[0].is;
          int os1 = dims1[0].os;

	  BENCH_ASSERT(n == dims1[0].n);

          for (i = 0; i < n; ++i) {
               recur(rnk - 1, dims0 + 1, dims1 + 1, k,
		     indx0, ondx0, indx1, ondx1);
	       indx0 += is0; ondx0 += os0;
	       indx1 += is1; ondx1 += os1;
	  }
     }
}

void bench_dotens2(const bench_tensor *sz0, const bench_tensor *sz1, dotens2_closure *k)
{
     BENCH_ASSERT(sz0->rnk == sz1->rnk);
     if (sz0->rnk == BENCH_RNK_MINFTY)
          return;
     recur(sz0->rnk, sz0->dims, sz1->dims, k, 0, 0, 0, 0);
}
