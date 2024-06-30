/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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

#include "libbench2/bench.h"
#include <stdlib.h>

bench_tensor *mktensor(int rnk) 
{
     bench_tensor *x;

     BENCH_ASSERT(rnk >= 0);

     x = (bench_tensor *)bench_malloc(sizeof(bench_tensor));
     if (BENCH_FINITE_RNK(rnk) && rnk > 0)
          x->dims = (bench_iodim *)bench_malloc(sizeof(bench_iodim) * rnk);
     else
          x->dims = 0;

     x->rnk = rnk;
     return x;
}

void tensor_destroy(bench_tensor *sz)
{
     bench_free0(sz->dims);
     bench_free(sz);
}

size_t tensor_sz(const bench_tensor *sz)
{
     int i;
     size_t n = 1;

     if (!BENCH_FINITE_RNK(sz->rnk))
          return 0;

     for (i = 0; i < sz->rnk; ++i)
          n *= sz->dims[i].n;
     return n;
}


/* total order among bench_iodim's */
static int dimcmp(const bench_iodim *a, const bench_iodim *b)
{
     if (b->is != a->is)
          return (b->is - a->is);	/* shorter strides go later */
     if (b->os != a->os)
          return (b->os - a->os);	/* shorter strides go later */
     return (int)(a->n - b->n);	        /* larger n's go later */
}

bench_tensor *tensor_compress(const bench_tensor *sz)
{
     int i, rnk;
     bench_tensor *x;

     BENCH_ASSERT(BENCH_FINITE_RNK(sz->rnk));
     for (i = rnk = 0; i < sz->rnk; ++i) {
          BENCH_ASSERT(sz->dims[i].n > 0);
          if (sz->dims[i].n != 1)
               ++rnk;
     }

     x = mktensor(rnk);
     for (i = rnk = 0; i < sz->rnk; ++i) {
          if (sz->dims[i].n != 1)
               x->dims[rnk++] = sz->dims[i];
     }

     if (rnk) {
	  /* God knows how qsort() behaves if n==0 */
	  qsort(x->dims, (size_t)x->rnk, sizeof(bench_iodim),
		(int (*)(const void *, const void *))dimcmp);
     }

     return x;
}

int tensor_unitstridep(bench_tensor *t)
{
     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));
     return (t->rnk == 0 ||
	     (t->dims[t->rnk - 1].is == 1 && t->dims[t->rnk - 1].os == 1));
}

/* detect screwy real padded rowmajor... ugh */
int tensor_real_rowmajorp(bench_tensor *t, int sign, int in_place)
{
     int i;

     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));

     i = t->rnk - 1;

     if (--i >= 0) {
          bench_iodim *d = t->dims + i;
	  if (sign < 0) {
	       if (d[0].is != d[1].is * (in_place ? 2*(d[1].n/2 + 1) : d[1].n))
		    return 0;
	       if (d[0].os != d[1].os * (d[1].n/2 + 1))
		    return 0;
	  }
	  else {
	       if (d[0].is != d[1].is * (d[1].n/2 + 1))
		    return 0;
	       if (d[0].os != d[1].os * (in_place ? 2*(d[1].n/2 + 1) : d[1].n))
		    return 0;
	  }
     }

     while (--i >= 0) {
          bench_iodim *d = t->dims + i;
          if (d[0].is != d[1].is * d[1].n)
               return 0;
          if (d[0].os != d[1].os * d[1].n)
               return 0;
     }
     return 1;
}

int tensor_rowmajorp(bench_tensor *t)
{
     int i;

     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));

     i = t->rnk - 1;
     while (--i >= 0) {
	  bench_iodim *d = t->dims + i;
	  if (d[0].is != d[1].is * d[1].n)
	       return 0;
	  if (d[0].os != d[1].os * d[1].n)
	       return 0;
     }
     return 1;
}

static void dimcpy(bench_iodim *dst, const bench_iodim *src, int rnk)
{
     int i;
     if (BENCH_FINITE_RNK(rnk))
          for (i = 0; i < rnk; ++i)
               dst[i] = src[i];
}

bench_tensor *tensor_append(const bench_tensor *a, const bench_tensor *b)
{
     if (!BENCH_FINITE_RNK(a->rnk) || !BENCH_FINITE_RNK(b->rnk)) {
          return mktensor(BENCH_RNK_MINFTY);
     } else {
	  bench_tensor *x = mktensor(a->rnk + b->rnk);
          dimcpy(x->dims, a->dims, a->rnk);
          dimcpy(x->dims + a->rnk, b->dims, b->rnk);
	  return x;
     }
}

static int imax(int a, int b)
{
     return (a > b) ? a : b;
}

static int imin(int a, int b)
{
     return (a < b) ? a : b;
}

#define DEFBOUNDS(name, xs)			\
void name(bench_tensor *t, int *lbp, int *ubp)	\
{						\
     int lb = 0;				\
     int ub = 1;				\
     int i;					\
						\
     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));		\
						\
     for (i = 0; i < t->rnk; ++i) {		\
	  bench_iodim *d = t->dims + i;		\
	  int n = d->n;				\
	  int s = d->xs;			\
	  lb = imin(lb, lb + s * (n - 1));	\
	  ub = imax(ub, ub + s * (n - 1));	\
     }						\
						\
     *lbp = lb;					\
     *ubp = ub;					\
}

DEFBOUNDS(tensor_ibounds, is)
DEFBOUNDS(tensor_obounds, os)

bench_tensor *tensor_copy(const bench_tensor *sz)
{
     bench_tensor *x = mktensor(sz->rnk);
     dimcpy(x->dims, sz->dims, sz->rnk);
     return x;
}

/* Like tensor_copy, but copy only rnk dimensions starting with start_dim. */
bench_tensor *tensor_copy_sub(const bench_tensor *sz, int start_dim, int rnk)
{
     bench_tensor *x;

     BENCH_ASSERT(BENCH_FINITE_RNK(sz->rnk) && start_dim + rnk <= sz->rnk);
     x = mktensor(rnk);
     dimcpy(x->dims, sz->dims + start_dim, rnk);
     return x;
}

bench_tensor *tensor_copy_swapio(const bench_tensor *sz)
{
     bench_tensor *x = tensor_copy(sz);
     int i;
     if (BENCH_FINITE_RNK(x->rnk))
	  for (i = 0; i < x->rnk; ++i) {
	       int s;
	       s = x->dims[i].is;
	       x->dims[i].is = x->dims[i].os;
	       x->dims[i].os = s;
	  }
     return x;
}
