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



/* Plans for handling vector transform loops.  These are *just* the
   loops, and rely on child plans for the actual RDFT2s.
 
   They form a wrapper around solvers that don't have apply functions
   for non-null vectors.
 
   vrank-geq1-rdft2 plans also recursively handle the case of
   multi-dimensional vectors, obviating the need for most solvers to
   deal with this.  We can also play games here, such as reordering
   the vector loops.
 
   Each vrank-geq1-rdft2 plan reduces the vector rank by 1, picking out a
   dimension determined by the vecloop_dim field of the solver. */

#include "rdft/rdft.h"

typedef struct {
     solver super;
     int vecloop_dim;
     const int *buddies;
     size_t nbuddies;
} S;

typedef struct {
     plan_rdft2 super;

     plan *cld;
     INT vl;
     INT rvs, cvs;
     const S *solver;
} P;

static void apply(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     INT i, vl = ego->vl;
     INT rvs = ego->rvs, cvs = ego->cvs;
     rdft2apply cldapply = ((plan_rdft2 *) ego->cld)->apply;

     for (i = 0; i < vl; ++i) {
          cldapply(ego->cld, r0 + i * rvs, r1 + i * rvs,
		   cr + i * cvs, ci + i * cvs);
     }
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->solver;
     p->print(p, "(rdft2-vrank>=1-x%D/%d%(%p%))",
	      ego->vl, s->vecloop_dim, ego->cld);
}

static int pickdim(const S *ego, const tensor *vecsz, int oop, int *dp)
{
     return X(pickdim)(ego->vecloop_dim, ego->buddies, ego->nbuddies,
		       vecsz, oop, dp);
}

static int applicable0(const solver *ego_, const problem *p_, int *dp)
{
     const S *ego = (const S *) ego_;
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     if (FINITE_RNK(p->vecsz->rnk)
	 && p->vecsz->rnk > 0
	 && pickdim(ego, p->vecsz, p->r0 != p->cr, dp)) {
	  if (p->r0 != p->cr)
	       return 1;  /* can always operate out-of-place */

	  return(X(rdft2_inplace_strides)(p, *dp));
     }

     return 0;
}


static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr, int *dp)
{
     const S *ego = (const S *)ego_;
     if (!applicable0(ego_, p_, dp)) return 0;

     /* fftw2 behavior */
     if (NO_VRANK_SPLITSP(plnr) && (ego->vecloop_dim != ego->buddies[0]))
	  return 0;

     if (NO_UGLYP(plnr)) {
	  const problem_rdft2 *p = (const problem_rdft2 *) p_;
	  iodim *d = p->vecsz->dims + *dp;
	       
	  /* Heuristic: if the transform is multi-dimensional, and the
	     vector stride is less than the transform size, then we
	     probably want to use a rank>=2 plan first in order to combine
	     this vector with the transform-dimension vectors. */
	  if (p->sz->rnk > 1
	      && X(imin)(X(iabs)(d->is), X(iabs)(d->os))
	      < X(rdft2_tensor_max_index)(p->sz, p->kind)
	       )
	       return 0;

	  /* Heuristic: don't use a vrank-geq1 for rank-0 vrank-1
	     transforms, since this case is better handled by rank-0
	     solvers. */
	  if (p->sz->rnk == 0 && p->vecsz->rnk == 1) return 0;

	  if (NO_NONTHREADEDP(plnr)) 
	       return 0; /* prefer threaded version */
     }

     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_rdft2 *p;
     P *pln;
     plan *cld;
     int vdim;
     iodim *d;
     INT rvs, cvs;

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr, &vdim))
          return (plan *) 0;
     p = (const problem_rdft2 *) p_;

     d = p->vecsz->dims + vdim;

     A(d->n > 1);  /* or else, p->ri + d->is etc. are invalid */

     X(rdft2_strides)(p->kind, d, &rvs, &cvs);

     cld = X(mkplan_d)(plnr, 
		       X(mkproblem_rdft2_d)(
			    X(tensor_copy)(p->sz),
			    X(tensor_copy_except)(p->vecsz, vdim),
			    TAINT(p->r0, rvs), TAINT(p->r1, rvs), 
			    TAINT(p->cr, cvs), TAINT(p->ci, cvs),
			    p->kind));
     if (!cld) return (plan *) 0;

     pln = MKPLAN_RDFT2(P, &padt, apply);

     pln->cld = cld;
     pln->vl = d->n;
     pln->rvs = rvs;
     pln->cvs = cvs;

     pln->solver = ego;
     X(ops_zero)(&pln->super.super.ops);
     pln->super.super.ops.other = 3.14159; /* magic to prefer codelet loops */
     X(ops_madd2)(pln->vl, &cld->ops, &pln->super.super.ops);

     if (p->sz->rnk != 1 || (p->sz->dims[0].n > 128))
	  pln->super.super.pcost = pln->vl * cld->pcost;

     return &(pln->super.super);
}

static solver *mksolver(int vecloop_dim, const int *buddies, size_t nbuddies)
{
     static const solver_adt sadt = { PROBLEM_RDFT2, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->vecloop_dim = vecloop_dim;
     slv->buddies = buddies;
     slv->nbuddies = nbuddies;
     return &(slv->super);
}

void X(rdft2_vrank_geq1_register)(planner *p)
{
     /* FIXME: Should we try other vecloop_dim values? */
     static const int buddies[] = { 1, -1 };
     size_t i;

     for (i = 0; i < NELEM(buddies); ++i)
          REGISTER_SOLVER(p, mksolver(buddies[i], buddies, NELEM(buddies)));
}
