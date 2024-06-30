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


/* Solve a DHT problem (Discrete Hartley Transform) via post-processing
   of an R2HC problem. */

#include "rdft/rdft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld;
     INT os;
     INT n;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT os = ego->os;
     INT i, n = ego->n;

     {
	  plan_rdft *cld = (plan_rdft *) ego->cld;
	  cld->apply((plan *) cld, I, O);
     }

     for (i = 1; i < n - i; ++i) {
	  E a, b;
	  a = O[os * i];
	  b = O[os * (n - i)];
#if FFT_SIGN == -1
	  O[os * i] = a - b;
	  O[os * (n - i)] = a + b;
#else
	  O[os * i] = a + b;
	  O[os * (n - i)] = a - b;
#endif
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
     p->print(p, "(dht-r2hc-%D%(%p%))", ego->n, ego->cld);
}

static int applicable0(const problem *p_, const planner *plnr)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     return (1
	     && !NO_DHT_R2HCP(plnr)
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     && p->kind[0] == DHT
	  );
}

static int applicable(const solver *ego, const problem *p, const planner *plnr)
{
     UNUSED(ego);
     return (!NO_SLOWP(plnr) && applicable0(p, plnr));
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     P *pln;
     const problem_rdft *p;
     plan *cld;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *)0;

     p = (const problem_rdft *) p_;

     /* NO_DHT_R2HC stops infinite loops with rdft-dht.c */
     cld = X(mkplan_f_d)(plnr, 
			 X(mkproblem_rdft_1)(p->sz, p->vecsz, 
					     p->I, p->O, R2HC),
			 NO_DHT_R2HC, 0, 0);
     if (!cld) return (plan *)0;

     pln = MKPLAN_RDFT(P, &padt, apply);

     pln->n = p->sz->dims[0].n;
     pln->os = p->sz->dims[0].os;
     pln->cld = cld;
     
     pln->super.super.ops = cld->ops;
     pln->super.super.ops.other += 4 * ((pln->n - 1)/2);
     pln->super.super.ops.add += 2 * ((pln->n - 1)/2);

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(dht_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
