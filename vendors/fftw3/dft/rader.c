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

/*
 * Compute transforms of prime sizes using Rader's trick: turn them
 * into convolutions of size n - 1, which you then perform via a pair
 * of FFTs.
 */

typedef struct {
     solver super;
} S;

typedef struct {
     plan_dft super;

     plan *cld1, *cld2;
     R *omega;
     INT n, g, ginv;
     INT is, os;
     plan *cld_omega;
} P;

static rader_tl *omegas = 0;

static R *mkomega(enum wakefulness wakefulness, plan *p_, INT n, INT ginv)
{
     plan_dft *p = (plan_dft *) p_;
     R *omega;
     INT i, gpower;
     trigreal scale;
     triggen *t;

     if ((omega = X(rader_tl_find)(n, n, ginv, omegas)))
	  return omega;

     omega = (R *)MALLOC(sizeof(R) * (n - 1) * 2, TWIDDLES);

     scale = n - 1.0; /* normalization for convolution */

     t = X(mktriggen)(wakefulness, n);
     for (i = 0, gpower = 1; i < n-1; ++i, gpower = MULMOD(gpower, ginv, n)) {
	  trigreal w[2];
	  t->cexpl(t, gpower, w);
	  omega[2*i] = w[0] / scale;
	  omega[2*i+1] = FFT_SIGN * w[1] / scale;
     }
     X(triggen_destroy)(t);
     A(gpower == 1);

     p->apply(p_, omega, omega + 1, omega, omega + 1);

     X(rader_tl_insert)(n, n, ginv, omega, &omegas);
     return omega;
}

static void free_omega(R *omega)
{
     X(rader_tl_delete)(omega, &omegas);
}


/***************************************************************************/

/* Below, we extensively use the identity that fft(x*)* = ifft(x) in
   order to share data between forward and backward transforms and to
   obviate the necessity of having separate forward and backward
   plans.  (Although we often compute separate plans these days anyway
   due to the differing strides, etcetera.)

   Of course, since the new FFTW gives us separate pointers to
   the real and imaginary parts, we could have instead used the
   fft(r,i) = ifft(i,r) form of this identity, but it was easier to
   reuse the code from our old version. */

static void apply(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT is, os;
     INT k, gpower, g, r;
     R *buf;
     R r0 = ri[0], i0 = ii[0];

     r = ego->n; is = ego->is; os = ego->os; g = ego->g; 
     buf = (R *) MALLOC(sizeof(R) * (r - 1) * 2, BUFFERS);

     /* First, permute the input, storing in buf: */
     for (gpower = 1, k = 0; k < r - 1; ++k, gpower = MULMOD(gpower, g, r)) {
	  R rA, iA;
	  rA = ri[gpower * is];
	  iA = ii[gpower * is];
	  buf[2*k] = rA; buf[2*k + 1] = iA;
     }
     /* gpower == g^(r-1) mod r == 1 */;


     /* compute DFT of buf, storing in output (except DC): */
     {
	    plan_dft *cld = (plan_dft *) ego->cld1;
	    cld->apply(ego->cld1, buf, buf+1, ro+os, io+os);
     }

     /* set output DC component: */
     {
	  ro[0] = r0 + ro[os];
	  io[0] = i0 + io[os];
     }

     /* now, multiply by omega: */
     {
	  const R *omega = ego->omega;
	  for (k = 0; k < r - 1; ++k) {
	       E rB, iB, rW, iW;
	       rW = omega[2*k];
	       iW = omega[2*k+1];
	       rB = ro[(k+1)*os];
	       iB = io[(k+1)*os];
	       ro[(k+1)*os] = rW * rB - iW * iB;
	       io[(k+1)*os] = -(rW * iB + iW * rB);
	  }
     }
     
     /* this will add input[0] to all of the outputs after the ifft */
     ro[os] += r0;
     io[os] -= i0;

     /* inverse FFT: */
     {
	    plan_dft *cld = (plan_dft *) ego->cld2;
	    cld->apply(ego->cld2, ro+os, io+os, buf, buf+1);
     }
     
     /* finally, do inverse permutation to unshuffle the output: */
     {
	  INT ginv = ego->ginv;
	  gpower = 1;
	  for (k = 0; k < r - 1; ++k, gpower = MULMOD(gpower, ginv, r)) {
	       ro[gpower * os] = buf[2*k];
	       io[gpower * os] = -buf[2*k+1];
	  }
	  A(gpower == 1);
     }


     X(ifree)(buf);
}

/***************************************************************************/

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;

     X(plan_awake)(ego->cld1, wakefulness);
     X(plan_awake)(ego->cld2, wakefulness);
     X(plan_awake)(ego->cld_omega, wakefulness);

     switch (wakefulness) {
	 case SLEEPY:
	      free_omega(ego->omega);
	      ego->omega = 0;
	      break;
	 default:
	      ego->g = X(find_generator)(ego->n);
	      ego->ginv = X(power_mod)(ego->g, ego->n - 2, ego->n);
	      A(MULMOD(ego->g, ego->ginv, ego->n) == 1);

	      ego->omega = mkomega(wakefulness,
				   ego->cld_omega, ego->n, ego->ginv);
	      break;
     }
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld_omega);
     X(plan_destroy_internal)(ego->cld2);
     X(plan_destroy_internal)(ego->cld1);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *)ego_;
     p->print(p, "(dft-rader-%D%ois=%oos=%(%p%)",
              ego->n, ego->is, ego->os, ego->cld1);
     if (ego->cld2 != ego->cld1)
          p->print(p, "%(%p%)", ego->cld2);
     if (ego->cld_omega != ego->cld1 && ego->cld_omega != ego->cld2)
          p->print(p, "%(%p%)", ego->cld_omega);
     p->putchr(p, ')');
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     UNUSED(ego_);
     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     && CIMPLIES(NO_SLOWP(plnr), p->sz->dims[0].n > RADER_MAX_SLOW)
	     && X(is_prime)(p->sz->dims[0].n)

	     /* proclaim the solver SLOW if p-1 is not easily factorizable.
		Bluestein should take care of this case. */
	     && CIMPLIES(NO_SLOWP(plnr), X(factors_into_small_primes)(p->sz->dims[0].n - 1))
	  );
}

static int mkP(P *pln, INT n, INT is, INT os, R *ro, R *io,
	       planner *plnr)
{
     plan *cld1 = (plan *) 0;
     plan *cld2 = (plan *) 0;
     plan *cld_omega = (plan *) 0;
     R *buf = (R *) 0;

     /* initial allocation for the purpose of planning */
     buf = (R *) MALLOC(sizeof(R) * (n - 1) * 2, BUFFERS);

     cld1 = X(mkplan_f_d)(plnr, 
			  X(mkproblem_dft_d)(X(mktensor_1d)(n - 1, 2, os),
					     X(mktensor_1d)(1, 0, 0),
					     buf, buf + 1, ro + os, io + os),
			  NO_SLOW, 0, 0);
     if (!cld1) goto nada;

     cld2 = X(mkplan_f_d)(plnr, 
			  X(mkproblem_dft_d)(X(mktensor_1d)(n - 1, os, 2),
					     X(mktensor_1d)(1, 0, 0),
					     ro + os, io + os, buf, buf + 1),
			  NO_SLOW, 0, 0);

     if (!cld2) goto nada;

     /* plan for omega array */
     cld_omega = X(mkplan_f_d)(plnr, 
			       X(mkproblem_dft_d)(X(mktensor_1d)(n - 1, 2, 2),
						  X(mktensor_1d)(1, 0, 0),
						  buf, buf + 1, buf, buf + 1),
			       NO_SLOW, ESTIMATE, 0);
     if (!cld_omega) goto nada;

     /* deallocate buffers; let awake() or apply() allocate them for real */
     X(ifree)(buf);
     buf = 0;

     pln->cld1 = cld1;
     pln->cld2 = cld2;
     pln->cld_omega = cld_omega;
     pln->omega = 0;
     pln->n = n;
     pln->is = is;
     pln->os = os;

     X(ops_add)(&cld1->ops, &cld2->ops, &pln->super.super.ops);
     pln->super.super.ops.other += (n - 1) * (4 * 2 + 6) + 6;
     pln->super.super.ops.add += (n - 1) * 2 + 4;
     pln->super.super.ops.mul += (n - 1) * 4;

     return 1;

 nada:
     X(ifree0)(buf);
     X(plan_destroy_internal)(cld_omega);
     X(plan_destroy_internal)(cld2);
     X(plan_destroy_internal)(cld1);
     return 0;
}

static plan *mkplan(const solver *ego, const problem *p_, planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     P *pln;
     INT n;
     INT is, os;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if (!applicable(ego, p_, plnr))
	  return (plan *) 0;

     n = p->sz->dims[0].n;
     is = p->sz->dims[0].is;
     os = p->sz->dims[0].os;

     pln = MKPLAN_DFT(P, &padt, apply);
     if (!mkP(pln, n, is, os, p->ro, p->io, plnr)) {
	  X(ifree)(pln);
	  return (plan *) 0;
     }
     return &(pln->super.super);
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(dft_rader_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
