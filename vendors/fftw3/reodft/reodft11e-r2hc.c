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


/* Do an R{E,O}DFT11 problem via an R2HC problem, with some
   pre/post-processing ala FFTPACK.  Use a trick from: 

     S. C. Chan and K. L. Ho, "Direct methods for computing discrete
     sinusoidal transforms," IEE Proceedings F 137 (6), 433--442 (1990).

   to re-express as an REDFT01 (DCT-III) problem.

   NOTE: We no longer use this algorithm, because it turns out to suffer
   a catastrophic loss of accuracy for certain inputs, apparently because
   its post-processing multiplies the output by a cosine.  Near the zero
   of the cosine, the REDFT01 must produce a near-singular output.
*/

#include "reodft/reodft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld;
     twid *td, *td2;
     INT is, os;
     INT n;
     INT vl;
     INT ivs, ovs;
     rdft_kind kind;
} P;

static void apply_re11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W;
     R *buf;
     E cur;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  /* I wish that this didn't require an extra pass. */
	  /* FIXME: use recursive/cascade summation for better stability? */
	  buf[n - 1] = cur = K(2.0) * I[is * (n - 1)];
	  for (i = n - 1; i > 0; --i) {
	       E curnew;
	       buf[(i - 1)] = curnew = K(2.0) * I[is * (i - 1)] - cur;
	       cur = curnew;
	  }
	  
	  W = ego->td->W;
	  for (i = 1; i < n - i; ++i) {
	       E a, b, apb, amb, wa, wb;
	       a = buf[i];
	       b = buf[n - i];
	       apb = a + b;
	       amb = a - b;
	       wa = W[2*i];
	       wb = W[2*i + 1];
	       buf[i] = wa * amb + wb * apb; 
	       buf[n - i] = wa * apb - wb * amb; 
	  }
	  if (i == n - i) {
	       buf[i] = K(2.0) * buf[i] * W[2*i];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  W = ego->td2->W;
	  O[0] = W[0] * buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b;
	       INT k;
	       a = buf[i];
	       b = buf[n - i];
	       k = i + i;
	       O[os * (k - 1)] = W[k - 1] * (a - b);
	       O[os * k] = W[k] * (a + b);
	  }
	  if (i == n - i) {
	       O[os * (n - 1)] = W[n - 1] * buf[i];
	  }
     }

     X(ifree)(buf);
}

/* like for rodft01, rodft11 is obtained from redft11 by
   reversing the input and flipping the sign of every other output. */
static void apply_ro11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W;
     R *buf;
     E cur;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  /* I wish that this didn't require an extra pass. */
	  /* FIXME: use recursive/cascade summation for better stability? */
	  buf[n - 1] = cur = K(2.0) * I[0];
	  for (i = n - 1; i > 0; --i) {
	       E curnew;
	       buf[(i - 1)] = curnew = K(2.0) * I[is * (n - i)] - cur;
	       cur = curnew;
	  }
	  
	  W = ego->td->W;
	  for (i = 1; i < n - i; ++i) {
	       E a, b, apb, amb, wa, wb;
	       a = buf[i];
	       b = buf[n - i];
	       apb = a + b;
	       amb = a - b;
	       wa = W[2*i];
	       wb = W[2*i + 1];
	       buf[i] = wa * amb + wb * apb; 
	       buf[n - i] = wa * apb - wb * amb; 
	  }
	  if (i == n - i) {
	       buf[i] = K(2.0) * buf[i] * W[2*i];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  W = ego->td2->W;
	  O[0] = W[0] * buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b;
	       INT k;
	       a = buf[i];
	       b = buf[n - i];
	       k = i + i;
	       O[os * (k - 1)] = W[k - 1] * (b - a);
	       O[os * k] = W[k] * (a + b);
	  }
	  if (i == n - i) {
	       O[os * (n - 1)] = -W[n - 1] * buf[i];
	  }
     }

     X(ifree)(buf);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     static const tw_instr reodft010e_tw[] = {
          { TW_COS, 0, 1 },
          { TW_SIN, 0, 1 },
          { TW_NEXT, 1, 0 }
     };
     static const tw_instr reodft11e_tw[] = {
          { TW_COS, 1, 1 },
          { TW_NEXT, 2, 0 }
     };

     X(plan_awake)(ego->cld, wakefulness);

     X(twiddle_awake)(wakefulness,
		      &ego->td, reodft010e_tw, 4*ego->n, 1, ego->n/2+1);
     X(twiddle_awake)(wakefulness,
		      &ego->td2, reodft11e_tw, 8*ego->n, 1, ego->n * 2);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(%se-r2hc-%D%v%(%p%))",
	      X(rdft_kind_str)(ego->kind), ego->n, ego->vl, ego->cld);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;

     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1
	     && (p->kind[0] == REDFT11 || p->kind[0] == RODFT11)
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

     n = p->sz->dims[0].n;
     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     cld = X(mkplan_d)(plnr, X(mkproblem_rdft_1_d)(X(mktensor_1d)(n, 1, 1),
                                                   X(mktensor_0d)(),
                                                   buf, buf, R2HC));
     X(ifree)(buf);
     if (!cld)
          return (plan *)0;

     pln = MKPLAN_RDFT(P, &padt, p->kind[0]==REDFT11 ? apply_re11:apply_ro11);
     pln->n = n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->cld = cld;
     pln->td = pln->td2 = 0;
     pln->kind = p->kind[0];
     
     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     
     X(ops_zero)(&ops);
     ops.other = 5 + (n-1) * 2 + (n-1)/2 * 12 + (1 - n % 2) * 6;
     ops.add = (n - 1) * 1 + (n-1)/2 * 6;
     ops.mul = 2 + (n-1) * 1 + (n-1)/2 * 6 + (1 - n % 2) * 3;

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

void X(reodft11e_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
