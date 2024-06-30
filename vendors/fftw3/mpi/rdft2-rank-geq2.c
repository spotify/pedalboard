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

/* Complex RDFT2s of rank >= 2, for the case where we are distributed
   across the first dimension only, and the output is not transposed. */

#include "mpi-dft.h"
#include "mpi-rdft2.h"
#include "rdft/rdft.h"

typedef struct {
     solver super;
     int preserve_input; /* preserve input even if DESTROY_INPUT was passed */
} S;

typedef struct {
     plan_mpi_rdft2 super;

     plan *cld1, *cld2;
     INT vn;
     int preserve_input;
} P;

static void apply_r2c(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld1;
     plan_rdft *cld2;
     
     /* RDFT2 local dimensions */
     cld1 = (plan_rdft2 *) ego->cld1;
     if (ego->preserve_input) {
	  cld1->apply(ego->cld1, I, I+ego->vn, O, O+1);
	  I = O;
     }
     else
	  cld1->apply(ego->cld1, I, I+ego->vn, I, I+1);

     /* DFT non-local dimension (via dft-rank1-bigvec, usually): */
     cld2 = (plan_rdft *) ego->cld2;
     cld2->apply(ego->cld2, I, O);
}

static void apply_c2r(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld1;
     plan_rdft *cld2;
     
     /* DFT non-local dimension (via dft-rank1-bigvec, usually): */
     cld2 = (plan_rdft *) ego->cld2;
     cld2->apply(ego->cld2, I, O);

     /* RDFT2 local dimensions */
     cld1 = (plan_rdft2 *) ego->cld1;
     cld1->apply(ego->cld1, O, O+ego->vn, O, O+1);

}

static int applicable(const S *ego, const problem *p_,
		      const planner *plnr)
{
     const problem_mpi_rdft2 *p = (const problem_mpi_rdft2 *) p_;
     return (1
	     && p->sz->rnk > 1
	     && p->flags == 0 /* TRANSPOSED/SCRAMBLED_IN/OUT not supported */
	     && (!ego->preserve_input || (!NO_DESTROY_INPUTP(plnr)
					  && p->I != p->O
					  && p->kind == R2HC))
	     && XM(is_local_after)(1, p->sz, IB)
	     && XM(is_local_after)(1, p->sz, OB)
	     && (!NO_SLOWP(plnr) /* slow if rdft2-serial is applicable */
		 || !XM(rdft2_serial_applicable)(p))
	  );
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
     p->print(p, "(mpi-rdft2-rank-geq2%s%(%p%)%(%p%))", 
	      ego->preserve_input==2 ?"/p":"", ego->cld1, ego->cld2);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_mpi_rdft2 *p;
     P *pln;
     plan *cld1 = 0, *cld2 = 0;
     R *r0, *r1, *cr, *ci, *I, *O;
     tensor *sz;
     dtensor *sz2;
     int i, my_pe, n_pes;
     INT nrest;
     static const plan_adt padt = {
          XM(rdft2_solve), awake, print, destroy
     };

     UNUSED(ego);

     if (!applicable(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_mpi_rdft2 *) p_;

     I = p->I; O = p->O;
     if (p->kind == R2HC) {
          r1 = (r0 = p->I) + p->vn;
	  if (ego->preserve_input || NO_DESTROY_INPUTP(plnr)) {
	       ci = (cr = p->O) + 1;
	       I = O; 
	  }
	  else 
	       ci = (cr = p->I) + 1;
     }
     else {
          r1 = (r0 = p->O) + p->vn;
          ci = (cr = p->O) + 1;
     }

     MPI_Comm_rank(p->comm, &my_pe);
     MPI_Comm_size(p->comm, &n_pes);

     sz = X(mktensor)(p->sz->rnk - 1); /* tensor of last rnk-1 dimensions */
     i = p->sz->rnk - 2; A(i >= 0);
     sz->dims[i].is = sz->dims[i].os = 2 * p->vn;
     sz->dims[i].n = p->sz->dims[i+1].n / 2 + 1;
     for (--i; i >= 0; --i) {
	  sz->dims[i].n = p->sz->dims[i+1].n;
	  sz->dims[i].is = sz->dims[i].os = sz->dims[i+1].n * sz->dims[i+1].is;
     }
     nrest = X(tensor_sz)(sz);
     {
	  INT ivs = 1 + (p->kind == HC2R), ovs = 1 + (p->kind == R2HC);
          INT is = sz->dims[0].n * sz->dims[0].is;
          INT b = XM(block)(p->sz->dims[0].n, p->sz->dims[0].b[IB], my_pe);
	  sz->dims[p->sz->rnk - 2].n = p->sz->dims[p->sz->rnk - 1].n;
	  cld1 = X(mkplan_d)(plnr,
                             X(mkproblem_rdft2_d)(sz,
						  X(mktensor_2d)(b, is, is,
							        p->vn,ivs,ovs),
						  r0, r1, cr, ci, p->kind));
	  if (XM(any_true)(!cld1, p->comm)) goto nada;
     }

     sz2 = XM(mkdtensor)(1); /* tensor for first (distributed) dimension */
     sz2->dims[0] = p->sz->dims[0];
     cld2 = X(mkplan_d)(plnr, XM(mkproblem_dft_d)(sz2, nrest * p->vn,
						  I, O, p->comm, 
						  p->kind == R2HC ?
						  FFT_SIGN : -FFT_SIGN,
						  RANK1_BIGVEC_ONLY));
     if (XM(any_true)(!cld2, p->comm)) goto nada;

     pln = MKPLAN_MPI_RDFT2(P, &padt, p->kind == R2HC ? apply_r2c : apply_c2r);
     pln->cld1 = cld1;
     pln->cld2 = cld2;
     pln->preserve_input = ego->preserve_input ? 2 : NO_DESTROY_INPUTP(plnr);
     pln->vn = p->vn;

     X(ops_add)(&cld1->ops, &cld2->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld2);
     X(plan_destroy_internal)(cld1);
     return (plan *) 0;
}

static solver *mksolver(int preserve_input)
{
     static const solver_adt sadt = { PROBLEM_MPI_RDFT2, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->preserve_input = preserve_input;
     return &(slv->super);
}

void XM(rdft2_rank_geq2_register)(planner *p)
{
     int preserve_input;
     for (preserve_input = 0; preserve_input <= 1; ++preserve_input)
	  REGISTER_SOLVER(p, mksolver(preserve_input));
}
