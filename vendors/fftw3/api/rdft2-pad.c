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

#include <string.h>
#include "api/api.h"

const int *X(rdft2_pad)(int rnk, const int *n, const int *nembed,
			int inplace, int cmplx, int **nfree)
{
     A(FINITE_RNK(rnk));
     *nfree = 0;
     if (!nembed && rnk > 0) {
          if (inplace || cmplx) {
               int *np = (int *) MALLOC(sizeof(int) * (unsigned)rnk, PROBLEMS);
               memcpy(np, n, sizeof(int) * (unsigned)rnk);
               np[rnk - 1] = (n[rnk - 1] / 2 + 1) * (1 + !cmplx);
               nembed = *nfree = np;
          } else
               nembed = n;
     }
     return nembed;
}
