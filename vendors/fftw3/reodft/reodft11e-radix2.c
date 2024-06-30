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


/* Do an R{E,O}DFT11 problem of *even* size by a pair of R2HC problems
   of half the size, plus some pre/post-processing.  Use a trick from:

   Zhongde Wang, "On computing the discrete Fourier and cosine transforms,"
   IEEE Trans. Acoust. Speech Sig. Proc. ASSP-33 (4), 1341--1344 (1985).

   to re-express as a pair of half-size REDFT01 (DCT-III) problems.  Our
   implementation looks quite a bit different from the algorithm described
   in the paper because we combined the paper's pre/post-processing with
   the pre/post-processing used to turn REDFT01 into R2HC.  (Also, the
   paper uses a DCT/DST pair, but we turn the DST into a DCT via the
   usual reordering/sign-flip trick.  We additionally combined a couple
   of the matrices/transformations of the paper into a single pass.)

   NOTE: We originally used a simpler method by S. C. Chan and K. L. Ho
   that turned out to have numerical problems; see reodft11e-r2hc.c.

   (For odd sizes, see reodft11e-r2hc-odd.c.)
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
     INT i, n = ego->n, n2 = n/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *W2;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = K(2.0) * I[0];
	  buf[n2] = K(2.0) * I[is * (n - 1)];
	  for (i = 1; i + i < n2; ++i) {
	       INT k = i + i;
	       E a, b, a2, b2;
	       {
		    E u, v;
		    u = I[is * (k - 1)];
		    v = I[is * k];
		    a = u + v;
		    b2 = u - v;
	       }
	       {
		    E u, v;
		    u = I[is * (n - k - 1)];
		    v = I[is * (n - k)];
		    b = u + v;
		    a2 = u - v;
	       }
	       {
		    E wa, wb;
		    wa = W[2*i];
		    wb = W[2*i + 1];
		    {
			 E apb, amb;
			 apb = a + b;
			 amb = a - b;
			 buf[i] = wa * amb + wb * apb; 
			 buf[n2 - i] = wa * apb - wb * amb; 
		    }
		    {
			 E apb, amb;
			 apb = a2 + b2;
			 amb = a2 - b2;
			 buf[n2 + i] = wa * amb + wb * apb; 
			 buf[n - i] = wa * apb - wb * amb; 
		    }
	       }
	  }
	  if (i + i == n2) {
	       E u, v;
	       u = I[is * (n2 - 1)];
	       v = I[is * n2];
	       buf[i] = (u + v) * (W[2*i] * K(2.0));
	       buf[n - i] = (u - v) * (W[2*i] * K(2.0));
	  }


	  /* child plan: two r2hc's of size n/2 */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  W2 = ego->td2->W;
	  { /* i == 0 case */
	       E wa, wb;
	       E a, b;
	       wa = W2[0]; /* cos */
	       wb = W2[1]; /* sin */
	       a = buf[0];
	       b = buf[n2];
	       O[0] = wa * a + wb * b;
	       O[os * (n - 1)] = wb * a - wa * b;
	  }
	  W2 += 2;
	  for (i = 1; i + i < n2; ++i, W2 += 2) {
	       INT k;
	       E u, v, u2, v2;
	       u = buf[i];
	       v = buf[n2 - i];
	       u2 = buf[n2 + i];
	       v2 = buf[n - i];
	       k = (i + i) - 1;
	       {
                    E wa, wb;
                    E a, b;
                    wa = W2[0]; /* cos */
                    wb = W2[1]; /* sin */
                    a = u - v;
                    b = v2 - u2;
                    O[os * k] = wa * a + wb * b;
                    O[os * (n - 1 - k)] = wb * a - wa * b;
               }
	       ++k;
	       W2 += 2;
	       {
		    E wa, wb;
		    E a, b;
		    wa = W2[0]; /* cos */
		    wb = W2[1]; /* sin */
		    a = u + v;
		    b = u2 + v2;
		    O[os * k] = wa * a + wb * b;
		    O[os * (n - 1 - k)] = wb * a - wa * b;
	       }
	  }
	  if (i + i == n2) {
	       INT k = (i + i) - 1;
	       E wa, wb;
	       E a, b;
	       wa = W2[0]; /* cos */
	       wb = W2[1]; /* sin */
	       a = buf[i];
	       b = buf[n2 + i];
	       O[os * k] = wa * a - wb * b;
	       O[os * (n - 1 - k)] = wb * a + wa * b;
	  }
     }

     X(ifree)(buf);
}

#if 0

/* This version of apply_re11 uses REDFT01 child plans, more similar
   to the original paper by Z. Wang.  We keep it around for reference
   (it is simpler) and because it may become more efficient if we
   ever implement REDFT01 codelets. */

static void apply_re11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = K(2.0) * I[0];
	  buf[n/2] = K(2.0) * I[is * (n - 1)];
	  for (i = 1; i + i < n; ++i) {
	       INT k = i + i;
	       E a, b;
	       a = I[is * (k - 1)];
	       b = I[is * k];
	       buf[i] = a + b;
	       buf[n - i] = a - b;
	  }

	  /* child plan: two redft01's (DCT-III) */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  W = ego->td2->W;
	  for (i = 0; i + 1 < n/2; ++i, W += 2) {
	       {
		    E wa, wb;
		    E a, b;
		    wa = W[0]; /* cos */
		    wb = W[1]; /* sin */
		    a = buf[i];
		    b = buf[n/2 + i];
		    O[os * i] = wa * a + wb * b;
		    O[os * (n - 1 - i)] = wb * a - wa * b;
	       }
	       ++i;
	       W += 2;
	       {
                    E wa, wb;
                    E a, b;
                    wa = W[0]; /* cos */
                    wb = W[1]; /* sin */
                    a = buf[i];
                    b = buf[n/2 + i];
                    O[os * i] = wa * a - wb * b;
                    O[os * (n - 1 - i)] = wb * a + wa * b;
               }
	  }
	  if (i < n/2) {
	       E wa, wb;
	       E a, b;
	       wa = W[0]; /* cos */
	       wb = W[1]; /* sin */
	       a = buf[i];
	       b = buf[n/2 + i];
	       O[os * i] = wa * a + wb * b;
	       O[os * (n - 1 - i)] = wb * a - wa * b;
	  }
     }

     X(ifree)(buf);
}

#endif /* 0 */

/* like for rodft01, rodft11 is obtained from redft11 by
   reversing the input and flipping the sign of every other output. */
static void apply_ro11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n, n2 = n/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *W2;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = K(2.0) * I[is * (n - 1)];
	  buf[n2] = K(2.0) * I[0];
	  for (i = 1; i + i < n2; ++i) {
	       INT k = i + i;
	       E a, b, a2, b2;
	       {
		    E u, v;
		    u = I[is * (n - k)];
		    v = I[is * (n - 1 - k)];
		    a = u + v;
		    b2 = u - v;
	       }
	       {
		    E u, v;
		    u = I[is * (k)];
		    v = I[is * (k - 1)];
		    b = u + v;
		    a2 = u - v;
	       }
	       {
		    E wa, wb;
		    wa = W[2*i];
		    wb = W[2*i + 1];
		    {
			 E apb, amb;
			 apb = a + b;
			 amb = a - b;
			 buf[i] = wa * amb + wb * apb; 
			 buf[n2 - i] = wa * apb - wb * amb; 
		    }
		    {
			 E apb, amb;
			 apb = a2 + b2;
			 amb = a2 - b2;
			 buf[n2 + i] = wa * amb + wb * apb; 
			 buf[n - i] = wa * apb - wb * amb; 
		    }
	       }
	  }
	  if (i + i == n2) {
	       E u, v;
	       u = I[is * n2];
	       v = I[is * (n2 - 1)];
	       buf[i] = (u + v) * (W[2*i] * K(2.0));
	       buf[n - i] = (u - v) * (W[2*i] * K(2.0));
	  }


	  /* child plan: two r2hc's of size n/2 */
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  W2 = ego->td2->W;
	  { /* i == 0 case */
	       E wa, wb;
	       E a, b;
	       wa = W2[0]; /* cos */
	       wb = W2[1]; /* sin */
	       a = buf[0];
	       b = buf[n2];
	       O[0] = wa * a + wb * b;
	       O[os * (n - 1)] = wa * b - wb * a;
	  }
	  W2 += 2;
	  for (i = 1; i + i < n2; ++i, W2 += 2) {
	       INT k;
	       E u, v, u2, v2;
	       u = buf[i];
	       v = buf[n2 - i];
	       u2 = buf[n2 + i];
	       v2 = buf[n - i];
	       k = (i + i) - 1;
	       {
                    E wa, wb;
                    E a, b;
                    wa = W2[0]; /* cos */
                    wb = W2[1]; /* sin */
                    a = v - u;
                    b = u2 - v2;
                    O[os * k] = wa * a + wb * b;
                    O[os * (n - 1 - k)] = wa * b - wb * a;
               }
	       ++k;
	       W2 += 2;
	       {
		    E wa, wb;
		    E a, b;
		    wa = W2[0]; /* cos */
		    wb = W2[1]; /* sin */
		    a = u + v;
		    b = u2 + v2;
		    O[os * k] = wa * a + wb * b;
		    O[os * (n - 1 - k)] = wa * b - wb * a;
	       }
	  }
	  if (i + i == n2) {
	       INT k = (i + i) - 1;
	       E wa, wb;
	       E a, b;
	       wa = W2[0]; /* cos */
	       wb = W2[1]; /* sin */
	       a = buf[i];
	       b = buf[n2 + i];
	       O[os * k] = wb * b - wa * a;
	       O[os * (n - 1 - k)] = wa * b + wb * a;
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
          { TW_SIN, 1, 1 },
          { TW_NEXT, 2, 0 }
     };

     X(plan_awake)(ego->cld, wakefulness);

     X(twiddle_awake)(wakefulness, &ego->td, reodft010e_tw, 
		      2*ego->n, 1, ego->n/4+1);
     X(twiddle_awake)(wakefulness, &ego->td2, reodft11e_tw, 
		      8*ego->n, 1, ego->n);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(%se-radix2-r2hc-%D%v%(%p%))",
	      X(rdft_kind_str)(ego->kind), ego->n, ego->vl, ego->cld);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1
	     && p->sz->dims[0].n % 2 == 0
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

     cld = X(mkplan_d)(plnr, X(mkproblem_rdft_1_d)(X(mktensor_1d)(n/2, 1, 1),
                                                   X(mktensor_1d)(2, n/2, n/2),
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
     ops.add = 2 + (n/2 - 1)/2 * 20;
     ops.mul = 6 + (n/2 - 1)/2 * 16;
     ops.other = 4*n + 2 + (n/2 - 1)/2 * 6;
     if ((n/2) % 2 == 0) {
	  ops.add += 4;
	  ops.mul += 8;
	  ops.other += 4;
     }

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

void X(reodft11e_radix2_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
