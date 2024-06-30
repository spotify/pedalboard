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

INT X(tensor_max_index)(const tensor *sz)
{
     int i;
     INT ni = 0, no = 0;

     A(FINITE_RNK(sz->rnk));
     for (i = 0; i < sz->rnk; ++i) {
          const iodim *p = sz->dims + i;
          ni += (p->n - 1) * X(iabs)(p->is);
          no += (p->n - 1) * X(iabs)(p->os);
     }
     return X(imax)(ni, no);
}

#define tensor_min_xstride(sz, xs) {			\
     A(FINITE_RNK(sz->rnk));				\
     if (sz->rnk == 0) return 0;			\
     else {						\
          int i;					\
          INT s = X(iabs)(sz->dims[0].xs);		\
          for (i = 1; i < sz->rnk; ++i)			\
               s = X(imin)(s, X(iabs)(sz->dims[i].xs));	\
          return s;					\
     }							\
}

INT X(tensor_min_istride)(const tensor *sz) tensor_min_xstride(sz, is)
INT X(tensor_min_ostride)(const tensor *sz) tensor_min_xstride(sz, os)

INT X(tensor_min_stride)(const tensor *sz)
{
     return X(imin)(X(tensor_min_istride)(sz), X(tensor_min_ostride)(sz));
}

int X(tensor_inplace_strides)(const tensor *sz)
{
     int i;
     A(FINITE_RNK(sz->rnk));
     for (i = 0; i < sz->rnk; ++i) {
          const iodim *p = sz->dims + i;
          if (p->is != p->os)
               return 0;
     }
     return 1;
}

int X(tensor_inplace_strides2)(const tensor *a, const tensor *b)
{
     return X(tensor_inplace_strides(a)) && X(tensor_inplace_strides(b));
}

/* return true (1) iff *any* strides of sz decrease when we
   tensor_inplace_copy(sz, k). */
static int tensor_strides_decrease(const tensor *sz, inplace_kind k)
{
     if (FINITE_RNK(sz->rnk)) {
          int i;
          for (i = 0; i < sz->rnk; ++i)
               if ((sz->dims[i].os - sz->dims[i].is)
                   * (k == INPLACE_OS ? (INT)1 : (INT)-1) < 0)
                    return 1;
     }
     return 0;
}

/* Return true (1) iff *any* strides of sz decrease when we
   tensor_inplace_copy(k) *or* if *all* strides of sz are unchanged
   but *any* strides of vecsz decrease.  This is used in indirect.c
   to determine whether to use INPLACE_IS or INPLACE_OS.

   Note: X(tensor_strides_decrease)(sz, vecsz, INPLACE_IS)
         || X(tensor_strides_decrease)(sz, vecsz, INPLACE_OS)
         || X(tensor_inplace_strides2)(p->sz, p->vecsz)
   must always be true. */
int X(tensor_strides_decrease)(const tensor *sz, const tensor *vecsz,
			       inplace_kind k)
{
     return(tensor_strides_decrease(sz, k)
	    || (X(tensor_inplace_strides)(sz)
		&& tensor_strides_decrease(vecsz, k)));
}
