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

tensor *X(mktensor_rowmajor)(int rnk, const int *n,
			     const int *niphys, const int *nophys,
			     int is, int os)
{
     tensor *x = X(mktensor)(rnk);

     if (FINITE_RNK(rnk) && rnk > 0) {
          int i;

          A(n && niphys && nophys);
          x->dims[rnk - 1].is = is;
          x->dims[rnk - 1].os = os;
          x->dims[rnk - 1].n = n[rnk - 1];
          for (i = rnk - 1; i > 0; --i) {
               x->dims[i - 1].is = x->dims[i].is * niphys[i];
               x->dims[i - 1].os = x->dims[i].os * nophys[i];
               x->dims[i - 1].n = n[i - 1];
          }
     }
     return x;
}

static int rowmajor_kosherp(int rnk, const int *n)
{
     int i;

     if (!FINITE_RNK(rnk)) return 0;
     if (rnk < 0) return 0;

     for (i = 0; i < rnk; ++i)
	  if (n[i] <= 0) return 0;

     return 1;
}

int X(many_kosherp)(int rnk, const int *n, int howmany)
{
     return (howmany >= 0) && rowmajor_kosherp(rnk, n);
}
