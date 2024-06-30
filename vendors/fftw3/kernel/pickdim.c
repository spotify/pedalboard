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


/* Given a solver which_dim, a vector sz, and whether or not the
   transform is out-of-place, return the actual dimension index that
   it corresponds to.  The basic idea here is that we return the
   which_dim'th valid dimension, starting from the end if
   which_dim < 0. */
static int really_pickdim(int which_dim, const tensor *sz, int oop, int *dp)
{
     int i;
     int count_ok = 0;
     if (which_dim > 0) {
          for (i = 0; i < sz->rnk; ++i) {
               if (oop || sz->dims[i].is == sz->dims[i].os)
                    if (++count_ok == which_dim) {
                         *dp = i;
                         return 1;
                    }
          }
     }
     else if (which_dim < 0) {
          for (i = sz->rnk - 1; i >= 0; --i) {
               if (oop || sz->dims[i].is == sz->dims[i].os)
                    if (++count_ok == -which_dim) {
                         *dp = i;
                         return 1;
                    }
          }
     }
     else { /* zero: pick the middle, if valid */
	  i = (sz->rnk - 1) / 2;
	  if (i >= 0 && (oop || sz->dims[i].is == sz->dims[i].os)) {
	       *dp = i;
	       return 1;
	  }
     }
     return 0;
}

/* Like really_pickdim, but only returns 1 if no previous "buddy"
   which_dim in the buddies list would give the same dim. */
int X(pickdim)(int which_dim, const int *buddies, size_t nbuddies,
	       const tensor *sz, int oop, int *dp)
{
     size_t i;
     int d1;

     if (!really_pickdim(which_dim, sz, oop, dp))
          return 0;

     /* check whether some buddy solver would produce the same dim.
        If so, consider this solver unapplicable and let the buddy
        take care of it.  The smallest-indexed buddy is applicable. */
     for (i = 0; i < nbuddies; ++i) {
          if (buddies[i] == which_dim)
               break;  /* found self */
          if (really_pickdim(buddies[i], sz, oop, &d1) && *dp == d1)
               return 0; /* found equivalent buddy */
     }
     return 1;
}
