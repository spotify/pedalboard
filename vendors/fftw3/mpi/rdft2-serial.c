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

/* "MPI" DFTs where all of the data is on one processor...just
   call through to serial API. */

#include "mpi-rdft2.h"
#include "rdft/rdft.h"

typedef struct {
     plan_mpi_rdft2 super;
     plan *cld;
     INT vn;
} P;

static void apply_r2c(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld;
     cld = (plan_rdft2 *) ego->cld;
     cld->apply(ego->cld, I, I+ego->vn, O, O+1);
}

static void apply_c2r(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld;
     cld = (plan_rdft2 *) ego->cld;
     cld->apply(ego->cld, O, O+ego->vn, I, I+1);
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
     p->print(p, "(mpi-rdft2-serial %(%p%))", ego->cld);
}

int XM(rdft2_serial_applicable)(const problem_mpi_rdft2 *p)
{
     return (1
	     && p->flags == 0 /* TRANSPOSED/SCRAMBLED_IN/OUT not supported */
	     && ((XM(is_local)(p->sz, IB) && XM(is_local)(p->sz, OB))
		 || p->vn == 0));
}

static plan *mkplan(const solver *ego, const problem *p_, planner *plnr)
{
     const problem_mpi_rdft2 *p = (const problem_mpi_rdft2 *) p_;
     P *pln;
     plan *cld;
     int my_pe;
     R *r0, *r1, *cr, *ci;
     static const plan_adt padt = {
          XM(rdft2_solve), awake, print, destroy
     };

     UNUSED(ego);

     /* check whether applicable: */
     if (!XM(rdft2_serial_applicable)(p))
          return (plan *) 0;

     if (p->kind == R2HC) {
	  r1 = (r0 = p->I) + p->vn;
	  ci = (cr = p->O) + 1;
     }
     else {
	  r1 = (r0 = p->O) + p->vn;
	  ci = (cr = p->I) + 1;
     }

     MPI_Comm_rank(p->comm, &my_pe);
     if (my_pe == 0 && p->vn > 0) {
	  INT ivs = 1 + (p->kind == HC2R), ovs = 1 + (p->kind == R2HC);
	  int i, rnk = p->sz->rnk;
	  tensor *sz = X(mktensor)(p->sz->rnk);
	  sz->dims[rnk - 1].is = sz->dims[rnk - 1].os = 2 * p->vn;
	  sz->dims[rnk - 1].n = p->sz->dims[rnk - 1].n / 2 + 1;
	  for (i = rnk - 1; i > 0; --i) {
	       sz->dims[i - 1].is = sz->dims[i - 1].os = 
		    sz->dims[i].is * sz->dims[i].n;
	       sz->dims[i - 1].n = p->sz->dims[i - 1].n;
	  }
	  sz->dims[rnk - 1].n = p->sz->dims[rnk - 1].n;

	  cld = X(mkplan_d)(plnr,
			    X(mkproblem_rdft2_d)(sz,
						 X(mktensor_1d)(p->vn,ivs,ovs),
						 r0, r1, cr, ci, p->kind));
     }
     else { /* idle process: make nop plan */
	  cld = X(mkplan_d)(plnr,
			    X(mkproblem_rdft2_d)(X(mktensor_0d)(),
						 X(mktensor_1d)(0,0,0),
						 cr, ci, cr, ci, HC2R));
     }
     if (XM(any_true)(!cld, p->comm)) return (plan *) 0;

     pln = MKPLAN_MPI_RDFT2(P, &padt, p->kind == R2HC ? apply_r2c : apply_c2r);
     pln->cld = cld;
     pln->vn = p->vn;
     X(ops_cpy)(&cld->ops, &pln->super.super.ops);
     return &(pln->super.super);
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_MPI_RDFT2, mkplan, 0 };
     return MKSOLVER(solver, &sadt);
}

void XM(rdft2_serial_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
