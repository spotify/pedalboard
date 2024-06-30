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

static int signof(INT x)
{
     if (x < 0) return -1;
     if (x == 0) return 0;
     /* if (x > 0) */ return 1;
}

/* total order among iodim's */
int X(dimcmp)(const iodim *a, const iodim *b)
{
     INT sai = X(iabs)(a->is), sbi = X(iabs)(b->is);
     INT sao = X(iabs)(a->os), sbo = X(iabs)(b->os);
     INT sam = X(imin)(sai, sao), sbm = X(imin)(sbi, sbo);

     /* in descending order of min{istride, ostride} */
     if (sam != sbm)
	  return signof(sbm - sam);

     /* in case of a tie, in descending order of istride */
     if (sbi != sai)
          return signof(sbi - sai);

     /* in case of a tie, in descending order of ostride */
     if (sbo != sao)
          return signof(sbo - sao);

     /* in case of a tie, in ascending order of n */
     return signof(a->n - b->n);
}

static void canonicalize(tensor *x)
{
     if (x->rnk > 1) {
	  qsort(x->dims, (unsigned)x->rnk, sizeof(iodim),
		(int (*)(const void *, const void *))X(dimcmp));
     }
}

static int compare_by_istride(const iodim *a, const iodim *b)
{
     INT sai = X(iabs)(a->is), sbi = X(iabs)(b->is);

     /* in descending order of istride */
     return signof(sbi - sai);
}

static tensor *really_compress(const tensor *sz)
{
     int i, rnk;
     tensor *x;

     A(FINITE_RNK(sz->rnk));
     for (i = rnk = 0; i < sz->rnk; ++i) {
          A(sz->dims[i].n > 0);
          if (sz->dims[i].n != 1)
               ++rnk;
     }

     x = X(mktensor)(rnk);
     for (i = rnk = 0; i < sz->rnk; ++i) {
          if (sz->dims[i].n != 1)
               x->dims[rnk++] = sz->dims[i];
     }
     return x;
}

/* Like tensor_copy, but eliminate n == 1 dimensions, which
   never affect any transform or transform vector.
 
   Also, we sort the tensor into a canonical order of decreasing
   strides (see X(dimcmp) for an exact definition).  In general,
   processing a loop/array in order of decreasing stride will improve
   locality.  Both forward and backwards traversal of the tensor are
   considered e.g. by vrank-geq1, so sorting in increasing
   vs. decreasing order is not really important. */
tensor *X(tensor_compress)(const tensor *sz)
{
     tensor *x = really_compress(sz);
     canonicalize(x);
     return x;
}

/* Return whether the strides of a and b are such that they form an
   effective contiguous 1d array.  Assumes that a.is >= b.is. */
static int strides_contig(iodim *a, iodim *b)
{
     return (a->is == b->is * b->n && a->os == b->os * b->n);
}

/* Like tensor_compress, but also compress into one dimension any
   group of dimensions that form a contiguous block of indices with
   some stride.  (This can safely be done for transform vector sizes.) */
tensor *X(tensor_compress_contiguous)(const tensor *sz)
{
     int i, rnk;
     tensor *sz2, *x;

     if (X(tensor_sz)(sz) == 0) 
	  return X(mktensor)(RNK_MINFTY);

     sz2 = really_compress(sz);
     A(FINITE_RNK(sz2->rnk));

     if (sz2->rnk <= 1) { /* nothing to compress. */ 
	  if (0) {
	       /* this call is redundant, because "sz->rnk <= 1" implies
		  that the tensor is already canonical, but I am writing
		  it explicitly because "logically" we need to canonicalize
		  the tensor before returning. */
	       canonicalize(sz2);
	  }
          return sz2;
     }

     /* sort in descending order of |istride|, so that compressible
	dimensions appear contigously */
     qsort(sz2->dims, (unsigned)sz2->rnk, sizeof(iodim),
		(int (*)(const void *, const void *))compare_by_istride);

     /* compute what the rank will be after compression */
     for (i = rnk = 1; i < sz2->rnk; ++i)
          if (!strides_contig(sz2->dims + i - 1, sz2->dims + i))
               ++rnk;

     /* merge adjacent dimensions whenever possible */
     x = X(mktensor)(rnk);
     x->dims[0] = sz2->dims[0];
     for (i = rnk = 1; i < sz2->rnk; ++i) {
          if (strides_contig(sz2->dims + i - 1, sz2->dims + i)) {
               x->dims[rnk - 1].n *= sz2->dims[i].n;
               x->dims[rnk - 1].is = sz2->dims[i].is;
               x->dims[rnk - 1].os = sz2->dims[i].os;
          } else {
               A(rnk < x->rnk);
               x->dims[rnk++] = sz2->dims[i];
          }
     }

     X(tensor_destroy)(sz2);

     /* reduce to canonical form */
     canonicalize(x);
     return x;
}

/* The inverse of X(tensor_append): splits the sz tensor into
   tensor a followed by tensor b, where a's rank is arnk. */
void X(tensor_split)(const tensor *sz, tensor **a, int arnk, tensor **b)
{
     A(FINITE_RNK(sz->rnk) && FINITE_RNK(arnk));

     *a = X(tensor_copy_sub)(sz, 0, arnk);
     *b = X(tensor_copy_sub)(sz, arnk, sz->rnk - arnk);
}

/* TRUE if the two tensors are equal */
int X(tensor_equal)(const tensor *a, const tensor *b)
{
     if (a->rnk != b->rnk)
	  return 0;

     if (FINITE_RNK(a->rnk)) {
	  int i;
	  for (i = 0; i < a->rnk; ++i) 
	       if (0
		   || a->dims[i].n != b->dims[i].n
		   || a->dims[i].is != b->dims[i].is
		   || a->dims[i].os != b->dims[i].os
		    )
		    return 0;
     }

     return 1;
}

/* TRUE if the sets of input and output locations described by
   (append sz vecsz) are the same */
int X(tensor_inplace_locations)(const tensor *sz, const tensor *vecsz)
{
     tensor *t = X(tensor_append)(sz, vecsz);
     tensor *ti = X(tensor_copy_inplace)(t, INPLACE_IS);
     tensor *to = X(tensor_copy_inplace)(t, INPLACE_OS);
     tensor *tic = X(tensor_compress_contiguous)(ti);
     tensor *toc = X(tensor_compress_contiguous)(to);

     int retval = X(tensor_equal)(tic, toc);

     X(tensor_destroy)(t);
     X(tensor_destroy4)(ti, to, tic, toc);

     return retval;
}
