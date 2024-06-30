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


#include "dft/dft.h"
#include "rdft/rdft.h"
#include <stddef.h>

static void destroy(problem *ego_)
{
     problem_rdft2 *ego = (problem_rdft2 *) ego_;
     X(tensor_destroy2)(ego->vecsz, ego->sz);
     X(ifree)(ego_);
}

static void hash(const problem *p_, md5 *m)
{
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     X(md5puts)(m, "rdft2");
     X(md5int)(m, p->r0 == p->cr);
     X(md5INT)(m, p->r1 - p->r0);
     X(md5INT)(m, p->ci - p->cr);
     X(md5int)(m, X(ialignment_of)(p->r0));
     X(md5int)(m, X(ialignment_of)(p->r1));
     X(md5int)(m, X(ialignment_of)(p->cr)); 
     X(md5int)(m, X(ialignment_of)(p->ci)); 
     X(md5int)(m, p->kind);
     X(tensor_md5)(m, p->sz);
     X(tensor_md5)(m, p->vecsz);
}

static void print(const problem *ego_, printer *p)
{
     const problem_rdft2 *ego = (const problem_rdft2 *) ego_;
     p->print(p, "(rdft2 %d %d %T %T)", 
	      (int)(ego->cr == ego->r0), 
	      (int)(ego->kind),
	      ego->sz,
	      ego->vecsz);
}

static void recur(const iodim *dims, int rnk, R *I0, R *I1)
{
     if (rnk == RNK_MINFTY)
          return;
     else if (rnk == 0)
          I0[0] = K(0.0);
     else if (rnk > 0) {
          INT i, n = dims[0].n, is = dims[0].is;

	  if (rnk == 1) {
	       for (i = 0; i < n - 1; i += 2) {
		    *I0 = *I1 = K(0.0);
		    I0 += is; I1 += is;
	       }
	       if (i < n) 
		    *I0 = K(0.0);
	  } else {
	       for (i = 0; i < n; ++i)
		    recur(dims + 1, rnk - 1, I0 + i * is, I1 + i * is);
	  }
     }
}

static void vrecur(const iodim *vdims, int vrnk,
		   const iodim *dims, int rnk, R *I0, R *I1)
{
     if (vrnk == RNK_MINFTY)
          return;
     else if (vrnk == 0)
	  recur(dims, rnk, I0, I1);
     else if (vrnk > 0) {
          INT i, n = vdims[0].n, is = vdims[0].is;

	  for (i = 0; i < n; ++i)
	       vrecur(vdims + 1, vrnk - 1, 
		      dims, rnk, I0 + i * is, I1 + i * is);
     }
}

INT X(rdft2_complex_n)(INT real_n, rdft_kind kind)
{
     switch (kind) {
	 case R2HC:
	 case HC2R:
	      return (real_n / 2) + 1;
	 case R2HCII:
	 case HC2RIII:
	      return (real_n + 1) / 2;
	 default:
	      /* can't happen */
	      A(0);
	      return 0;
     }
}

static void zero(const problem *ego_)
{
     const problem_rdft2 *ego = (const problem_rdft2 *) ego_;
     if (R2HC_KINDP(ego->kind)) {
	  /* FIXME: can we avoid the double recursion somehow? */
	  vrecur(ego->vecsz->dims, ego->vecsz->rnk, 
		 ego->sz->dims, ego->sz->rnk, 
		 UNTAINT(ego->r0), UNTAINT(ego->r1));
     } else {
	  tensor *sz;
	  tensor *sz2 = X(tensor_copy)(ego->sz);
	  int rnk = sz2->rnk;
	  if (rnk > 0) /* ~half as many complex outputs */
	       sz2->dims[rnk-1].n = 
		    X(rdft2_complex_n)(sz2->dims[rnk-1].n, ego->kind);
	  sz = X(tensor_append)(ego->vecsz, sz2);
	  X(tensor_destroy)(sz2);
	  X(dft_zerotens)(sz, UNTAINT(ego->cr), UNTAINT(ego->ci));
	  X(tensor_destroy)(sz);
     }
}

static const problem_adt padt =
{
     PROBLEM_RDFT2,
     hash,
     zero,
     print,
     destroy
};

problem *X(mkproblem_rdft2)(const tensor *sz, const tensor *vecsz,
			    R *r0, R *r1, R *cr, R *ci,
			    rdft_kind kind)
{
     problem_rdft2 *ego;

     A(kind == R2HC || kind == R2HCII || kind == HC2R || kind == HC2RIII);
     A(X(tensor_kosherp)(sz));
     A(X(tensor_kosherp)(vecsz));
     A(FINITE_RNK(sz->rnk));

     /* require in-place problems to use r0 == cr */
     if (UNTAINT(r0) == UNTAINT(ci))
	  return X(mkproblem_unsolvable)();

     /* FIXME: should check UNTAINT(r1) == UNTAINT(cr) but
	only if odd elements exist, which requires compressing the 
	tensors first */

     if (UNTAINT(r0) == UNTAINT(cr)) 
	  r0 = cr = JOIN_TAINT(r0, cr);

     ego = (problem_rdft2 *)X(mkproblem)(sizeof(problem_rdft2), &padt);

     if (sz->rnk > 1) { /* have to compress rnk-1 dims separately, ugh */
	  tensor *szc = X(tensor_copy_except)(sz, sz->rnk - 1);
	  tensor *szr = X(tensor_copy_sub)(sz, sz->rnk - 1, 1);
	  tensor *szcc = X(tensor_compress)(szc);
	  if (szcc->rnk > 0)
	       ego->sz = X(tensor_append)(szcc, szr);
	  else
	       ego->sz = X(tensor_compress)(szr);
	  X(tensor_destroy2)(szc, szr); X(tensor_destroy)(szcc);
     } else {
	  ego->sz = X(tensor_compress)(sz);
     }
     ego->vecsz = X(tensor_compress_contiguous)(vecsz);
     ego->r0 = r0;
     ego->r1 = r1;
     ego->cr = cr;
     ego->ci = ci;
     ego->kind = kind;

     A(FINITE_RNK(ego->sz->rnk));
     return &(ego->super);

}

/* Same as X(mkproblem_rdft2), but also destroy input tensors. */
problem *X(mkproblem_rdft2_d)(tensor *sz, tensor *vecsz,
			      R *r0, R *r1, R *cr, R *ci, rdft_kind kind)
{
     problem *p = X(mkproblem_rdft2)(sz, vecsz, r0, r1, cr, ci, kind);
     X(tensor_destroy2)(vecsz, sz);
     return p;
}

/* Same as X(mkproblem_rdft2_d), but with only one R pointer.
   Used by the API. */
problem *X(mkproblem_rdft2_d_3pointers)(tensor *sz, tensor *vecsz,
					R *r0, R *cr, R *ci, rdft_kind kind)
{
     problem *p;
     int rnk = sz->rnk;
     R *r1;

     if (rnk == 0)
	  r1 = r0;
     else if (R2HC_KINDP(kind)) {
	  r1 = r0 + sz->dims[rnk-1].is;
	  sz->dims[rnk-1].is *= 2;
     } else {
	  r1 = r0 + sz->dims[rnk-1].os;
	  sz->dims[rnk-1].os *= 2;
     }

     p = X(mkproblem_rdft2)(sz, vecsz, r0, r1, cr, ci, kind);
     X(tensor_destroy2)(vecsz, sz);
     return p;
}
