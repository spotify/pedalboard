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

/* solvers/plans for vectors of DFTs corresponding to the columns
   of a matrix: first transpose the matrix so that the DFTs are
   contiguous, then do DFTs with transposed output.   In particular,
   we restrict ourselves to the case of a square transpose (or a
   sequence thereof). */

#include "dft/dft.h"

typedef solver S;

typedef struct {
     plan_dft super;
     INT vl, ivs, ovs;
     plan *cldtrans, *cld, *cldrest;
} P;

/* initial transpose is out-of-place from input to output */
static void apply_op(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT vl = ego->vl, ivs = ego->ivs, ovs = ego->ovs, i;

     for (i = 0; i < vl; ++i) {
	  {
	       plan_dft *cldtrans = (plan_dft *) ego->cldtrans;
	       cldtrans->apply(ego->cldtrans, ri, ii, ro, io);
	  }
	  {
	       plan_dft *cld = (plan_dft *) ego->cld;
	       cld->apply(ego->cld, ro, io, ro, io);
	  }
	  ri += ivs; ii += ivs;
	  ro += ovs; io += ovs;
     }
     {
	  plan_dft *cldrest = (plan_dft *) ego->cldrest;
	  cldrest->apply(ego->cldrest, ri, ii, ro, io);
     }
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldrest);
     X(plan_destroy_internal)(ego->cld);
     X(plan_destroy_internal)(ego->cldtrans);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cldtrans, wakefulness);
     X(plan_awake)(ego->cld, wakefulness);
     X(plan_awake)(ego->cldrest, wakefulness);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(indirect-transpose%v%(%p%)%(%p%)%(%p%))", 
	      ego->vl, ego->cldtrans, ego->cld, ego->cldrest);
}

static int pickdim(const tensor *vs, const tensor *s, int *pdim0, int *pdim1)
{
     int dim0, dim1;
     *pdim0 = *pdim1 = -1;
     for (dim0 = 0; dim0 < vs->rnk; ++dim0)
          for (dim1 = 0; dim1 < s->rnk; ++dim1) 
	       if (vs->dims[dim0].n * X(iabs)(vs->dims[dim0].is) <= X(iabs)(s->dims[dim1].is)
		   && vs->dims[dim0].n >= s->dims[dim1].n
		   && (*pdim0 == -1 
		       || (X(iabs)(vs->dims[dim0].is) <= X(iabs)(vs->dims[*pdim0].is)
			   && X(iabs)(s->dims[dim1].is) >= X(iabs)(s->dims[*pdim1].is)))) {
		    *pdim0 = dim0;
		    *pdim1 = dim1;
	       }
     return (*pdim0 != -1 && *pdim1 != -1);
}

static int applicable0(const solver *ego_, const problem *p_,
		       const planner *plnr,
		       int *pdim0, int *pdim1)
{
     const problem_dft *p = (const problem_dft *) p_;
     UNUSED(ego_); UNUSED(plnr);

     return (1
	     && FINITE_RNK(p->vecsz->rnk) && FINITE_RNK(p->sz->rnk)

	     /* FIXME: can/should we relax this constraint? */
	     && X(tensor_inplace_strides2)(p->vecsz, p->sz)

	     && pickdim(p->vecsz, p->sz, pdim0, pdim1)

	     /* output should not *already* include the transpose
		(in which case we duplicate the regular indirect.c) */
	     && (p->sz->dims[*pdim1].os != p->vecsz->dims[*pdim0].is)
	  );
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr,
		      int *pdim0, int *pdim1)
{
     if (!applicable0(ego_, p_, plnr, pdim0, pdim1)) return 0;
     {
          const problem_dft *p = (const problem_dft *) p_;
	  INT u = p->ri == p->ii + 1 || p->ii == p->ri + 1 ? (INT)2 : (INT)1;

	  /* UGLY if does not result in contiguous transforms or
	     transforms of contiguous vectors (since the latter at
	     least have efficient transpositions) */
	  if (NO_UGLYP(plnr)
	      && p->vecsz->dims[*pdim0].is != u
	      && !(p->vecsz->rnk == 2
		   && p->vecsz->dims[1-*pdim0].is == u
		   && p->vecsz->dims[*pdim0].is
		      == u * p->vecsz->dims[1-*pdim0].n))
	       return 0;

	  if (NO_INDIRECT_OP_P(plnr) && p->ri != p->ro) return 0;
     }
     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     P *pln;
     plan *cld = 0, *cldtrans = 0, *cldrest = 0;
     int pdim0, pdim1;
     tensor *ts, *tv;
     INT vl, ivs, ovs;
     R *rit, *iit, *rot, *iot;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr, &pdim0, &pdim1))
          return (plan *) 0;

     vl = p->vecsz->dims[pdim0].n / p->sz->dims[pdim1].n;
     A(vl >= 1);
     ivs = p->sz->dims[pdim1].n * p->vecsz->dims[pdim0].is;
     ovs = p->sz->dims[pdim1].n * p->vecsz->dims[pdim0].os;
     rit = TAINT(p->ri, vl == 1 ? 0 : ivs);
     iit = TAINT(p->ii, vl == 1 ? 0 : ivs);
     rot = TAINT(p->ro, vl == 1 ? 0 : ovs);
     iot = TAINT(p->io, vl == 1 ? 0 : ovs);

     ts = X(tensor_copy_inplace)(p->sz, INPLACE_IS);
     ts->dims[pdim1].os = p->vecsz->dims[pdim0].is;
     tv = X(tensor_copy_inplace)(p->vecsz, INPLACE_IS);
     tv->dims[pdim0].os = p->sz->dims[pdim1].is;
     tv->dims[pdim0].n = p->sz->dims[pdim1].n;
     cldtrans = X(mkplan_d)(plnr, 
			    X(mkproblem_dft_d)(X(mktensor_0d)(),
					       X(tensor_append)(tv, ts),
					       rit, iit, 
					       rot, iot));
     X(tensor_destroy2)(ts, tv);
     if (!cldtrans) goto nada;

     ts = X(tensor_copy)(p->sz);
     ts->dims[pdim1].is = p->vecsz->dims[pdim0].is;
     tv = X(tensor_copy)(p->vecsz);
     tv->dims[pdim0].is = p->sz->dims[pdim1].is;
     tv->dims[pdim0].n = p->sz->dims[pdim1].n;
     cld = X(mkplan_d)(plnr, X(mkproblem_dft_d)(ts, tv,
						rot, iot,
						rot, iot));
     if (!cld) goto nada;

     tv = X(tensor_copy)(p->vecsz);
     tv->dims[pdim0].n -= vl * p->sz->dims[pdim1].n;
     cldrest = X(mkplan_d)(plnr, X(mkproblem_dft_d)(X(tensor_copy)(p->sz), tv,
						    p->ri + ivs * vl,
						    p->ii + ivs * vl,
						    p->ro + ovs * vl,
						    p->io + ovs * vl));
     if (!cldrest) goto nada;

     pln = MKPLAN_DFT(P, &padt, apply_op);
     pln->cldtrans = cldtrans;
     pln->cld = cld;
     pln->cldrest = cldrest;
     pln->vl = vl;
     pln->ivs = ivs;
     pln->ovs = ovs;
     X(ops_cpy)(&cldrest->ops, &pln->super.super.ops);
     X(ops_madd2)(vl, &cld->ops, &pln->super.super.ops);
     X(ops_madd2)(vl, &cldtrans->ops, &pln->super.super.ops);
     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cldrest);
     X(plan_destroy_internal)(cld);
     X(plan_destroy_internal)(cldtrans);
     return (plan *)0;
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return slv;
}

void X(dft_indirect_transpose_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
