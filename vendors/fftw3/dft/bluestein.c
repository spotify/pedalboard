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
     INT n;     /* problem size */
     INT nb;    /* size of convolution */
     R *w;      /* lambda k . exp(2*pi*i*k^2/(2*n)) */
     R *W;      /* DFT(w) */
     plan *cldf;
     INT is, os;
} P;

static void bluestein_sequence(enum wakefulness wakefulness, INT n, R *w)
{
     INT k, ksq, n2 = 2 * n;
     triggen *t = X(mktriggen)(wakefulness, n2);

     ksq = 0;
     for (k = 0; k < n; ++k) {
	  t->cexp(t, ksq, w+2*k);
          /* careful with overflow */
          ksq += 2*k + 1; while (ksq > n2) ksq -= n2;
     }

     X(triggen_destroy)(t);
}

static void mktwiddle(enum wakefulness wakefulness, P *p)
{
     INT i;
     INT n = p->n, nb = p->nb;
     R *w, *W;
     E nbf = (E)nb;

     p->w = w = (R *) MALLOC(2 * n * sizeof(R), TWIDDLES);
     p->W = W = (R *) MALLOC(2 * nb * sizeof(R), TWIDDLES);

     bluestein_sequence(wakefulness, n, w);

     for (i = 0; i < nb; ++i)
          W[2*i] = W[2*i+1] = K(0.0);

     W[0] = w[0] / nbf;
     W[1] = w[1] / nbf;

     for (i = 1; i < n; ++i) {
          W[2*i] = W[2*(nb-i)] = w[2*i] / nbf;
          W[2*i+1] = W[2*(nb-i)+1] = w[2*i+1] / nbf;
     }

     {
          plan_dft *cldf = (plan_dft *)p->cldf;
	  /* cldf must be awake */
          cldf->apply(p->cldf, W, W+1, W, W+1);
     }
}

static void apply(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT i, n = ego->n, nb = ego->nb, is = ego->is, os = ego->os;
     R *w = ego->w, *W = ego->W;
     R *b = (R *) MALLOC(2 * nb * sizeof(R), BUFFERS);

     /* multiply input by conjugate bluestein sequence */
     for (i = 0; i < n; ++i) {
	  E xr = ri[i*is], xi = ii[i*is];
          E wr = w[2*i], wi = w[2*i+1];
          b[2*i] = xr * wr + xi * wi;
          b[2*i+1] = xi * wr - xr * wi;
     }

     for (; i < nb; ++i) b[2*i] = b[2*i+1] = K(0.0);

     /* convolution: FFT */
     {
          plan_dft *cldf = (plan_dft *)ego->cldf;
          cldf->apply(ego->cldf, b, b+1, b, b+1);
     }

     /* convolution: pointwise multiplication */
     for (i = 0; i < nb; ++i) {
	  E xr = b[2*i], xi = b[2*i+1];
          E wr = W[2*i], wi = W[2*i+1];
          b[2*i] = xi * wr + xr * wi;
          b[2*i+1] = xr * wr - xi * wi;
     }

     /* convolution: IFFT by FFT with real/imag input/output swapped */
     {
          plan_dft *cldf = (plan_dft *)ego->cldf;
          cldf->apply(ego->cldf, b, b+1, b, b+1);
     }

     /* multiply output by conjugate bluestein sequence */
     for (i = 0; i < n; ++i) {
	  E xi = b[2*i], xr = b[2*i+1];
          E wr = w[2*i], wi = w[2*i+1];
          ro[i*os] = xr * wr + xi * wi;
          io[i*os] = xi * wr - xr * wi;
     }

     X(ifree)(b);	  
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;

     X(plan_awake)(ego->cldf, wakefulness);

     switch (wakefulness) {
	 case SLEEPY:
	      X(ifree0)(ego->w); ego->w = 0;
	      X(ifree0)(ego->W); ego->W = 0;
	      break;
	 default:
	      A(!ego->w);
	      mktwiddle(wakefulness, ego);
	      break;
     }
}

static int applicable(const solver *ego, const problem *p_, 
		      const planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     UNUSED(ego);
     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk == 0
	     /* FIXME: allow other sizes */
	     && X(is_prime)(p->sz->dims[0].n)

	     /* FIXME: avoid infinite recursion of bluestein with itself.
		This works because all factors in child problems are 2, 3, 5 */
	     && p->sz->dims[0].n > 16

	     && CIMPLIES(NO_SLOWP(plnr), p->sz->dims[0].n > BLUESTEIN_MAX_SLOW)
	  );
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldf);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *)ego_;
     p->print(p, "(dft-bluestein-%D/%D%(%p%))",
              ego->n, ego->nb, ego->cldf);
}

static INT choose_transform_size(INT minsz)
{
     while (!X(factors_into_small_primes)(minsz))
	  ++minsz;
     return minsz;
}

static plan *mkplan(const solver *ego, const problem *p_, planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     P *pln;
     INT n, nb;
     plan *cldf = 0;
     R *buf = (R *) 0;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if (!applicable(ego, p_, plnr))
	  return (plan *) 0;

     n = p->sz->dims[0].n;
     nb = choose_transform_size(2 * n - 1);
     buf = (R *) MALLOC(2 * nb * sizeof(R), BUFFERS);

     cldf = X(mkplan_f_d)(plnr, 
			  X(mkproblem_dft_d)(X(mktensor_1d)(nb, 2, 2),
					     X(mktensor_1d)(1, 0, 0),
					     buf, buf+1, 
					     buf, buf+1),
			  NO_SLOW, 0, 0);
     if (!cldf) goto nada;

     X(ifree)(buf);

     pln = MKPLAN_DFT(P, &padt, apply);

     pln->n = n;
     pln->nb = nb;
     pln->w = 0;
     pln->W = 0;
     pln->cldf = cldf;
     pln->is = p->sz->dims[0].is;
     pln->os = p->sz->dims[0].os;

     X(ops_add)(&cldf->ops, &cldf->ops, &pln->super.super.ops);
     pln->super.super.ops.add += 4 * n + 2 * nb;
     pln->super.super.ops.mul += 8 * n + 4 * nb;
     pln->super.super.ops.other += 6 * (n + nb);

     return &(pln->super.super);

 nada:
     X(ifree0)(buf);
     X(plan_destroy_internal)(cldf);
     return (plan *)0;
}


static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(dft_bluestein_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
