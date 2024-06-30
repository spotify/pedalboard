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

/* Complex DFTs of rank == 1 when the vector length vn is >= # processes.
   In this case, we don't need to use a six-step type algorithm, and can
   instead transpose the DFT dimension with the vector dimension to 
   make the DFT local. */

#include "mpi-dft.h"
#include "mpi-transpose.h"
#include "dft/dft.h"

typedef struct {
     solver super;
     int preserve_input; /* preserve input even if DESTROY_INPUT was passed */
     rearrangement rearrange;
} S;

typedef struct {
     plan_mpi_dft super;

     plan *cldt_before, *cld, *cldt_after;
     INT roff, ioff;
     int preserve_input;
     rearrangement rearrange;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;
     plan_rdft *cldt_before, *cldt_after;
     INT roff = ego->roff, ioff = ego->ioff;
     
     /* global transpose */
     cldt_before = (plan_rdft *) ego->cldt_before;
     cldt_before->apply(ego->cldt_before, I, O);
     
     if (ego->preserve_input) I = O;
	  
     /* 1d DFT(s) */
     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, O+roff, O+ioff, I+roff, I+ioff);
     
     /* global transpose */
     cldt_after = (plan_rdft *) ego->cldt_after;
     cldt_after->apply(ego->cldt_after, I, O);
}

static int applicable(const S *ego, const problem *p_,
		      const planner *plnr)
{
     const problem_mpi_dft *p = (const problem_mpi_dft *) p_;
     int n_pes;
     MPI_Comm_size(p->comm, &n_pes);
     return (1
	     && p->sz->rnk == 1
	     && !(p->flags & ~RANK1_BIGVEC_ONLY)
	     && (!ego->preserve_input || (!NO_DESTROY_INPUTP(plnr)
					  && p->I != p->O))
	     && (p->vn >= n_pes /* TODO: relax this, using more memory? */
		 || (p->flags & RANK1_BIGVEC_ONLY))

	     && XM(rearrange_applicable)(ego->rearrange,
					 p->sz->dims[0], p->vn, n_pes)

	     && (!NO_SLOWP(plnr) /* slow if dft-serial is applicable */
                 || !XM(dft_serial_applicable)(p))
	  );
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cldt_before, wakefulness);
     X(plan_awake)(ego->cld, wakefulness);
     X(plan_awake)(ego->cldt_after, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldt_after);
     X(plan_destroy_internal)(ego->cld);
     X(plan_destroy_internal)(ego->cldt_before);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const char descrip[][16] = { "contig", "discontig", "square-after",
				  "square-middle", "square-before" };
     p->print(p, "(mpi-dft-rank1-bigvec/%s%s %(%p%) %(%p%) %(%p%))",
	      descrip[ego->rearrange], ego->preserve_input==2 ?"/p":"",
	      ego->cldt_before, ego->cld, ego->cldt_after);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_mpi_dft *p;
     P *pln;
     plan *cld = 0, *cldt_before = 0, *cldt_after = 0;
     R *ri, *ii, *ro, *io, *I, *O;
     INT yblock, yb, nx, ny, vn;
     int my_pe, n_pes;
     static const plan_adt padt = {
          XM(dft_solve), awake, print, destroy
     };

     UNUSED(ego);

     if (!applicable(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_mpi_dft *) p_;

     MPI_Comm_rank(p->comm, &my_pe);
     MPI_Comm_size(p->comm, &n_pes);
     
     nx = p->sz->dims[0].n;
     if (!(ny = XM(rearrange_ny)(ego->rearrange, p->sz->dims[0],p->vn,n_pes)))
	  return (plan *) 0;
     vn = p->vn / ny;
     A(ny * vn == p->vn);

     yblock = XM(default_block)(ny, n_pes);
     cldt_before = X(mkplan_d)(plnr,
			       XM(mkproblem_transpose)(
				    nx, ny, vn*2,
				    I = p->I, O = p->O,
				    p->sz->dims[0].b[IB], yblock,
				    p->comm, 0));
     if (XM(any_true)(!cldt_before, p->comm)) goto nada;	  
     if (ego->preserve_input || NO_DESTROY_INPUTP(plnr)) { I = O; }
     
     X(extract_reim)(p->sign, I, &ri, &ii);
     X(extract_reim)(p->sign, O, &ro, &io);

     yb = XM(block)(ny, yblock, my_pe);
     cld = X(mkplan_d)(plnr,
		       X(mkproblem_dft_d)(X(mktensor_1d)(nx, vn*2, vn*2),
					  X(mktensor_2d)(yb, vn*2*nx, vn*2*nx,
							 vn, 2, 2),
					  ro, io, ri, ii));
     if (XM(any_true)(!cld, p->comm)) goto nada;	  
     
     cldt_after = X(mkplan_d)(plnr,
			      XM(mkproblem_transpose)(
				   ny, nx, vn*2,
				   I, O,
				   yblock, p->sz->dims[0].b[OB], 
				   p->comm, 0));
     if (XM(any_true)(!cldt_after, p->comm)) goto nada;	  

     pln = MKPLAN_MPI_DFT(P, &padt, apply);

     pln->cldt_before = cldt_before;
     pln->cld = cld;
     pln->cldt_after = cldt_after;
     pln->preserve_input = ego->preserve_input ? 2 : NO_DESTROY_INPUTP(plnr);
     pln->roff = ro - p->O;
     pln->ioff = io - p->O;
     pln->rearrange = ego->rearrange;

     X(ops_add)(&cldt_before->ops, &cld->ops, &pln->super.super.ops);
     X(ops_add2)(&cldt_after->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cldt_after);
     X(plan_destroy_internal)(cld);
     X(plan_destroy_internal)(cldt_before);
     return (plan *) 0;
}

static solver *mksolver(rearrangement rearrange, int preserve_input)
{
     static const solver_adt sadt = { PROBLEM_MPI_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->rearrange = rearrange;
     slv->preserve_input = preserve_input;
     return &(slv->super);
}

void XM(dft_rank1_bigvec_register)(planner *p)
{
     rearrangement rearrange;
     int preserve_input;
     FORALL_REARRANGE(rearrange)
	  for (preserve_input = 0; preserve_input <= 1; ++preserve_input)
	       REGISTER_SOLVER(p, mksolver(rearrange, preserve_input));
}
