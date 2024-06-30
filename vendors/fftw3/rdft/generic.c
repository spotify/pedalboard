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

#include "rdft/rdft.h"

typedef struct {
     solver super;
     rdft_kind kind;
} S;

typedef struct {
     plan_rdft super;
     twid *td;
     INT n, is, os;
     rdft_kind kind;
} P;

/***************************************************************************/

static void cdot_r2hc(INT n, const E *x, const R *w, R *or0, R *oi1)
{
     INT i;

     E rr = x[0], ri = 0;
     x += 1;
     for (i = 1; i + i < n; ++i) {
	  rr += x[0] * w[0];
	  ri += x[1] * w[1];
	  x += 2; w += 2;
     }
     *or0 = rr;
     *oi1 = ri;
}

static void hartley_r2hc(INT n, const R *xr, INT xs, E *o, R *pr)
{
     INT i;
     E sr;
     o[0] = sr = xr[0]; o += 1;
     for (i = 1; i + i < n; ++i) {
	  R a, b;
	  a = xr[i * xs];
	  b =  xr[(n - i) * xs];
	  sr += (o[0] = a + b);
#if FFT_SIGN == -1
	  o[1] = b - a;
#else
	  o[1] = a - b;
#endif
	  o += 2;
     }
     *pr = sr;
}
		    
static void apply_r2hc(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT i;
     INT n = ego->n, is = ego->is, os = ego->os;
     const R *W = ego->td->W;
     E *buf;
     size_t bufsz = n * sizeof(E);

     BUF_ALLOC(E *, buf, bufsz);
     hartley_r2hc(n, I, is, buf, O);

     for (i = 1; i + i < n; ++i) {
	  cdot_r2hc(n, buf, W, O + i * os, O + (n - i) * os);
	  W += n - 1;
     }

     BUF_FREE(buf, bufsz);
}


static void cdot_hc2r(INT n, const E *x, const R *w, R *or0, R *or1)
{
     INT i;

     E rr = x[0], ii = 0; 
     x += 1;
     for (i = 1; i + i < n; ++i) {
	  rr += x[0] * w[0];
	  ii += x[1] * w[1];
	  x += 2; w += 2;
     }
#if FFT_SIGN == -1
     *or0 = rr - ii;
     *or1 = rr + ii;
#else
     *or0 = rr + ii;
     *or1 = rr - ii;
#endif
}

static void hartley_hc2r(INT n, const R *x, INT xs, E *o, R *pr)
{
     INT i;
     E sr;

     o[0] = sr = x[0]; o += 1;
     for (i = 1; i + i < n; ++i) {
	  sr += (o[0] = x[i * xs] + x[i * xs]);
	  o[1] = x[(n - i) * xs] + x[(n - i) * xs];
	  o += 2;
     }
     *pr = sr;
}

static void apply_hc2r(const plan *ego_, R *I, R *O)		    
{
     const P *ego = (const P *) ego_;
     INT i;
     INT n = ego->n, is = ego->is, os = ego->os;
     const R *W = ego->td->W;
     E *buf;
     size_t bufsz = n * sizeof(E);

     BUF_ALLOC(E *, buf, bufsz);
     hartley_hc2r(n, I, is, buf, O);

     for (i = 1; i + i < n; ++i) {
	  cdot_hc2r(n, buf, W, O + i * os, O + (n - i) * os);
	  W += n - 1;
     }

     BUF_FREE(buf, bufsz);
}


/***************************************************************************/

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

     p->print(p, "(rdft-generic-%s-%D)", 
	      ego->kind == R2HC ? "r2hc" : "hc2r", 
	      ego->n);
}

static int applicable(const S *ego, const problem *p_, 
		      const planner *plnr)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     && (p->sz->dims[0].n % 2) == 1 
	     && CIMPLIES(NO_LARGE_GENERICP(plnr), p->sz->dims[0].n < GENERIC_MIN_BAD)
	     && CIMPLIES(NO_SLOWP(plnr), p->sz->dims[0].n > GENERIC_MAX_SLOW)
	     && X(is_prime)(p->sz->dims[0].n)
	     && p->kind[0] == ego->kind
	  );
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *)ego_;
     const problem_rdft *p;
     P *pln;
     INT n;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, X(plan_null_destroy)
     };

     if (!applicable(ego, p_, plnr))
          return (plan *)0;

     p = (const problem_rdft *) p_;
     pln = MKPLAN_RDFT(P, &padt, 
		       R2HC_KINDP(p->kind[0]) ? apply_r2hc : apply_hc2r);

     pln->n = n = p->sz->dims[0].n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->td = 0;
     pln->kind = ego->kind;

     pln->super.super.ops.add = (n-1) * 2.5;
     pln->super.super.ops.mul = 0;
     pln->super.super.ops.fma = 0.5 * (n-1) * (n-1) ;
#if 0 /* these are nice pipelined sequential loads and should cost nothing */
     pln->super.super.ops.other = (n-1)*(2 + 1 + (n-1));  /* approximate */
#endif

     return &(pln->super.super);
}

static solver *mksolver(rdft_kind kind)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->kind = kind;
     return &(slv->super);
}

void X(rdft_generic_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver(R2HC));
     REGISTER_SOLVER(p, mksolver(HC2R));
}
