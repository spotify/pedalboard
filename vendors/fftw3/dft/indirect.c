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



/* solvers/plans for vectors of small DFT's that cannot be done
   in-place directly.  Use a rank-0 plan to rearrange the data
   before or after the transform.  Can also change an out-of-place
   plan into a copy + in-place (where the in-place transform
   is e.g. unit stride). */

/* FIXME: merge with rank-geq2.c(?), since this is just a special case
   of a rank split where the first/second transform has rank 0. */

#include "dft/dft.h"

typedef problem *(*mkcld_t) (const problem_dft *p);

typedef struct {
     dftapply apply;
     problem *(*mkcld)(const problem_dft *p);
     const char *nam;
} ndrct_adt;

typedef struct {
     solver super;
     const ndrct_adt *adt;
} S;

typedef struct {
     plan_dft super;
     plan *cldcpy, *cld;
     const S *slv;
} P;

/*-----------------------------------------------------------------------*/
/* first rearrange, then transform */
static void apply_before(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;

     {
          plan_dft *cldcpy = (plan_dft *) ego->cldcpy;
          cldcpy->apply(ego->cldcpy, ri, ii, ro, io);
     }
     {
          plan_dft *cld = (plan_dft *) ego->cld;
          cld->apply(ego->cld, ro, io, ro, io);
     }
}

static problem *mkcld_before(const problem_dft *p)
{
     return X(mkproblem_dft_d)(X(tensor_copy_inplace)(p->sz, INPLACE_OS),
			       X(tensor_copy_inplace)(p->vecsz, INPLACE_OS),
			       p->ro, p->io, p->ro, p->io);
}

static const ndrct_adt adt_before =
{
     apply_before, mkcld_before, "dft-indirect-before"
};

/*-----------------------------------------------------------------------*/
/* first transform, then rearrange */

static void apply_after(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;

     {
          plan_dft *cld = (plan_dft *) ego->cld;
          cld->apply(ego->cld, ri, ii, ri, ii);
     }
     {
          plan_dft *cldcpy = (plan_dft *) ego->cldcpy;
          cldcpy->apply(ego->cldcpy, ri, ii, ro, io);
     }
}

static problem *mkcld_after(const problem_dft *p)
{
     return X(mkproblem_dft_d)(X(tensor_copy_inplace)(p->sz, INPLACE_IS),
			       X(tensor_copy_inplace)(p->vecsz, INPLACE_IS),
			       p->ri, p->ii, p->ri, p->ii);
}

static const ndrct_adt adt_after =
{
     apply_after, mkcld_after, "dft-indirect-after"
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
     const problem_dft *p = (const problem_dft *) p_;
     return (1
	     && FINITE_RNK(p->vecsz->rnk)

	     /* problem must be a nontrivial transform, not just a copy */
	     && p->sz->rnk > 0

	     && (0

		 /* problem must be in-place & require some
		    rearrangement of the data; to prevent
		    infinite loops with indirect-transpose, we
		    further require that at least some transform
		    strides must decrease */
		 || (p->ri == p->ro
		     && !X(tensor_inplace_strides2)(p->sz, p->vecsz)
		     && X(tensor_strides_decrease)(
			  p->sz, p->vecsz,
			  ego->adt->apply == apply_after ? 
			  INPLACE_IS : INPLACE_OS))

		 /* or problem must be out of place, transforming
		    from stride 1/2 to bigger stride, for apply_after */
		 || (p->ri != p->ro && ego->adt->apply == apply_after
		     && !NO_DESTROY_INPUTP(plnr)
		     && X(tensor_min_istride)(p->sz) <= 2
		     && X(tensor_min_ostride)(p->sz) > 2)
			  
		 /* or problem must be out of place, transforming
		    to stride 1/2 from bigger stride, for apply_before */
		 || (p->ri != p->ro && ego->adt->apply == apply_before
		     && X(tensor_min_ostride)(p->sz) <= 2
		     && X(tensor_min_istride)(p->sz) > 2)
		  )
	  );
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr)
{
     if (!applicable0(ego_, p_, plnr)) return 0;
     {
          const problem_dft *p = (const problem_dft *) p_;
	  if (NO_INDIRECT_OP_P(plnr) && p->ri != p->ro) return 0;
     }
     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     const S *ego = (const S *) ego_;
     P *pln;
     plan *cld = 0, *cldcpy = 0;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *) 0;

     cldcpy =
	  X(mkplan_d)(plnr, 
		      X(mkproblem_dft_d)(X(mktensor_0d)(),
					 X(tensor_append)(p->vecsz, p->sz),
					 p->ri, p->ii, p->ro, p->io));

     if (!cldcpy) goto nada;

     cld = X(mkplan_f_d)(plnr, ego->adt->mkcld(p), NO_BUFFERING, 0, 0);
     if (!cld) goto nada;

     pln = MKPLAN_DFT(P, &padt, ego->adt->apply);
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
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->adt = adt;
     return &(slv->super);
}

void X(dft_indirect_register)(planner *p)
{
     unsigned i;
     static const ndrct_adt *const adts[] = {
	  &adt_before, &adt_after
     };

     for (i = 0; i < sizeof(adts) / sizeof(adts[0]); ++i)
          REGISTER_SOLVER(p, mksolver(adts[i]));
}
