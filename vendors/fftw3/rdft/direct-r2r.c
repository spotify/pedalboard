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


/* direct RDFT solver, using r2r codelets */

#include "rdft/rdft.h"

typedef struct {
     solver super;
     const kr2r_desc *desc;
     kr2r k;
} S;

typedef struct {
     plan_rdft super;

     INT vl, ivs, ovs;
     stride is, os;
     kr2r k;
     const S *slv;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     ASSERT_ALIGNED_DOUBLE;
     ego->k(I, O, ego->is, ego->os, ego->vl, ego->ivs, ego->ovs);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(stride_destroy)(ego->is);
     X(stride_destroy)(ego->os);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->slv;

     p->print(p, "(rdft-%s-direct-r2r-%D%v \"%s\")", 
	      X(rdft_kind_str)(s->desc->kind), s->desc->n,
	      ego->vl, s->desc->nam);
}

static int applicable(const solver *ego_, const problem *p_)
{
     const S *ego = (const S *) ego_;
     const problem_rdft *p = (const problem_rdft *) p_;
     INT vl;
     INT ivs, ovs;

     return (
	  1
	  && p->sz->rnk == 1
	  && p->vecsz->rnk <= 1
	  && p->sz->dims[0].n == ego->desc->n
	  && p->kind[0] == ego->desc->kind

	  /* check strides etc */
	  && X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs)

	  && (0
	      /* can operate out-of-place */
	      || p->I != p->O

	      /* computing one transform */
	      || vl == 1

	      /* can operate in-place as long as strides are the same */
	      || X(tensor_inplace_strides2)(p->sz, p->vecsz)
	       )
	  );
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     const problem_rdft *p;
     iodim *d;

     static const plan_adt padt = {
	  X(rdft_solve), X(null_awake), print, destroy
     };

     UNUSED(plnr);

     if (!applicable(ego_, p_))
          return (plan *)0;

     p = (const problem_rdft *) p_;


     pln = MKPLAN_RDFT(P, &padt, apply);

     d = p->sz->dims;

     pln->k = ego->k;

     pln->is = X(mkstride)(d->n, d->is);
     pln->os = X(mkstride)(d->n, d->os);

     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);

     pln->slv = ego;
     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(pln->vl / ego->desc->genus->vl,
		  &ego->desc->ops,
		  &pln->super.super.ops);

     pln->super.super.could_prune_now_p = 1;

     return &(pln->super.super);
}

/* constructor */
solver *X(mksolver_rdft_r2r_direct)(kr2r k, const kr2r_desc *desc)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->k = k;
     slv->desc = desc;
     return &(slv->super);
}

