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


/* Do a REDFT00 problem via an R2HC problem, padded symmetrically to
   twice the size.  This is asymptotically a factor of ~2 worse than
   redft00e-r2hc.c (the algorithm used in e.g. FFTPACK and Numerical
   Recipes), but we abandoned the latter after we discovered that it
   has intrinsic accuracy problems. */

#include "reodft/reodft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld, *cldcpy;
     INT is;
     INT n;
     INT vl;
     INT ivs, ovs;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * (2*n), BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[0];
	  for (i = 1; i < n; ++i) {
	       R a = I[i * is];
	       buf[i] = a;
	       buf[2*n - i] = a;
	  }
	  buf[i] = I[i * is]; /* i == n, Nyquist */
	  
	  /* r2hc transform of size 2*n */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  /* copy n+1 real numbers (real parts of hc array) from buf to O */
	  {
	       plan_rdft *cldcpy = (plan_rdft *) ego->cldcpy;
	       cldcpy->apply((plan *) cldcpy, buf, O);
	  }
     }

     X(ifree)(buf);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld, wakefulness);
     X(plan_awake)(ego->cldcpy, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldcpy);
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(redft00e-r2hc-pad-%D%v%(%p%)%(%p%))", 
	      ego->n + 1, ego->vl, ego->cld, ego->cldcpy);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1
	     && p->kind[0] == REDFT00
	     && p->sz->dims[0].n > 1  /* n == 1 is not well-defined */
	  );
}

static int applicable(const solver *ego, const problem *p, const planner *plnr)
{
     return (!NO_SLOWP(plnr) && applicable0(ego, p));
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     P *pln;
     const problem_rdft *p;
     plan *cld = (plan *) 0, *cldcpy;
     R *buf = (R *) 0;
     INT n;
     INT vl, ivs, ovs;
     opcnt ops;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
	  goto nada;

     p = (const problem_rdft *) p_;

     n = p->sz->dims[0].n - 1;
     A(n > 0);
     buf = (R *) MALLOC(sizeof(R) * (2*n), BUFFERS);

     cld = X(mkplan_d)(plnr,X(mkproblem_rdft_1_d)(X(mktensor_1d)(2*n,1,1), 
						  X(mktensor_0d)(), 
						  buf, buf, R2HC));
     if (!cld)
	  goto nada;

     X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs);
     cldcpy =
	  X(mkplan_d)(plnr,
		      X(mkproblem_rdft_1_d)(X(mktensor_0d)(),
					    X(mktensor_1d)(n+1,1,
							   p->sz->dims[0].os), 
					    buf, TAINT(p->O, ovs), R2HC));
     if (!cldcpy)
	  goto nada;

     X(ifree)(buf);

     pln = MKPLAN_RDFT(P, &padt, apply);

     pln->n = n;
     pln->is = p->sz->dims[0].is;
     pln->cld = cld;
     pln->cldcpy = cldcpy;
     pln->vl = vl;
     pln->ivs = ivs;
     pln->ovs = ovs;
     
     X(ops_zero)(&ops);
     ops.other = n + 2*n; /* loads + stores (input -> buf) */

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(pln->vl, &ops, &pln->super.super.ops);
     X(ops_madd2)(pln->vl, &cld->ops, &pln->super.super.ops);
     X(ops_madd2)(pln->vl, &cldcpy->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(ifree0)(buf);
     if (cld)
	  X(plan_destroy_internal)(cld);  
     return (plan *)0;
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(redft00e_r2hc_pad_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
