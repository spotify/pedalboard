/*
 * Copyright (c) 2005 Matteo Frigo
 * Copyright (c) 2005 Massachusetts Institute of Technology
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


/* Do an R{E,O}DFT00 problem (of an odd length n) recursively via an
   R{E,O}DFT00 problem and an RDFT problem of half the length.

   This works by "logically" expanding the array to a real-even/odd DFT of
   length 2n-/+2 and then applying the split-radix algorithm.

   In this way, we can avoid having to pad to twice the length
   (ala redft00-r2hc-pad), saving a factor of ~2 for n=2^m+/-1,
   but don't incur the accuracy loss that the "ordinary" algorithm
   sacrifices (ala redft00-r2hc.c).
*/

#include "reodft/reodft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *clde, *cldo;
     twid *td;
     INT is, os;
     INT n;
     INT vl;
     INT ivs, ovs;
} P;

/* redft00 */
static void apply_e(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, j, n = ego->n + 1, n2 = (n-1)/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W - 2;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n2, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  /* do size (n-1)/2 r2hc transform of odd-indexed elements
	     with stride 4, "wrapping around" end of array with even
	     boundary conditions */
	  for (j = 0, i = 1; i < n; i += 4)
	       buf[j++] = I[is * i];
	  for (i = 2*n-2-i; i > 0; i -= 4)
	       buf[j++] = I[is * i];
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cldo;
	       cld->apply((plan *) cld, buf, buf);
	  }

	  /* do size (n+1)/2 redft00 of the even-indexed elements,
	     writing to O: */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->clde;
	       cld->apply((plan *) cld, I, O);
	  }

	  /* combine the results with the twiddle factors to get output */
	  { /* DC element */
	       E b20 = O[0], b0 = K(2.0) * buf[0];
	       O[0] = b20 + b0;
	       O[2*(n2*os)] = b20 - b0;
	       /* O[n2*os] = O[n2*os]; */
	  }
	  for (i = 1; i < n2 - i; ++i) {
	       E ap, am, br, bi, wr, wi, wbr, wbi;
	       br = buf[i];
	       bi = buf[n2 - i];
	       wr = W[2*i];
	       wi = W[2*i+1];
#if FFT_SIGN == -1
	       wbr = K(2.0) * (wr*br + wi*bi);
	       wbi = K(2.0) * (wr*bi - wi*br);
#else
	       wbr = K(2.0) * (wr*br - wi*bi);
	       wbi = K(2.0) * (wr*bi + wi*br);
#endif
	       ap = O[i*os];
	       O[i*os] = ap + wbr;
	       O[(2*n2 - i)*os] = ap - wbr;
	       am = O[(n2 - i)*os];
#if FFT_SIGN == -1
	       O[(n2 - i)*os] = am - wbi;
	       O[(n2 + i)*os] = am + wbi;
#else
	       O[(n2 - i)*os] = am + wbi;
	       O[(n2 + i)*os] = am - wbi;
#endif
	  }
	  if (i == n2 - i) { /* Nyquist element */
	       E ap, wbr;
	       wbr = K(2.0) * (W[2*i] * buf[i]);
	       ap = O[i*os];
	       O[i*os] = ap + wbr;
	       O[(2*n2 - i)*os] = ap - wbr;
	  }
     }

     X(ifree)(buf);
}

/* rodft00 */
static void apply_o(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, j, n = ego->n - 1, n2 = (n+1)/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W - 2;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n2, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  /* do size (n+1)/2 r2hc transform of even-indexed elements
	     with stride 4, "wrapping around" end of array with odd
	     boundary conditions */
	  for (j = 0, i = 0; i < n; i += 4)
	       buf[j++] = I[is * i];
	  for (i = 2*n-i; i > 0; i -= 4)
	       buf[j++] = -I[is * i];
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cldo;
	       cld->apply((plan *) cld, buf, buf);
	  }

	  /* do size (n-1)/2 rodft00 of the odd-indexed elements,
	     writing to O: */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->clde;
	       if (I == O) {
		    /* can't use I+is and I, subplan would lose in-placeness */
		    cld->apply((plan *) cld, I + is, I + is);
		    /* we could maybe avoid this copy by modifying the
		       twiddle loop, but currently I can't be bothered. */
		    A(is >= os);
		    for (i = 0; i < n2-1; ++i)
			 O[os*i] = I[is*(i+1)];
	       }
	       else
		    cld->apply((plan *) cld, I + is, O);
	  }

	  /* combine the results with the twiddle factors to get output */
	  O[(n2-1)*os] = K(2.0) * buf[0];
	  for (i = 1; i < n2 - i; ++i) {
	       E ap, am, br, bi, wr, wi, wbr, wbi;
	       br = buf[i];
	       bi = buf[n2 - i];
	       wr = W[2*i];
	       wi = W[2*i+1];
#if FFT_SIGN == -1
	       wbr = K(2.0) * (wr*br + wi*bi);
	       wbi = K(2.0) * (wi*br - wr*bi);
#else
	       wbr = K(2.0) * (wr*br - wi*bi);
	       wbi = K(2.0) * (wr*bi + wi*br);
#endif
	       ap = O[(i-1)*os];
	       O[(i-1)*os] = wbi + ap;
	       O[(2*n2-1 - i)*os] = wbi - ap;
	       am = O[(n2-1 - i)*os];
#if FFT_SIGN == -1
	       O[(n2-1 - i)*os] = wbr + am;
	       O[(n2-1 + i)*os] = wbr - am;
#else
	       O[(n2-1 - i)*os] = wbr + am;
	       O[(n2-1 + i)*os] = wbr - am;
#endif
	  }
	  if (i == n2 - i) { /* Nyquist element */
	       E ap, wbi;
	       wbi = K(2.0) * (W[2*i+1] * buf[i]);
	       ap = O[(i-1)*os];
	       O[(i-1)*os] = wbi + ap;
	       O[(2*n2-1 - i)*os] = wbi - ap;
	  }
     }

     X(ifree)(buf);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     static const tw_instr reodft00e_tw[] = {
          { TW_COS, 1, 1 },
          { TW_SIN, 1, 1 },
          { TW_NEXT, 1, 0 }
     };

     X(plan_awake)(ego->clde, wakefulness);
     X(plan_awake)(ego->cldo, wakefulness);
     X(twiddle_awake)(wakefulness, &ego->td, reodft00e_tw, 
		      2*ego->n, 1, ego->n/4);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldo);
     X(plan_destroy_internal)(ego->clde);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     if (ego->super.apply == apply_e)
	  p->print(p, "(redft00e-splitradix-%D%v%(%p%)%(%p%))", 
		   ego->n + 1, ego->vl, ego->clde, ego->cldo);
     else
	  p->print(p, "(rodft00e-splitradix-%D%v%(%p%)%(%p%))", 
		   ego->n - 1, ego->vl, ego->clde, ego->cldo);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1
	     && (p->kind[0] == REDFT00 || p->kind[0] == RODFT00)
	     && p->sz->dims[0].n > 1  /* don't create size-0 sub-plans */
	     && p->sz->dims[0].n % 2  /* odd: 4 divides "logical" DFT */
	     && (p->I != p->O || p->vecsz->rnk == 0
		 || p->vecsz->dims[0].is == p->vecsz->dims[0].os)
	     && (p->kind[0] != RODFT00 || p->I != p->O || 
		 p->sz->dims[0].is >= p->sz->dims[0].os) /* laziness */
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
     plan *clde, *cldo;
     R *buf;
     INT n, n0;
     opcnt ops;
     int inplace_odd;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr))
          return (plan *)0;

     p = (const problem_rdft *) p_;

     n = (n0 = p->sz->dims[0].n) + (p->kind[0] == REDFT00 ? (INT)-1 : (INT)1);
     A(n > 0 && n % 2 == 0);
     buf = (R *) MALLOC(sizeof(R) * (n/2), BUFFERS);

     inplace_odd = p->kind[0]==RODFT00 && p->I == p->O;
     clde = X(mkplan_d)(plnr, X(mkproblem_rdft_1_d)(
			     X(mktensor_1d)(n0-n/2, 2*p->sz->dims[0].is, 
					    inplace_odd ? p->sz->dims[0].is
					    : p->sz->dims[0].os), 
			     X(mktensor_0d)(), 
			     TAINT(p->I 
				   + p->sz->dims[0].is * (p->kind[0]==RODFT00),
				   p->vecsz->rnk ? p->vecsz->dims[0].is : 0),
			     TAINT(p->O
				   + p->sz->dims[0].is * inplace_odd,
				   p->vecsz->rnk ? p->vecsz->dims[0].os : 0),
			     p->kind[0]));
     if (!clde) {
	  X(ifree)(buf);
          return (plan *)0;
     }

     cldo = X(mkplan_d)(plnr, X(mkproblem_rdft_1_d)(
			     X(mktensor_1d)(n/2, 1, 1), 
			     X(mktensor_0d)(), 
			     buf, buf, R2HC));
     X(ifree)(buf);
     if (!cldo)
          return (plan *)0;

     pln = MKPLAN_RDFT(P, &padt, p->kind[0] == REDFT00 ? apply_e : apply_o);

     pln->n = n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->clde = clde;
     pln->cldo = cldo;
     pln->td = 0;

     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     
     X(ops_zero)(&ops);
     ops.other = n/2;
     ops.add = (p->kind[0]==REDFT00 ? (INT)2 : (INT)0) +
	  (n/2-1)/2 * 6 + ((n/2)%2==0) * 2;
     ops.mul = 1 + (n/2-1)/2 * 6 + ((n/2)%2==0) * 2;

     /* tweak ops.other so that r2hc-pad is used for small sizes, which
	seems to be a lot faster on my machine: */
     ops.other += 256;

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(pln->vl, &ops, &pln->super.super.ops);
     X(ops_madd2)(pln->vl, &clde->ops, &pln->super.super.ops);
     X(ops_madd2)(pln->vl, &cldo->ops, &pln->super.super.ops);

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(reodft00e_splitradix_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
