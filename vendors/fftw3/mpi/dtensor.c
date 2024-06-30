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

#include "ifftw-mpi.h"

dtensor *XM(mkdtensor)(int rnk) 
{
     dtensor *x;

     A(rnk >= 0);

#if defined(STRUCT_HACK_KR)
     if (FINITE_RNK(rnk) && rnk > 1)
	  x = (dtensor *)MALLOC(sizeof(dtensor) + (rnk - 1) * sizeof(ddim),
				    TENSORS);
     else
	  x = (dtensor *)MALLOC(sizeof(dtensor), TENSORS);
#elif defined(STRUCT_HACK_C99)
     if (FINITE_RNK(rnk))
	  x = (dtensor *)MALLOC(sizeof(dtensor) + rnk * sizeof(ddim),
				    TENSORS);
     else
	  x = (dtensor *)MALLOC(sizeof(dtensor), TENSORS);
#else
     x = (dtensor *)MALLOC(sizeof(dtensor), TENSORS);
     if (FINITE_RNK(rnk) && rnk > 0)
          x->dims = (ddim *)MALLOC(sizeof(ddim) * rnk, TENSORS);
     else
          x->dims = 0;
#endif

     x->rnk = rnk;
     return x;
}

void XM(dtensor_destroy)(dtensor *sz)
{
#if !defined(STRUCT_HACK_C99) && !defined(STRUCT_HACK_KR)
     X(ifree0)(sz->dims);
#endif
     X(ifree)(sz);
}

void XM(dtensor_md5)(md5 *p, const dtensor *t)
{
     int i;
     X(md5int)(p, t->rnk);
     if (FINITE_RNK(t->rnk)) {
          for (i = 0; i < t->rnk; ++i) {
               const ddim *q = t->dims + i;
               X(md5INT)(p, q->n);
               X(md5INT)(p, q->b[IB]);
               X(md5INT)(p, q->b[OB]);
          }
     }
}

dtensor *XM(dtensor_copy)(const dtensor *sz)
{
     dtensor *x = XM(mkdtensor)(sz->rnk);
     int i;
     if (FINITE_RNK(sz->rnk))
          for (i = 0; i < sz->rnk; ++i)
               x->dims[i] = sz->dims[i];
     return x;
}

dtensor *XM(dtensor_canonical)(const dtensor *sz, int compress)
{
     int i, rnk;
     dtensor *x;
     block_kind k;

     if (!FINITE_RNK(sz->rnk))
	  return XM(mkdtensor)(sz->rnk);
     for (i = rnk = 0; i < sz->rnk; ++i) {
	  if (sz->dims[i].n <= 0)
	       return XM(mkdtensor)(RNK_MINFTY);
	  else if (!compress || sz->dims[i].n > 1)
	       ++rnk;
     }
     x = XM(mkdtensor)(rnk);
     for (i = rnk = 0; i < sz->rnk; ++i) {
	  if (!compress || sz->dims[i].n > 1) {
               x->dims[rnk].n = sz->dims[i].n;
	       FORALL_BLOCK_KIND(k) {
		    if (XM(num_blocks)(sz->dims[i].n, sz->dims[i].b[k]) == 1)
			 x->dims[rnk].b[k] = sz->dims[i].n;
		    else
			 x->dims[rnk].b[k] = sz->dims[i].b[k];
	       }
	       ++rnk;
	  }
     }
     return x;
}

int XM(dtensor_validp)(const dtensor *sz)
{
     int i;
     if (sz->rnk < 0) return 0;
     if (FINITE_RNK(sz->rnk))
	  for (i = 0; i < sz->rnk; ++i)
	       if (sz->dims[i].n < 0
		   || sz->dims[i].b[IB] <= 0
		   || sz->dims[i].b[OB] <= 0)
		    return 0;
     return 1;
}

void XM(dtensor_print)(const dtensor *t, printer *p)
{
     if (FINITE_RNK(t->rnk)) {
          int i;
          int first = 1;
          p->print(p, "(");
          for (i = 0; i < t->rnk; ++i) {
               const ddim *d = t->dims + i;
               p->print(p, "%s(%D %D %D)",
                        first ? "" : " ",
                        d->n, d->b[IB], d->b[OB]);
               first = 0;
          }
          p->print(p, ")");
     } else {
          p->print(p, "rank-minfty");
     }

}
