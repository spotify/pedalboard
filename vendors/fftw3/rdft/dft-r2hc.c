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


/* Compute the complex DFT by combining R2HC RDFTs on the real
   and imaginary parts.   This could be useful for people just wanting
   to link to the real codelets and not the complex ones.  It could
   also even be faster than the complex algorithms for split (as opposed
   to interleaved) real/imag complex data. */

#include "rdft/rdft.h"
#include "dft/dft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_dft super;
     plan *cld;
     INT ishift, oshift;
     INT os;
     INT n;
} P;

static void apply(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT n;

     UNUSED(ii);

     { /* transform vector of real & imag parts: */
	  plan_rdft *cld = (plan_rdft *) ego->cld;
	  cld->apply((plan *) cld, ri + ego->ishift, ro + ego->oshift);
     }

     n = ego->n;
     if (n > 1) {
	  INT i, os = ego->os;
	  for (i = 1; i < (n + 1)/2; ++i) {
	       E rop, iop, iom, rom;
	       rop = ro[os * i];
	       iop = io[os * i];
	       rom = ro[os * (n - i)];
	       iom = io[os * (n - i)];
	       ro[os * i] = rop - iom;
	       io[os * i] = iop + rom;
	       ro[os * (n - i)] = rop + iom;
	       io[os * (n - i)] = iop - rom;
	  }
     }
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
     p->print(p, "(dft-r2hc-%D%(%p%))", ego->n, ego->cld);
}


static int applicable0(const problem *p_)
{
     const problem_dft *p = (const problem_dft *) p_;
     return ((p->sz->rnk == 1 && p->vecsz->rnk == 0)
	     || (p->sz->rnk == 0 && FINITE_RNK(p->vecsz->rnk))
	  );
}

static int splitp(R *r, R *i, INT n, INT s)
{
     return ((r > i ? (r - i) : (i - r)) >= n * (s > 0 ? s : 0-s));
}

static int applicable(const problem *p_, const planner *plnr)
{
     if (!applicable0(p_)) return 0;

     {
	  const problem_dft *p = (const problem_dft *) p_;

	  /* rank-0 problems are always OK */
	  if (p->sz->rnk == 0) return 1;

	  /* this solver is ok for split arrays */
	  if (p->sz->rnk == 1 &&
	      splitp(p->ri, p->ii, p->sz->dims[0].n, p->sz->dims[0].is) &&
	      splitp(p->ro, p->io, p->sz->dims[0].n, p->sz->dims[0].os))
	       return 1;

	  return !(NO_DFT_R2HCP(plnr));
     }
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     P *pln;
     const problem_dft *p;
     plan *cld;
     INT ishift = 0, oshift = 0;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     UNUSED(ego_);
     if (!applicable(p_, plnr))
          return (plan *)0;

     p = (const problem_dft *) p_;

     {
	  tensor *ri_vec = X(mktensor_1d)(2, p->ii - p->ri, p->io - p->ro);
	  tensor *cld_vec = X(tensor_append)(ri_vec, p->vecsz);
	  int i;
	  for (i = 0; i < cld_vec->rnk; ++i) { /* make all istrides > 0 */
	       if (cld_vec->dims[i].is < 0) {
		    INT nm1 = cld_vec->dims[i].n - 1;
		    ishift -= nm1 * (cld_vec->dims[i].is *= -1);
		    oshift -= nm1 * (cld_vec->dims[i].os *= -1);
	       }
	  }
	  cld = X(mkplan_d)(plnr, 
			    X(mkproblem_rdft_1)(p->sz, cld_vec, 
						p->ri + ishift, 
						p->ro + oshift, R2HC));
	  X(tensor_destroy2)(ri_vec, cld_vec);
     }
     if (!cld) return (plan *)0;

     pln = MKPLAN_DFT(P, &padt, apply);

     if (p->sz->rnk == 0) {
	  pln->n = 1;
	  pln->os = 0;
     }
     else {
	  pln->n = p->sz->dims[0].n;
	  pln->os = p->sz->dims[0].os;
     }
     pln->ishift = ishift;
     pln->oshift = oshift;

     pln->cld = cld;
     
     pln->super.super.ops = cld->ops;
     pln->super.super.ops.other += 8 * ((pln->n - 1)/2);
     pln->super.super.ops.add += 4 * ((pln->n - 1)/2);
     pln->super.super.ops.other += 1; /* estimator hack for nop plans */

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(dft_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
