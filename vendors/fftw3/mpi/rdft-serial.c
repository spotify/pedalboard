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

/* "MPI" RDFTs where all of the data is on one processor...just
   call through to serial API. */

#include "mpi-rdft.h"

typedef struct {
     plan_mpi_rdft super;
     plan *cld;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, I, O);
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
     p->print(p, "(mpi-rdft-serial %(%p%))", ego->cld);
}

int XM(rdft_serial_applicable)(const problem_mpi_rdft *p)
{
     return (1
	     && p->flags == 0 /* TRANSPOSED/SCRAMBLED_IN/OUT not supported */
	     && ((XM(is_local)(p->sz, IB) && XM(is_local)(p->sz, OB))
		 || p->vn == 0));
}

static plan *mkplan(const solver *ego, const problem *p_, planner *plnr)
{
     const problem_mpi_rdft *p = (const problem_mpi_rdft *) p_;
     P *pln;
     plan *cld;
     int my_pe;
     static const plan_adt padt = {
          XM(rdft_solve), awake, print, destroy
     };

     UNUSED(ego);

     /* check whether applicable: */
     if (!XM(rdft_serial_applicable)(p))
          return (plan *) 0;

     MPI_Comm_rank(p->comm, &my_pe);
     if (my_pe == 0 && p->vn > 0) {
	  int i, rnk = p->sz->rnk;
	  tensor *sz = X(mktensor)(rnk);
	  rdft_kind *kind 
	       = (rdft_kind *) MALLOC(sizeof(rdft_kind) * rnk, PROBLEMS);
	  sz->dims[rnk - 1].is = sz->dims[rnk - 1].os = p->vn;
	  sz->dims[rnk - 1].n = p->sz->dims[rnk - 1].n;
	  for (i = rnk - 1; i > 0; --i) {
	       sz->dims[i - 1].is = sz->dims[i - 1].os = 
		    sz->dims[i].is * sz->dims[i].n;
	       sz->dims[i - 1].n = p->sz->dims[i - 1].n;
	  }
	  for (i = 0; i < rnk; ++i)
	       kind[i] = p->kind[i];
	  
	  cld = X(mkplan_d)(plnr,
			    X(mkproblem_rdft_d)(sz,
						X(mktensor_1d)(p->vn, 1, 1),
						p->I, p->O, kind));
	  X(ifree0)(kind);
     }
     else { /* idle process: make nop plan */
	  cld = X(mkplan_d)(plnr,
			    X(mkproblem_rdft_0_d)(X(mktensor_1d)(0,0,0),
						  p->I, p->O));
     }
     if (XM(any_true)(!cld, p->comm)) return (plan *) 0;

     pln = MKPLAN_MPI_RDFT(P, &padt, apply);
     pln->cld = cld;
     X(ops_cpy)(&cld->ops, &pln->super.super.ops);
     return &(pln->super.super);
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_MPI_RDFT, mkplan, 0 };
     return MKSOLVER(solver, &sadt);
}

void XM(rdft_serial_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
