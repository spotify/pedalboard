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


/* Do a REDFT00 problem via an R2HC problem, with some pre/post-processing.

   This code uses the trick from FFTPACK, also documented in a similar
   form by Numerical Recipes.  Unfortunately, this algorithm seems to
   have intrinsic numerical problems (similar to those in
   reodft11e-r2hc.c), possibly due to the fact that it multiplies its
   input by a cosine, causing a loss of precision near the zero.  For
   transforms of 16k points, it has already lost three or four decimal
   places of accuracy, which we deem unacceptable.

   So, we have abandoned this algorithm in favor of the one in
   redft00-r2hc-pad.c, which unfortunately sacrifices 30-50% in speed.
   The only other alternative in the literature that does not have
   similar numerical difficulties seems to be the direct adaptation of
   the Cooley-Tukey decomposition for symmetric data, but this would
   require a whole new set of codelets and it's not clear that it's
   worth it at this point.  However, we did implement the latter
   algorithm for the specific case of odd n (logically adapting the
   split-radix algorithm); see reodft00e-splitradix.c. */

#include "reodft/reodft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld;
     twid *td;
     INT is, os;
     INT n;
     INT vl;
     INT ivs, ovs;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *buf;
     E csum;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[0] + I[is * n];
	  csum = I[0] - I[is * n];
	  for (i = 1; i < n - i; ++i) {
	       E a, b, apb, amb;
	       a = I[is * i];
	       b = I[is * (n - i)];
	       csum += W[2*i] * (amb = K(2.0)*(a - b));
	       amb = W[2*i+1] * amb;
	       apb = (a + b);
	       buf[i] = apb - amb;
	       buf[n - i] = apb + amb;
	  }
	  if (i == n - i) {
	       buf[i] = K(2.0) * I[is * i];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  /* FIXME: use recursive/cascade summation for better stability? */
	  O[0] = buf[0];
	  O[os] = csum;
	  for (i = 1; i + i < n; ++i) {
	       INT k = i + i;
	       O[os * k] = buf[i];
	       O[os * (k + 1)] = O[os * (k - 1)] - buf[n - i];
	  }
	  if (i + i == n) {
	       O[os * n] = buf[i];
	  }
     }

     X(ifree)(buf);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     static const tw_instr redft00e_tw[] = {
          { TW_COS, 0, 1 },
          { TW_SIN, 0, 1 },
          { TW_NEXT, 1, 0 }
     };

     X(plan_awake)(ego->cld, wakefulness);
     X(twiddle_awake)(wakefulness,
		      &ego->td, redft00e_tw, 2*ego->n, 1, (ego->n+1)/2);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(redft00e-r2hc-%D%v%(%p%))", ego->n + 1, ego->vl, ego->cld);
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
     plan *cld;
     R *buf;
     INT n;
     opcnt ops;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *)0;

     p = (const problem_rdft *) p_;

     n = p->sz->dims[0].n - 1;
     A(n > 0);
     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     cld = X(mkplan_d)(plnr, X(mkproblem_rdft_1_d)(X(mktensor_1d)(n, 1, 1), 
						   X(mktensor_0d)(), 
						   buf, buf, R2HC));
     X(ifree)(buf);
     if (!cld)
          return (plan *)0;

     pln = MKPLAN_RDFT(P, &padt, apply);

     pln->n = n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->cld = cld;
     pln->td = 0;

     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     
     X(ops_zero)(&ops);
     ops.other = 8 + (n-1)/2 * 11 + (1 - n % 2) * 5;
     ops.add = 2 + (n-1)/2 * 5;
     ops.mul = (n-1)/2 * 3 + (1 - n % 2) * 1;

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(pln->vl, &ops, &pln->super.super.ops);
     X(ops_madd2)(pln->vl, &cld->ops, &pln->super.super.ops);

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(redft00e_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
