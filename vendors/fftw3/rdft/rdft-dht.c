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


/* Solve an R2HC/HC2R problem via post/pre processing of a DHT.  This
   is mainly useful because we can use Rader to compute DHTs of prime
   sizes.  It also allows us to express hc2r problems in terms of r2hc
   (via dht-r2hc), and to do hc2r problems without destroying the input. */

#include "rdft/rdft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld;
     INT is, os;
     INT n;
} P;

static void apply_r2hc(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT os;
     INT i, n;

     {
	  plan_rdft *cld = (plan_rdft *) ego->cld;
	  cld->apply((plan *) cld, I, O);
     }

     n = ego->n;
     os = ego->os;
     for (i = 1; i < n - i; ++i) {
	  E a, b;
	  a = K(0.5) * O[os * i];
	  b = K(0.5) * O[os * (n - i)];
	  O[os * i] = a + b;
#if FFT_SIGN == -1
	  O[os * (n - i)] = b - a;
#else
	  O[os * (n - i)] = a - b;
#endif
     }
}

/* hc2r, destroying input as usual */
static void apply_hc2r(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is;
     INT i, n = ego->n;

     for (i = 1; i < n - i; ++i) {
	  E a, b;
	  a = I[is * i];
	  b = I[is * (n - i)];
#if FFT_SIGN == -1
	  I[is * i] = a - b;
	  I[is * (n - i)] = a + b;
#else
	  I[is * i] = a + b;
	  I[is * (n - i)] = a - b;
#endif
     }

     {
	  plan_rdft *cld = (plan_rdft *) ego->cld;
	  cld->apply((plan *) cld, I, O);
     }
}

/* hc2r, without destroying input */
static void apply_hc2r_save(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;

     O[0] = I[0];
     for (i = 1; i < n - i; ++i) {
	  E a, b;
	  a = I[is * i];
	  b = I[is * (n - i)];
#if FFT_SIGN == -1
	  O[os * i] = a - b;
	  O[os * (n - i)] = a + b;
#else
	  O[os * i] = a + b;
	  O[os * (n - i)] = a - b;
#endif
     }
     if (i == n - i)
	  O[os * i] = I[is * i];

     {
	  plan_rdft *cld = (plan_rdft *) ego->cld;
	  cld->apply((plan *) cld, O, O);
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
     p->print(p, "(%s-dht-%D%(%p%))", 
	      ego->super.apply == apply_r2hc ? "r2hc" : "hc2r",
	      ego->n, ego->cld);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     && (p->kind[0] == R2HC || p->kind[0] == HC2R)

	     /* hack: size-2 DHT etc. are defined as being equivalent
		to size-2 R2HC in problem.c, so we need this to prevent
		infinite loops for size 2 in EXHAUSTIVE mode: */
	     && p->sz->dims[0].n > 2
	  );
}

static int applicable(const solver *ego, const problem *p_, 
		      const planner *plnr)
{
     return (!NO_SLOWP(plnr) && applicable0(ego, p_));
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     P *pln;
     const problem_rdft *p;
     problem *cldp;
     plan *cld;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *)0;

     p = (const problem_rdft *) p_;

     if (p->kind[0] == R2HC || !NO_DESTROY_INPUTP(plnr))
	  cldp = X(mkproblem_rdft_1)(p->sz, p->vecsz, p->I, p->O, DHT);
     else {
	  tensor *sz = X(tensor_copy_inplace)(p->sz, INPLACE_OS);
	  cldp = X(mkproblem_rdft_1)(sz, p->vecsz, p->O, p->O, DHT);
	  X(tensor_destroy)(sz);
     }
     cld = X(mkplan_d)(plnr, cldp);
     if (!cld) return (plan *)0;

     pln = MKPLAN_RDFT(P, &padt, p->kind[0] == R2HC ? 
		       apply_r2hc : (NO_DESTROY_INPUTP(plnr) ?
				     apply_hc2r_save : apply_hc2r));
     pln->n = p->sz->dims[0].n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->cld = cld;
     
     pln->super.super.ops = cld->ops;
     pln->super.super.ops.other += 4 * ((pln->n - 1)/2);
     pln->super.super.ops.add += 2 * ((pln->n - 1)/2);
     if (p->kind[0] == R2HC)
	  pln->super.super.ops.mul += 2 * ((pln->n - 1)/2);
     if (pln->super.apply == apply_hc2r_save)
	  pln->super.super.ops.other += 2 + (pln->n % 2 ? 0 : 2);

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(rdft_dht_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
