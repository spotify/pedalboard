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


/* plans for RDFT of rank >= 2 (multidimensional) */

/* FIXME: this solver cannot strictly be applied to multidimensional
   DHTs, since the latter are not separable...up to rnk-1 additional
   post-processing passes may be required.  See also:

   R. N. Bracewell, O. Buneman, H. Hao, and J. Villasenor, "Fast
   two-dimensional Hartley transform," Proc. IEEE 74, 1282-1283 (1986).

   H. Hao and R. N. Bracewell, "A three-dimensional DFT algorithm
   using the fast Hartley transform," Proc. IEEE 75(2), 264-266 (1987).
*/

#include "rdft/rdft.h"

typedef struct {
     solver super;
     int spltrnk;
     const int *buddies;
     size_t nbuddies;
} S;

typedef struct {
     plan_rdft super;

     plan *cld1, *cld2;
     const S *solver;
} P;

/* Compute multi-dimensional RDFT by applying the two cld plans
   (lower-rnk RDFTs). */
static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld1, *cld2;

     cld1 = (plan_rdft *) ego->cld1;
     cld1->apply(ego->cld1, I, O);

     cld2 = (plan_rdft *) ego->cld2;
     cld2->apply(ego->cld2, O, O);
}


static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld1, wakefulness);
     X(plan_awake)(ego->cld2, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld2);
     X(plan_destroy_internal)(ego->cld1);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->solver;
     p->print(p, "(rdft-rank>=2/%d%(%p%)%(%p%))",
	      s->spltrnk, ego->cld1, ego->cld2);
}

static int picksplit(const S *ego, const tensor *sz, int *rp)
{
     A(sz->rnk > 1); /* cannot split rnk <= 1 */
     if (!X(pickdim)(ego->spltrnk, ego->buddies, ego->nbuddies, sz, 1, rp))
	  return 0;
     *rp += 1; /* convert from dim. index to rank */
     if (*rp >= sz->rnk) /* split must reduce rank */
	  return 0;
     return 1;
}

static int applicable0(const solver *ego_, const problem *p_, int *rp)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     const S *ego = (const S *)ego_;
     return (1
	     && FINITE_RNK(p->sz->rnk) && FINITE_RNK(p->vecsz->rnk)
	     && p->sz->rnk >= 2
	     && picksplit(ego, p->sz, rp)
	  );
}

/* TODO: revise this. */
static int applicable(const solver *ego_, const problem *p_, 
		      const planner *plnr, int *rp)
{
     const S *ego = (const S *)ego_;

     if (!applicable0(ego_, p_, rp)) return 0;

     if (NO_RANK_SPLITSP(plnr) && (ego->spltrnk != ego->buddies[0]))
	  return 0;

     if (NO_UGLYP(plnr)) {
	  /* Heuristic: if the vector stride is greater than the transform
	     sz, don't use (prefer to do the vector loop first with a
	     vrank-geq1 plan). */
	  const problem_rdft *p = (const problem_rdft *) p_;

	  if (p->vecsz->rnk > 0 &&
	      X(tensor_min_stride)(p->vecsz) > X(tensor_max_index)(p->sz))
	       return 0;
     }

     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_rdft *p;
     P *pln;
     plan *cld1 = 0, *cld2 = 0;
     tensor *sz1, *sz2, *vecszi, *sz2i;
     int spltrnk;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr, &spltrnk))
          return (plan *) 0;

     p = (const problem_rdft *) p_;
     X(tensor_split)(p->sz, &sz1, spltrnk, &sz2);
     vecszi = X(tensor_copy_inplace)(p->vecsz, INPLACE_OS);
     sz2i = X(tensor_copy_inplace)(sz2, INPLACE_OS);

     cld1 = X(mkplan_d)(plnr, 
			X(mkproblem_rdft_d)(X(tensor_copy)(sz2),
					    X(tensor_append)(p->vecsz, sz1),
					    p->I, p->O, p->kind + spltrnk));
     if (!cld1) goto nada;

     cld2 = X(mkplan_d)(plnr, 
			X(mkproblem_rdft_d)(
			     X(tensor_copy_inplace)(sz1, INPLACE_OS),
			     X(tensor_append)(vecszi, sz2i),
			     p->O, p->O, p->kind));
     if (!cld2) goto nada;

     pln = MKPLAN_RDFT(P, &padt, apply);

     pln->cld1 = cld1;
     pln->cld2 = cld2;

     pln->solver = ego;
     X(ops_add)(&cld1->ops, &cld2->ops, &pln->super.super.ops);

     X(tensor_destroy4)(sz2, sz1, vecszi, sz2i);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld2);
     X(plan_destroy_internal)(cld1);
     X(tensor_destroy4)(sz2, sz1, vecszi, sz2i);
     return (plan *) 0;
}

static solver *mksolver(int spltrnk, const int *buddies, size_t nbuddies)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->spltrnk = spltrnk;
     slv->buddies = buddies;
     slv->nbuddies = nbuddies;
     return &(slv->super);
}

void X(rdft_rank_geq2_register)(planner *p)
{
     static const int buddies[] = { 1, 0, -2 };
     size_t i;

     for (i = 0; i < NELEM(buddies); ++i)
          REGISTER_SOLVER(p, mksolver(buddies[i], buddies, NELEM(buddies)));

     /* FIXME: Should we try more buddies?  See also dft/rank-geq2. */
}
