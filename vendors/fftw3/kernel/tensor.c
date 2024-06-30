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

tensor *X(mktensor)(int rnk) 
{
     tensor *x;

     A(rnk >= 0);

#if defined(STRUCT_HACK_KR)
     if (FINITE_RNK(rnk) && rnk > 1)
	  x = (tensor *)MALLOC(sizeof(tensor) + (unsigned)(rnk - 1) * sizeof(iodim),
				    TENSORS);
     else
	  x = (tensor *)MALLOC(sizeof(tensor), TENSORS);
#elif defined(STRUCT_HACK_C99)
     if (FINITE_RNK(rnk))
	  x = (tensor *)MALLOC(sizeof(tensor) + (unsigned)rnk * sizeof(iodim),
				    TENSORS);
     else
	  x = (tensor *)MALLOC(sizeof(tensor), TENSORS);
#else
     x = (tensor *)MALLOC(sizeof(tensor), TENSORS);
     if (FINITE_RNK(rnk) && rnk > 0)
          x->dims = (iodim *)MALLOC(sizeof(iodim) * (unsigned)rnk, TENSORS);
     else
          x->dims = 0;
#endif

     x->rnk = rnk;
     return x;
}

void X(tensor_destroy)(tensor *sz)
{
#if !defined(STRUCT_HACK_C99) && !defined(STRUCT_HACK_KR)
     X(ifree0)(sz->dims);
#endif
     X(ifree)(sz);
}

INT X(tensor_sz)(const tensor *sz)
{
     int i;
     INT n = 1;

     if (!FINITE_RNK(sz->rnk))
          return 0;

     for (i = 0; i < sz->rnk; ++i)
          n *= sz->dims[i].n;
     return n;
}

void X(tensor_md5)(md5 *p, const tensor *t)
{
     int i;
     X(md5int)(p, t->rnk);
     if (FINITE_RNK(t->rnk)) {
	  for (i = 0; i < t->rnk; ++i) {
	       const iodim *q = t->dims + i;
	       X(md5INT)(p, q->n);
	       X(md5INT)(p, q->is);
	       X(md5INT)(p, q->os);
	  }
     }
}

/* treat a (rank <= 1)-tensor as a rank-1 tensor, extracting
   appropriate n, is, and os components */
int X(tensor_tornk1)(const tensor *t, INT *n, INT *is, INT *os)
{
     A(t->rnk <= 1);
     if (t->rnk == 1) {
	  const iodim *vd = t->dims;
          *n = vd[0].n;
          *is = vd[0].is;
          *os = vd[0].os;
     } else {
          *n = 1;
          *is = *os = 0;
     }
     return 1;
}

void X(tensor_print)(const tensor *x, printer *p)
{
     if (FINITE_RNK(x->rnk)) {
	  int i;
	  int first = 1;
	  p->print(p, "(");
	  for (i = 0; i < x->rnk; ++i) {
	       const iodim *d = x->dims + i;
	       p->print(p, "%s(%D %D %D)", 
			first ? "" : " ",
			d->n, d->is, d->os);
	       first = 0;
	  }
	  p->print(p, ")");
     } else {
	  p->print(p, "rank-minfty"); 
     }
}
