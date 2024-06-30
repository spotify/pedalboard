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



/* solvers/plans for vectors of small RDFT's that cannot be done
   in-place directly.  Use a rank-0 plan to rearrange the data
   before or after the transform.  Can also change an out-of-place
   plan into a copy + in-place (where the in-place transform
   is e.g. unit stride). */

/* FIXME: merge with rank-geq2.c(?), since this is just a special case
   of a rank split where the first/second transform has rank 0. */

#include "rdft/rdft.h"

typedef problem *(*mkcld_t) (const problem_rdft *p);

typedef struct {
     rdftapply apply;
     problem *(*mkcld)(const problem_rdft *p);
     const char *nam;
} ndrct_adt;

typedef struct {
     solver super;
     const ndrct_adt *adt;
} S;

typedef struct {
     plan_rdft super;
     plan *cldcpy, *cld;
     const S *slv;
} P;

/*-----------------------------------------------------------------------*/
/* first rearrange, then transform */
static void apply_before(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;

     {
          plan_rdft *cldcpy = (plan_rdft *) ego->cldcpy;
          cldcpy->apply(ego->cldcpy, I, O);
     }
     {
          plan_rdft *cld = (plan_rdft *) ego->cld;
          cld->apply(ego->cld, O, O);
     }
}

static problem *mkcld_before(const problem_rdft *p)
{
     return X(mkproblem_rdft_d)(X(tensor_copy_inplace)(p->sz, INPLACE_OS),
				X(tensor_copy_inplace)(p->vecsz, INPLACE_OS),
				p->O, p->O, p->kind);
}

static const ndrct_adt adt_before =
{
     apply_before, mkcld_before, "rdft-indirect-before"
};

/*-----------------------------------------------------------------------*/
/* first transform, then rearrange */

static void apply_after(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;

     {
          plan_rdft *cld = (plan_rdft *) ego->cld;
          cld->apply(ego->cld, I, I);
     }
     {
          plan_rdft *cldcpy = (plan_rdft *) ego->cldcpy;
          cldcpy->apply(ego->cldcpy, I, O);
     }
}

static problem *mkcld_after(const problem_rdft *p)
{
     return X(mkproblem_rdft_d)(X(tensor_copy_inplace)(p->sz, INPLACE_IS),
				X(tensor_copy_inplace)(p->vecsz, INPLACE_IS),
				p->I, p->I, p->kind);
}

static const ndrct_adt adt_after =
{
     apply_after, mkcld_after, "rdft-indirect-after"
};

/*-----------------------------------------------------------------------*/
static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
     X(plan_destroy_internal)(ego->cldcpy);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cldcpy, wakefulness);
     X(plan_awake)(ego->cld, wakefulness);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->slv;
     p->print(p, "(%s%(%p%)%(%p%))", s->adt->nam, ego->cld, ego->cldcpy);
}

static int applicable0(const solver *ego_, const problem *p_,
		       const planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_rdft *p = (const problem_rdft *) p_;
     return (1
	     && FINITE_RNK(p->vecsz->rnk)

	     /* problem must be a nontrivial transform, not just a copy */
	     && p->sz->rnk > 0

	     && (0

		 /* problem must be in-place & require some
		    rearrangement of the data */
		 || (p->I == p->O
		     && !(X(tensor_inplace_strides2)(p->sz, p->vecsz)))

		 /* or problem must be out of place, transforming
		    from stride 1/2 to bigger stride, for apply_after */
		 || (p->I != p->O && ego->adt->apply == apply_after
		     && !NO_DESTROY_INPUTP(plnr)
		     && X(tensor_min_istride)(p->sz) <= 2
		     && X(tensor_min_ostride)(p->sz) > 2)
			  
		 /* or problem must be out of place, transforming
		    to stride 1/2 from bigger stride, for apply_before */
		 || (p->I != p->O && ego->adt->apply == apply_before
		     && X(tensor_min_ostride)(p->sz) <= 2
		     && X(tensor_min_istride)(p->sz) > 2)
			  
		  )
	  );
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr)
{
     if (!applicable0(ego_, p_, plnr)) return 0;
	  
     if (NO_INDIRECT_OP_P(plnr)) {
	  const problem_rdft *p = (const problem_rdft *)p_;
	  if (p->I != p->O) return 0;
     }

     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     const S *ego = (const S *) ego_;
     P *pln;
     plan *cld = 0, *cldcpy = 0;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *) 0;

     cldcpy = X(mkplan_d)(plnr,
			  X(mkproblem_rdft_0_d)(
			       X(tensor_append)(p->vecsz, p->sz),
			       p->I, p->O));
     if (!cldcpy) goto nada;

     cld = X(mkplan_f_d)(plnr, ego->adt->mkcld(p), NO_BUFFERING, 0, 0);
     if (!cld) goto nada;

     pln = MKPLAN_RDFT(P, &padt, ego->adt->apply);
     pln->cld = cld;
     pln->cldcpy = cldcpy;
     pln->slv = ego;
     X(ops_add)(&cld->ops, &cldcpy->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld);
     X(plan_destroy_internal)(cldcpy);
     return (plan *)0;
}

static solver *mksolver(const ndrct_adt *adt)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->adt = adt;
     return &(slv->super);
}

void X(rdft_indirect_register)(planner *p)
{
     unsigned i;
     static const ndrct_adt *const adts[] = {
	  &adt_before, &adt_after
     };

     for (i = 0; i < sizeof(adts) / sizeof(adts[0]); ++i)
          REGISTER_SOLVER(p, mksolver(adts[i]));
}
