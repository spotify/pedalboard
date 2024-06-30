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


/* Do an R{E,O}DFT{01,10} problem via an R2HC problem, with some
   pre/post-processing ala FFTPACK. */

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
     rdft_kind kind;
} P;

/* A real-even-01 DFT operates logically on a size-4N array:
                   I 0 -r(I*) -I 0 r(I*),
   where r denotes reversal and * denotes deletion of the 0th element.
   To compute the transform of this, we imagine performing a radix-4
   (real-input) DIF step, which turns the size-4N DFT into 4 size-N
   (contiguous) DFTs, two of which are zero and two of which are
   conjugates.  The non-redundant size-N DFT has halfcomplex input, so
   we can do it with a size-N hc2r transform.  (In order to share
   plans with the re10 (inverse) transform, however, we use the DHT
   trick to re-express the hc2r problem as r2hc.  This has little cost
   since we are already pre- and post-processing the data in {i,n-i}
   order.)  Finally, we have to write out the data in the correct
   order...the two size-N redundant (conjugate) hc2r DFTs correspond
   to the even and odd outputs in O (i.e. the usual interleaved output
   of DIF transforms); since this data has even symmetry, we only
   write the first half of it.

   The real-even-10 DFT is just the reverse of these steps, i.e. a
   radix-4 DIT transform.  There, however, we just use the r2hc
   transform naturally without resorting to the DHT trick.

   A real-odd-01 DFT is very similar, except that the input is
   0 I (rI)* 0 -I -(rI)*.  This format, however, can be transformed
   into precisely the real-even-01 format above by sending I -> rI
   and shifting the array by N.  The former swap is just another
   transformation on the input during preprocessing; the latter
   multiplies the even/odd outputs by i/-i, which combines with
   the factor of -i (to take the imaginary part) to simply flip
   the sign of the odd outputs.  Vice-versa for real-odd-10.

   The FFTPACK source code was very helpful in working this out.
   (They do unnecessary passes over the array, though.)  The same
   algorithm is also described in:

      John Makhoul, "A fast cosine transform in one and two dimensions,"
      IEEE Trans. on Acoust. Speech and Sig. Proc., ASSP-28 (1), 27--34 (1980).

   Note that Numerical Recipes suggests a different algorithm that
   requires more operations and uses trig. functions for both the pre-
   and post-processing passes.
*/

static void apply_re01(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b, apb, amb, wa, wb;
	       a = I[is * i];
	       b = I[is * (n - i)];
	       apb = a + b;
	       amb = a - b;
	       wa = W[2*i];
	       wb = W[2*i + 1];
	       buf[i] = wa * amb + wb * apb; 
	       buf[n - i] = wa * apb - wb * amb; 
	  }
	  if (i == n - i) {
	       buf[i] = K(2.0) * I[is * i] * W[2*i];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  O[0] = buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b;
	       INT k;
	       a = buf[i];
	       b = buf[n - i];
	       k = i + i;
	       O[os * (k - 1)] = a - b;
	       O[os * k] = a + b;
	  }
	  if (i == n - i) {
	       O[os * (n - 1)] = buf[i];
	  }
     }

     X(ifree)(buf);
}

/* ro01 is same as re01, but with i <-> n - 1 - i in the input and
   the sign of the odd output elements flipped. */
static void apply_ro01(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[is * (n - 1)];
	  for (i = 1; i < n - i; ++i) {
	       E a, b, apb, amb, wa, wb;
	       a = I[is * (n - 1 - i)];
	       b = I[is * (i - 1)];
	       apb = a + b;
	       amb = a - b;
	       wa = W[2*i];
	       wb = W[2*i+1];
	       buf[i] = wa * amb + wb * apb; 
	       buf[n - i] = wa * apb - wb * amb; 
	  }
	  if (i == n - i) {
	       buf[i] = K(2.0) * I[is * (i - 1)] * W[2*i];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  O[0] = buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b;
	       INT k;
	       a = buf[i];
	       b = buf[n - i];
	       k = i + i;
	       O[os * (k - 1)] = b - a;
	       O[os * k] = a + b;
	  }
	  if (i == n - i) {
	       O[os * (n - 1)] = -buf[i];
	  }
     }

     X(ifree)(buf);
}

static void apply_re10(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[0];
	  for (i = 1; i < n - i; ++i) {
	       E u, v;
	       INT k = i + i;
	       u = I[is * (k - 1)];
	       v = I[is * k];
	       buf[n - i] = u;
	       buf[i] = v;
	  }
	  if (i == n - i) {
	       buf[i] = I[is * (n - 1)];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  O[0] = K(2.0) * buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b, wa, wb;
	       a = K(2.0) * buf[i];
	       b = K(2.0) * buf[n - i];
	       wa = W[2*i];
	       wb = W[2*i + 1];
	       O[os * i] = wa * a + wb * b;
	       O[os * (n - i)] = wb * a - wa * b;
	  }
	  if (i == n - i) {
	       O[os * i] = K(2.0) * buf[i] * W[2*i];
	  }
     }

     X(ifree)(buf);
}

/* ro10 is same as re10, but with i <-> n - 1 - i in the output and
   the sign of the odd input elements flipped. */
static void apply_ro10(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *W = ego->td->W;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  buf[0] = I[0];
	  for (i = 1; i < n - i; ++i) {
	       E u, v;
	       INT k = i + i;
	       u = -I[is * (k - 1)];
	       v = I[is * k];
	       buf[n - i] = u;
	       buf[i] = v;
	  }
	  if (i == n - i) {
	       buf[i] = -I[is * (n - 1)];
	  }
	  
	  {
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  O[os * (n - 1)] = K(2.0) * buf[0];
	  for (i = 1; i < n - i; ++i) {
	       E a, b, wa, wb;
	       a = K(2.0) * buf[i];
	       b = K(2.0) * buf[n - i];
	       wa = W[2*i];
	       wb = W[2*i + 1];
	       O[os * (n - 1 - i)] = wa * a + wb * b;
	       O[os * (i - 1)] = wb * a - wa * b;
	  }
	  if (i == n - i) {
	       O[os * (i - 1)] = K(2.0) * buf[i] * W[2*i];
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

     X(plan_awake)(ego->cld, wakefulness);

     X(twiddle_awake)(wakefulness, &ego->td, reodft010e_tw, 
		      4*ego->n, 1, ego->n/2+1);
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
	     && (p->kind[0] == REDFT01 || p->kind[0] == REDFT10
		 || p->kind[0] == RODFT01 || p->kind[0] == RODFT10)
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

     switch (p->kind[0]) {
	 case REDFT01: pln = MKPLAN_RDFT(P, &padt, apply_re01); break;
	 case REDFT10: pln = MKPLAN_RDFT(P, &padt, apply_re10); break;
	 case RODFT01: pln = MKPLAN_RDFT(P, &padt, apply_ro01); break;
	 case RODFT10: pln = MKPLAN_RDFT(P, &padt, apply_ro10); break;
	 default: A(0); return (plan*)0;
     }

     pln->n = n;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;
     pln->cld = cld;
     pln->td = 0;
     pln->kind = p->kind[0];
     
     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     
     X(ops_zero)(&ops);
     ops.other = 4 + (n-1)/2 * 10 + (1 - n % 2) * 5;
     if (p->kind[0] == REDFT01 || p->kind[0] == RODFT01) {
	  ops.add = (n-1)/2 * 6;
	  ops.mul = (n-1)/2 * 4 + (1 - n % 2) * 2;
     }
     else { /* 10 transforms */
	  ops.add = (n-1)/2 * 2;
	  ops.mul = 1 + (n-1)/2 * 6 + (1 - n % 2) * 2;
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

void X(reodft010e_r2hc_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
