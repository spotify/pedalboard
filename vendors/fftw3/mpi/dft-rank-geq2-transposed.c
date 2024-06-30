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

/* Complex DFTs of rank >= 2, for the case where we are distributed
   across the first dimension only, and the output is transposed both
   in data distribution and in ordering (for the first 2 dimensions).

   (Note that we don't have to handle the case where the input is
   transposed, since this is equivalent to transposed output with the
   first two dimensions swapped, and is automatically canonicalized as
   such by dft-problem.c. */

#include "mpi-dft.h"
#include "mpi-transpose.h"
#include "dft/dft.h"

typedef struct {
     solver super;
     int preserve_input; /* preserve input even if DESTROY_INPUT was passed */
} S;

typedef struct {
     plan_mpi_dft super;

     plan *cld1, *cldt, *cld2;
     INT roff, ioff;
     int preserve_input;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld1, *cld2;
     plan_rdft *cldt;
     INT roff = ego->roff, ioff = ego->ioff;
     
     /* DFT local dimensions */
     cld1 = (plan_dft *) ego->cld1;
     if (ego->preserve_input) {
	  cld1->apply(ego->cld1, I+roff, I+ioff, O+roff, O+ioff);
	  I = O;
     }
     else
	  cld1->apply(ego->cld1, I+roff, I+ioff, I+roff, I+ioff);

     /* global transpose */
     cldt = (plan_rdft *) ego->cldt;
     cldt->apply(ego->cldt, I, O);

     /* DFT final local dimension */
     cld2 = (plan_dft *) ego->cld2;
     cld2->apply(ego->cld2, O+roff, O+ioff, O+roff, O+ioff);
}

static int applicable(const S *ego, const problem *p_,
		      const planner *plnr)
{
     const problem_mpi_dft *p = (const problem_mpi_dft *) p_;
     return (1
	     && p->sz->rnk > 1
	     && p->flags == TRANSPOSED_OUT
	     && (!ego->preserve_input || (!NO_DESTROY_INPUTP(plnr)
					  && p->I != p->O))
	     && XM(is_local_after)(1, p->sz, IB)
	     && XM(is_local_after)(2, p->sz, OB)
	     && XM(num_blocks)(p->sz->dims[0].n, p->sz->dims[0].b[OB]) == 1
	     && (!NO_SLOWP(plnr) /* slow if dft-serial is applicable */
		 || !XM(dft_serial_applicable)(p))
	  );
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld1, wakefulness);
     X(plan_awake)(ego->cldt, wakefulness);
     X(plan_awake)(ego->cld2, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld2);
     X(plan_destroy_internal)(ego->cldt);
     X(plan_destroy_internal)(ego->cld1);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(mpi-dft-rank-geq2-transposed%s%(%p%)%(%p%)%(%p%))", 
	      ego->preserve_input==2 ?"/p":"",
	      ego->cld1, ego->cldt, ego->cld2);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_mpi_dft *p;
     P *pln;
     plan *cld1 = 0, *cldt = 0, *cld2 = 0;
     R *ri, *ii, *ro, *io, *I, *O;
     tensor *sz;
     int i, my_pe, n_pes;
     INT nrest;
     static const plan_adt padt = {
          XM(dft_solve), awake, print, destroy
     };

     UNUSED(ego);

     if (!applicable(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_mpi_dft *) p_;

     X(extract_reim)(p->sign, I = p->I, &ri, &ii);
     X(extract_reim)(p->sign, O = p->O, &ro, &io);
     if (ego->preserve_input || NO_DESTROY_INPUTP(plnr)) 
	  I = O; 
     else { 
	  ro = ri;
	  io = ii;
     }
     MPI_Comm_rank(p->comm, &my_pe);
     MPI_Comm_size(p->comm, &n_pes);

     sz = X(mktensor)(p->sz->rnk - 1); /* tensor of last rnk-1 dimensions */
     i = p->sz->rnk - 2; A(i >= 0);
     sz->dims[i].n = p->sz->dims[i+1].n;
     sz->dims[i].is = sz->dims[i].os = 2 * p->vn;
     for (--i; i >= 0; --i) {
	  sz->dims[i].n = p->sz->dims[i+1].n;
	  sz->dims[i].is = sz->dims[i].os = sz->dims[i+1].n * sz->dims[i+1].is;
     }
     nrest = 1; for (i = 1; i < sz->rnk; ++i) nrest *= sz->dims[i].n;
     {
          INT is = sz->dims[0].n * sz->dims[0].is;
          INT b = XM(block)(p->sz->dims[0].n, p->sz->dims[0].b[IB], my_pe);
	  cld1 = X(mkplan_d)(plnr,
                             X(mkproblem_dft_d)(sz,
                                                X(mktensor_2d)(b, is, is,
                                                               p->vn, 2, 2),
                                                ri, ii, ro, io));
	  if (XM(any_true)(!cld1, p->comm)) goto nada;
     }

     nrest *= p->vn;
     cldt = X(mkplan_d)(plnr,
			XM(mkproblem_transpose)(
			     p->sz->dims[0].n, p->sz->dims[1].n, nrest * 2,
			     I, O,
			     p->sz->dims[0].b[IB], p->sz->dims[1].b[OB], 
			     p->comm, 0));
     if (XM(any_true)(!cldt, p->comm)) goto nada;

     X(extract_reim)(p->sign, O, &ro, &io);
     {
	  INT is = p->sz->dims[0].n * nrest * 2;
	  INT b = XM(block)(p->sz->dims[1].n, p->sz->dims[1].b[OB], my_pe);
	  cld2 = X(mkplan_d)(plnr,
			     X(mkproblem_dft_d)(X(mktensor_1d)(
						     p->sz->dims[0].n,
						     nrest * 2, nrest * 2),
						X(mktensor_2d)(b, is, is,
							       nrest, 2, 2),
						ro, io, ro, io));
	  if (XM(any_true)(!cld2, p->comm)) goto nada;
     }

     pln = MKPLAN_MPI_DFT(P, &padt, apply);
     pln->cld1 = cld1;
     pln->cldt = cldt;
     pln->cld2 = cld2;
     pln->preserve_input = ego->preserve_input ? 2 : NO_DESTROY_INPUTP(plnr);
     pln->roff = ri - p->I;
     pln->ioff = ii - p->I;

     X(ops_add)(&cld1->ops, &cld2->ops, &pln->super.super.ops);
     X(ops_add2)(&cldt->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld2);
     X(plan_destroy_internal)(cldt);
     X(plan_destroy_internal)(cld1);
     return (plan *) 0;
}

static solver *mksolver(int preserve_input)
{
     static const solver_adt sadt = { PROBLEM_MPI_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->preserve_input = preserve_input;
     return &(slv->super);
}

void XM(dft_rank_geq2_transposed_register)(planner *p)
{
     int preserve_input;
     for (preserve_input = 0; preserve_input <= 1; ++preserve_input)
	  REGISTER_SOLVER(p, mksolver(preserve_input));
}
