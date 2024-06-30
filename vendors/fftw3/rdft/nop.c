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


/* plans for vrank -infty RDFTs (nothing to do) */

#include "rdft/rdft.h"

static void apply(const plan *ego_, R *I, R *O)
{
     UNUSED(ego_);
     UNUSED(I);
     UNUSED(O);
}

static int applicable(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);
     return 0
	  /* case 1 : -infty vector rank */
	  || (p->vecsz->rnk == RNK_MINFTY)

	  /* case 2 : rank-0 in-place rdft */
	  || (1
	      && p->sz->rnk == 0
	      && FINITE_RNK(p->vecsz->rnk)
	      && p->O == p->I
	      && X(tensor_inplace_strides)(p->vecsz)
	       );
}

static void print(const plan *ego, printer *p)
{
     UNUSED(ego);
     p->print(p, "(rdft-nop)");
}

static plan *mkplan(const solver *ego, const problem *p, planner *plnr)
{
     static const plan_adt padt = {
	  X(rdft_solve), X(null_awake), print, X(plan_null_destroy)
     };
     plan_rdft *pln;

     UNUSED(plnr);

     if (!applicable(ego, p))
          return (plan *) 0;
     pln = MKPLAN_RDFT(plan_rdft, &padt, apply);
     X(ops_zero)(&pln->super.ops);

     return &(pln->super);
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     return MKSOLVER(solver, &sadt);
}

void X(rdft_nop_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
