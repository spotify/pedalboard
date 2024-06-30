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


/* Do an R{E,O}DFT11 problem via an R2HC problem of the same *odd* size,
   with some permutations and post-processing, as described in:

     S. C. Chan and K. L. Ho, "Fast algorithms for computing the
     discrete cosine transform," IEEE Trans. Circuits Systems II:
     Analog & Digital Sig. Proc. 39 (3), 185--190 (1992).

   (For even sizes, see reodft11e-radix2.c.)  

   This algorithm is related to the 8 x n prime-factor-algorithm (PFA)
   decomposition of the size 8n "logical" DFT corresponding to the
   R{EO}DFT11.

   Aside from very confusing notation (several symbols are redefined
   from one line to the next), be aware that this paper has some
   errors.  In particular, the signs are wrong in Eqs. (34-35).  Also,
   Eqs. (36-37) should be simply C(k) = C(2k + 1 mod N), and similarly
   for S (or, equivalently, the second cases should have 2*N - 2*k - 1
   instead of N - k - 1).  Note also that in their definition of the
   DFT, similarly to FFTW's, the exponent's sign is -1, but they
   forgot to correspondingly multiply S (the sine terms) by -1.
*/

#include "reodft/reodft.h"

typedef struct {
     solver super;
} S;

typedef struct {
     plan_rdft super;
     plan *cld;
     INT is, os;
     INT n;
     INT vl;
     INT ivs, ovs;
     rdft_kind kind;
} P;

static DK(SQRT2, +1.4142135623730950488016887242096980785696718753769);

#define SGN_SET(x, i) ((i) % 2 ? -(x) : (x))

static void apply_re11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n, n2 = n/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  {
	       INT m;
	       for (i = 0, m = n2; m < n; ++i, m += 4)
		    buf[i] = I[is * m];
	       for (; m < 2 * n; ++i, m += 4)
		    buf[i] = -I[is * (2*n - m - 1)];
	       for (; m < 3 * n; ++i, m += 4)
		    buf[i] = -I[is * (m - 2*n)];
	       for (; m < 4 * n; ++i, m += 4)
		    buf[i] = I[is * (4*n - m - 1)];
	       m -= 4 * n;
	       for (; i < n; ++i, m += 4)
		    buf[i] = I[is * m];
	  }

	  { /* child plan: R2HC of size n */
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  /* FIXME: strength-reduce loop by 4 to eliminate ugly sgn_set? */
	  for (i = 0; i + i + 1 < n2; ++i) {
	       INT k = i + i + 1;
	       E c1, s1;
	       E c2, s2;
	       c1 = buf[k];
	       c2 = buf[k + 1];
	       s2 = buf[n - (k + 1)];
	       s1 = buf[n - k];
	       
	       O[os * i] = SQRT2 * (SGN_SET(c1, (i+1)/2) +
				    SGN_SET(s1, i/2));
	       O[os * (n - (i+1))] = SQRT2 * (SGN_SET(c1, (n-i)/2) -
					      SGN_SET(s1, (n-(i+1))/2));
	       
	       O[os * (n2 - (i+1))] = SQRT2 * (SGN_SET(c2, (n2-i)/2) -
					       SGN_SET(s2, (n2-(i+1))/2));
	       O[os * (n2 + (i+1))] = SQRT2 * (SGN_SET(c2, (n2+i+2)/2) +
					       SGN_SET(s2, (n2+(i+1))/2));
	  }
	  if (i + i + 1 == n2) {
	       E c, s;
	       c = buf[n2];
	       s = buf[n - n2];
	       O[os * i] = SQRT2 * (SGN_SET(c, (i+1)/2) +
				    SGN_SET(s, i/2));
	       O[os * (n - (i+1))] = SQRT2 * (SGN_SET(c, (i+2)/2) +
					      SGN_SET(s, (i+1)/2));
	  }
	  O[os * n2] = SQRT2 * SGN_SET(buf[0], (n2+1)/2);
     }

     X(ifree)(buf);
}

/* like for rodft01, rodft11 is obtained from redft11 by
   reversing the input and flipping the sign of every other output. */
static void apply_ro11(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     INT is = ego->is, os = ego->os;
     INT i, n = ego->n, n2 = n/2;
     INT iv, vl = ego->vl;
     INT ivs = ego->ivs, ovs = ego->ovs;
     R *buf;

     buf = (R *) MALLOC(sizeof(R) * n, BUFFERS);

     for (iv = 0; iv < vl; ++iv, I += ivs, O += ovs) {
	  {
	       INT m;
	       for (i = 0, m = n2; m < n; ++i, m += 4)
		    buf[i] = I[is * (n - 1 - m)];
	       for (; m < 2 * n; ++i, m += 4)
		    buf[i] = -I[is * (m - n)];
	       for (; m < 3 * n; ++i, m += 4)
		    buf[i] = -I[is * (3*n - 1 - m)];
	       for (; m < 4 * n; ++i, m += 4)
		    buf[i] = I[is * (m - 3*n)];
	       m -= 4 * n;
	       for (; i < n; ++i, m += 4)
		    buf[i] = I[is * (n - 1 - m)];
	  }

	  { /* child plan: R2HC of size n */
	       plan_rdft *cld = (plan_rdft *) ego->cld;
	       cld->apply((plan *) cld, buf, buf);
	  }
	  
	  /* FIXME: strength-reduce loop by 4 to eliminate ugly sgn_set? */
	  for (i = 0; i + i + 1 < n2; ++i) {
	       INT k = i + i + 1;
	       INT j;
	       E c1, s1;
	       E c2, s2;
	       c1 = buf[k];
	       c2 = buf[k + 1];
	       s2 = buf[n - (k + 1)];
	       s1 = buf[n - k];
	       
	       O[os * i] = SQRT2 * (SGN_SET(c1, (i+1)/2 + i) +
				    SGN_SET(s1, i/2 + i));
	       O[os * (n - (i+1))] = SQRT2 * (SGN_SET(c1, (n-i)/2 + i) -
					      SGN_SET(s1, (n-(i+1))/2 + i));
	       
	       j = n2 - (i+1);
	       O[os * j] = SQRT2 * (SGN_SET(c2, (n2-i)/2 + j) -
				    SGN_SET(s2, (n2-(i+1))/2 + j));
	       O[os * (n2 + (i+1))] = SQRT2 * (SGN_SET(c2, (n2+i+2)/2 + j) +
					       SGN_SET(s2, (n2+(i+1))/2 + j));
	  }
	  if (i + i + 1 == n2) {
	       E c, s;
	       c = buf[n2];
	       s = buf[n - n2];
	       O[os * i] = SQRT2 * (SGN_SET(c, (i+1)/2 + i) +
				    SGN_SET(s, i/2 + i));
	       O[os * (n - (i+1))] = SQRT2 * (SGN_SET(c, (i+2)/2 + i) +
					      SGN_SET(s, (i+1)/2 + i));
	  }
	  O[os * n2] = SQRT2 * SGN_SET(buf[0], (n2+1)/2 + n2);
     }

     X(ifree)(buf);
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
     p->print(p, "(%se-r2hc-odd-%D%v%(%p%))",
	      X(rdft_kind_str)(ego->kind), ego->n, ego->vl, ego->cld);
}

static int applicable0(const solver *ego_, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     UNUSED(ego_);

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1
	     && p->sz->dims[0].n % 2 == 1
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
     pln->kind = p->kind[0];
     
     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     
     X(ops_zero)(&ops);
     ops.add = n - 1;
     ops.mul = n;
     ops.other = 4*n;

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

void X(reodft11e_r2hc_odd_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
