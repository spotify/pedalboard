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
} S;

typedef struct {
     plan_rdft2 super;

     plan *cld, *cldrest;
     INT n, vl, nbuf, bufdist;
     INT cs, ivs, ovs;
} P;

/***************************************************************************/

/* FIXME: have alternate copy functions that push a vector loop inside
   the n loops? */

/* copy halfcomplex array r (contiguous) to complex (strided) array rio/iio. */
static void hc2c(INT n, R *r, R *rio, R *iio, INT os)
{
     INT i;

     rio[0] = r[0];
     iio[0] = 0;

     for (i = 1; i + i < n; ++i) {
	  rio[i * os] = r[i];
	  iio[i * os] = r[n - i];
     }

     if (i + i == n) {	/* store the Nyquist frequency */
	  rio[i * os] = r[i];
	  iio[i * os] = K(0.0);
     }
}

/* reverse of hc2c */
static void c2hc(INT n, R *rio, R *iio, INT is, R *r)
{
     INT i;

     r[0] = rio[0];

     for (i = 1; i + i < n; ++i) {
	  r[i] = rio[i * is];
	  r[n - i] = iio[i * is];
     }

     if (i + i == n)		/* store the Nyquist frequency */
	  r[i] = rio[i * is];
}

/***************************************************************************/

static void apply_r2hc(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld = (plan_rdft *) ego->cld;
     INT i, j, vl = ego->vl, nbuf = ego->nbuf, bufdist = ego->bufdist;
     INT n = ego->n;
     INT ivs = ego->ivs, ovs = ego->ovs, os = ego->cs;
     R *bufs = (R *)MALLOC(sizeof(R) * nbuf * bufdist, BUFFERS);
     plan_rdft2 *cldrest;

     for (i = nbuf; i <= vl; i += nbuf) {
          /* transform to bufs: */
          cld->apply((plan *) cld, r0, bufs);
	  r0 += ivs * nbuf; r1 += ivs * nbuf;

          /* copy back */
	  for (j = 0; j < nbuf; ++j, cr += ovs, ci += ovs)
	       hc2c(n, bufs + j*bufdist, cr, ci, os);
     }

     X(ifree)(bufs);

     /* Do the remaining transforms, if any: */
     cldrest = (plan_rdft2 *) ego->cldrest;
     cldrest->apply((plan *) cldrest, r0, r1, cr, ci);
}

static void apply_hc2r(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld = (plan_rdft *) ego->cld;
     INT i, j, vl = ego->vl, nbuf = ego->nbuf, bufdist = ego->bufdist;
     INT n = ego->n;
     INT ivs = ego->ivs, ovs = ego->ovs, is = ego->cs;
     R *bufs = (R *)MALLOC(sizeof(R) * nbuf * bufdist, BUFFERS);
     plan_rdft2 *cldrest;

     for (i = nbuf; i <= vl; i += nbuf) {
          /* copy to bufs */
	  for (j = 0; j < nbuf; ++j, cr += ivs, ci += ivs)
	       c2hc(n, cr, ci, is, bufs + j*bufdist);

          /* transform back: */
          cld->apply((plan *) cld, bufs, r0);
	  r0 += ovs * nbuf; r1 += ovs * nbuf;
     }

     X(ifree)(bufs);

     /* Do the remaining transforms, if any: */
     cldrest = (plan_rdft2 *) ego->cldrest;
     cldrest->apply((plan *) cldrest, r0, r1, cr, ci);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;

     X(plan_awake)(ego->cld, wakefulness);
     X(plan_awake)(ego->cldrest, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldrest);
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(rdft2-rdft-%s-%D%v/%D-%D%(%p%)%(%p%))",
	      ego->super.apply == apply_r2hc ? "r2hc" : "hc2r",
              ego->n, ego->nbuf,
              ego->vl, ego->bufdist % ego->n,
              ego->cld, ego->cldrest);
}

static INT min_nbuf(const problem_rdft2 *p, INT n, INT vl)
{
     INT is, os, ivs, ovs;

     if (p->r0 != p->cr)
	  return 1;
     if (X(rdft2_inplace_strides(p, RNK_MINFTY)))
	  return 1;
     A(p->vecsz->rnk == 1); /*  rank 0 and MINFTY are inplace */

     X(rdft2_strides)(p->kind, p->sz->dims, &is, &os);
     X(rdft2_strides)(p->kind, p->vecsz->dims, &ivs, &ovs);
     
     /* handle one potentially common case: "contiguous" real and
	complex arrays, which overlap because of the differing sizes. */
     if (n * X(iabs)(is) <= X(iabs)(ivs)
	 && (n/2 + 1) * X(iabs)(os) <= X(iabs)(ovs)
	 && ( ((p->cr - p->ci) <= X(iabs)(os)) || 
	      ((p->ci - p->cr) <= X(iabs)(os)) )
	 && ivs > 0 && ovs > 0) {
	  INT vsmin = X(imin)(ivs, ovs);
	  INT vsmax = X(imax)(ivs, ovs);
	  return(((vsmax - vsmin) * vl + vsmin - 1) / vsmin);
     }

     return vl; /* punt: just buffer the whole vector */
}

static int applicable0(const problem *p_, const S *ego, const planner *plnr)
{
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     UNUSED(ego);
     return(1
	    && p->vecsz->rnk <= 1
	    && p->sz->rnk == 1

	    /* FIXME: does it make sense to do R2HCII ? */
	    && (p->kind == R2HC || p->kind == HC2R)

	    /* real strides must allow for reduction to rdft */
	    && (2 * (p->r1 - p->r0) ==
		(((p->kind == R2HC) ? p->sz->dims[0].is : p->sz->dims[0].os)))

	    && !(X(toobig)(p->sz->dims[0].n) && CONSERVE_MEMORYP(plnr))
	  );
}

static int applicable(const problem *p_, const S *ego, const planner *plnr)
{
     const problem_rdft2 *p;

     if (NO_BUFFERINGP(plnr)) return 0;

     if (!applicable0(p_, ego, plnr)) return 0;

     p = (const problem_rdft2 *) p_;
     if (NO_UGLYP(plnr)) {
	  if (p->r0 != p->cr) return 0;
	  if (X(toobig)(p->sz->dims[0].n)) return 0;
     }
     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     plan *cld = (plan *) 0;
     plan *cldrest = (plan *) 0;
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     R *bufs = (R *) 0;
     INT nbuf = 0, bufdist, n, vl;
     INT ivs, ovs, rs, id, od;

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!applicable(p_, ego, plnr))
          goto nada;

     n = p->sz->dims[0].n;
     X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs);

     nbuf = X(imax)(X(nbuf)(n, vl, 0), min_nbuf(p, n, vl));
     bufdist = X(bufdist)(n, vl);
     A(nbuf > 0);

     /* initial allocation for the purpose of planning */
     bufs = (R *) MALLOC(sizeof(R) * nbuf * bufdist, BUFFERS);

     id = ivs * (nbuf * (vl / nbuf));
     od = ovs * (nbuf * (vl / nbuf));

     if (p->kind == R2HC) {
	  cld = X(mkplan_f_d)(
	       plnr,
	       X(mkproblem_rdft_d)(
		    X(mktensor_1d)(n, p->sz->dims[0].is/2, 1),
		    X(mktensor_1d)(nbuf, ivs, bufdist),
		    TAINT(p->r0, ivs * nbuf), bufs, &p->kind),
	       0, 0, (p->r0 == p->cr) ? NO_DESTROY_INPUT : 0);
	  if (!cld) goto nada;
	  X(ifree)(bufs); bufs = 0;

	  cldrest = X(mkplan_d)(plnr, 
				X(mkproblem_rdft2_d)(
				     X(tensor_copy)(p->sz),
				     X(mktensor_1d)(vl % nbuf, ivs, ovs),
				     p->r0 + id, p->r1 + id, 
				     p->cr + od, p->ci + od,
				     p->kind));
	  if (!cldrest) goto nada;

	  pln = MKPLAN_RDFT2(P, &padt, apply_r2hc);
     } else {
	  A(p->kind == HC2R);
	  cld = X(mkplan_f_d)(
	       plnr,
	       X(mkproblem_rdft_d)(
		    X(mktensor_1d)(n, 1, p->sz->dims[0].os/2),
		    X(mktensor_1d)(nbuf, bufdist, ovs),
		    bufs, TAINT(p->r0, ovs * nbuf), &p->kind),
	       0, 0, NO_DESTROY_INPUT); /* always ok to destroy bufs */
	  if (!cld) goto nada;
	  X(ifree)(bufs); bufs = 0;

	  cldrest = X(mkplan_d)(plnr, 
				X(mkproblem_rdft2_d)(
				     X(tensor_copy)(p->sz),
				     X(mktensor_1d)(vl % nbuf, ivs, ovs),
				     p->r0 + od, p->r1 + od, 
				     p->cr + id, p->ci + id,
				     p->kind));
	  if (!cldrest) goto nada;
	  pln = MKPLAN_RDFT2(P, &padt, apply_hc2r);
     }

     pln->cld = cld;
     pln->cldrest = cldrest;
     pln->n = n;
     pln->vl = vl;
     pln->ivs = ivs;
     pln->ovs = ovs;
     X(rdft2_strides)(p->kind, &p->sz->dims[0], &rs, &pln->cs);
     pln->nbuf = nbuf;
     pln->bufdist = bufdist;

     X(ops_madd)(vl / nbuf, &cld->ops, &cldrest->ops,
		 &pln->super.super.ops);
     pln->super.super.ops.other += (p->kind == R2HC ? (n + 2) : n) * vl;

     return &(pln->super.super);

 nada:
     X(ifree0)(bufs);
     X(plan_destroy_internal)(cldrest);
     X(plan_destroy_internal)(cld);
     return (plan *) 0;
}

static solver *mksolver(void)
{
     static const solver_adt sadt = { PROBLEM_RDFT2, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     return &(slv->super);
}

void X(rdft2_rdft_register)(planner *p)
{
     REGISTER_SOLVER(p, mksolver());
}
