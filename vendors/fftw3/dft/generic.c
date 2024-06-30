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

#include "dft/dft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_dft super;
     twid *td;
     INT n, is, os;
} P;


static void cdot(INT n, const E *x, const R *w, 
		 R *or0, R *oi0, R *or1, R *oi1)
{
     INT i;

     E rr = x[0], ri = 0, ir = x[1], ii = 0; 
     x += 2;
     for (i = 1; i + i < n; ++i) {
	  rr += x[0] * w[0];
	  ir += x[1] * w[0];
	  ri += x[2] * w[1];
	  ii += x[3] * w[1];
	  x += 4; w += 2;
     }
     *or0 = rr + ii;
     *oi0 = ir - ri;
     *or1 = rr - ii;
     *oi1 = ir + ri;
}

static void hartley(INT n, const R *xr, const R *xi, INT xs, E *o,
		    R *pr, R *pi)
{
     INT i;
     E sr, si;
     o[0] = sr = xr[0]; o[1] = si = xi[0]; o += 2;
     for (i = 1; i + i < n; ++i) {
	  sr += (o[0] = xr[i * xs] + xr[(n - i) * xs]);
	  si += (o[1] = xi[i * xs] + xi[(n - i) * xs]);
	  o[2] = xr[i * xs] - xr[(n - i) * xs];
	  o[3] = xi[i * xs] - xi[(n - i) * xs];
	  o += 4;
     }
     *pr = sr;
     *pi = si;
}
		    
static void apply(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT i;
     INT n = ego->n, is = ego->is, os = ego->os;
     const R *W = ego->td->W;
     E *buf;
     size_t bufsz = n * 2 * sizeof(E);

     BUF_ALLOC(E *, buf, bufsz);
     hartley(n, ri, ii, is, buf, ro, io);

     for (i = 1; i + i < n; ++i) {
	  cdot(n, buf, W,
	       ro + i * os, io + i * os,
	       ro + (n - i) * os, io + (n - i) * os);
	  W += n - 1;
     }

     BUF_FREE(buf, bufsz);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     static const tw_instr half_tw[] = {
	  { TW_HALF, 1, 0 },
	  { TW_NEXT, 1, 0 }
     };

     X(twiddle_awake)(wakefulness, &ego->td, half_tw, ego->n, ego->n,
		      (ego->n - 1) / 2);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;

     p->print(p, "(dft-generic-%D)", ego->n);
}

static int applicable(const solver *ego, const problem *p_, 
		      const planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     UNUSED(ego);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     && (p->sz->dims[0].n % 2) == 1 
	     && CIMPLIES(NO_LARGE_GENERICP(plnr), p->sz->dims[0].n < GENERIC_MIN_BAD)
	     && CIMPLIES(NO_SLOWP(plnr), p->sz->dims[0].n > GENERIC_MAX_SLOW)
	     && X(is_prime)(p->sz->dims[0].n)
	  );
}

static plan *mkplan(const solver *ego, const problem *p_, planner *plnr)
{
     const problem_dft *p;
     P *pln;
     INT n;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, X(plan_null_destroy)
     };

     if (!applicable(ego, p_, plnr))
          return (plan *)0;

     pln = MKPLAN_DFT(P, &padt, apply);

     p = (const problem_dft *) p_;
     pln->n = n = p->sz->dims[0].n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->td = 0;

     pln->super.super.ops.add = (n-1) * 5;
     pln->super.super.ops.mul = 0;
     pln->super.super.ops.fma = (n-1) * (n-1) ;
#if 0 /* these are nice pipelined sequential loads and should cost nothing */
     pln->super.super.ops.other = (n-1)*(4 + 1 + 2 * (n-1));  /* approximate */
#endif

     return &(pln->super.super);
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(dft_generic_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
