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


/* buffering of rdft2.  We always buffer the complex array */

#include "rdft/rdft.h"
#include "dft/dft.h"

typedef struct {
     solver super;
     size_t maxnbuf_ndx;
} S;

static const INT maxnbufs[] = { 8, 256 };

typedef struct {
     plan_rdft2 super;

     plan *cld, *cldcpy, *cldrest;
     INT n, vl, nbuf, bufdist;
     INT ivs_by_nbuf, ovs_by_nbuf;
     INT ioffset, roffset;
} P;

/* transform a vector input with the help of bufs */
static void apply_r2hc(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld = (plan_rdft2 *) ego->cld;
     plan_dft *cldcpy = (plan_dft *) ego->cldcpy;
     INT i, vl = ego->vl, nbuf = ego->nbuf;
     INT ivs_by_nbuf = ego->ivs_by_nbuf, ovs_by_nbuf = ego->ovs_by_nbuf;
     R *bufs = (R *)MALLOC(sizeof(R) * nbuf * ego->bufdist, BUFFERS);
     R *bufr = bufs + ego->roffset;
     R *bufi = bufs + ego->ioffset;
     plan_rdft2 *cldrest;

     for (i = nbuf; i <= vl; i += nbuf) {
          /* transform to bufs: */
          cld->apply((plan *) cld, r0, r1, bufr, bufi);
	  r0 += ivs_by_nbuf; r1 += ivs_by_nbuf;

          /* copy back */
          cldcpy->apply((plan *) cldcpy, bufr, bufi, cr, ci);
	  cr += ovs_by_nbuf; ci += ovs_by_nbuf;
     }

     X(ifree)(bufs);

     /* Do the remaining transforms, if any: */
     cldrest = (plan_rdft2 *) ego->cldrest;
     cldrest->apply((plan *) cldrest, r0, r1, cr, ci);
}

/* for hc2r problems, copy the input into buffer, and then
   transform buffer->output, which allows for destruction of the
   buffer */
static void apply_hc2r(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld = (plan_rdft2 *) ego->cld;
     plan_dft *cldcpy = (plan_dft *) ego->cldcpy;
     INT i, vl = ego->vl, nbuf = ego->nbuf;
     INT ivs_by_nbuf = ego->ivs_by_nbuf, ovs_by_nbuf = ego->ovs_by_nbuf;
     R *bufs = (R *)MALLOC(sizeof(R) * nbuf * ego->bufdist, BUFFERS);
     R *bufr = bufs + ego->roffset;
     R *bufi = bufs + ego->ioffset;
     plan_rdft2 *cldrest;

     for (i = nbuf; i <= vl; i += nbuf) {
          /* copy input into bufs: */
          cldcpy->apply((plan *) cldcpy, cr, ci, bufr, bufi);
	  cr += ivs_by_nbuf; ci += ivs_by_nbuf;

          /* transform to output */
          cld->apply((plan *) cld, r0, r1, bufr, bufi);
	  r0 += ovs_by_nbuf; r1 += ovs_by_nbuf;
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
     X(plan_awake)(ego->cldcpy, wakefulness);
     X(plan_awake)(ego->cldrest, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldrest);
     X(plan_destroy_internal)(ego->cldcpy);
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(rdft2-buffered-%D%v/%D-%D%(%p%)%(%p%)%(%p%))",
              ego->n, ego->nbuf,
              ego->vl, ego->bufdist % ego->n,
              ego->cld, ego->cldcpy, ego->cldrest);
}

static int applicable0(const S *ego, const problem *p_, const planner *plnr)
{
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     iodim *d = p->sz->dims;

     if (1
	 && p->vecsz->rnk <= 1
	 && p->sz->rnk == 1

	 /* we assume even n throughout */
	 && (d[0].n % 2) == 0

	 /* and we only consider these two cases */
	 && (p->kind == R2HC || p->kind == HC2R)

	  ) {
	  INT vl, ivs, ovs;
	  X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs);

	  if (X(toobig)(d[0].n) && CONSERVE_MEMORYP(plnr))
	       return 0;

	  /* if this solver is redundant, in the sense that a solver
	     of lower index generates the same plan, then prune this
	     solver */
	  if (X(nbuf_redundant)(d[0].n, vl,
				ego->maxnbuf_ndx,
				maxnbufs, NELEM(maxnbufs)))
	       return 0;

	  if (p->r0 != p->cr) {
	       if (p->kind == HC2R) {
		    /* Allow HC2R problems only if the input is to be
		       preserved.  This solver sets NO_DESTROY_INPUT,
		       which prevents infinite loops */
		    return (NO_DESTROY_INPUTP(plnr));
	       } else {
		    /*
		      In principle, the buffered transforms might be useful
		      when working out of place.  However, in order to
		      prevent infinite loops in the planner, we require
		      that the output stride of the buffered transforms be
		      greater than 2.
		    */
		    return (d[0].os > 2);
	       }
	  }

	  /*
	   * If the problem is in place, the input/output strides must
	   * be the same or the whole thing must fit in the buffer.
	   */
	  if (X(rdft2_inplace_strides(p, RNK_MINFTY)))
	       return 1;

	  if (/* fits into buffer: */
	       ((p->vecsz->rnk == 0)
		||
		(X(nbuf)(d[0].n, p->vecsz->dims[0].n,
			 maxnbufs[ego->maxnbuf_ndx])
		 == p->vecsz->dims[0].n)))
	       return 1;
     }

     return 0;
}

static int applicable(const S *ego, const problem *p_, const planner *plnr)
{
     const problem_rdft2 *p;

     if (NO_BUFFERINGP(plnr)) return 0;

     if (!applicable0(ego, p_, plnr)) return 0;

     p = (const problem_rdft2 *) p_;
     if (p->kind == HC2R) {
	  if (NO_UGLYP(plnr)) {
	       /* UGLY if in-place and too big, since the problem
		  could be solved via transpositions */
	       if (p->r0 == p->cr && X(toobig)(p->sz->dims[0].n)) 
		    return 0;
	  }
     } else {
	  if (NO_UGLYP(plnr)) {
	       if (p->r0 != p->cr || X(toobig)(p->sz->dims[0].n))
		    return 0;
	  }
     }
     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     P *pln;
     const S *ego = (const S *)ego_;
     plan *cld = (plan *) 0;
     plan *cldcpy = (plan *) 0;
     plan *cldrest = (plan *) 0;
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     R *bufs = (R *) 0;
     INT nbuf = 0, bufdist, n, vl;
     INT ivs, ovs, ioffset, roffset, id, od;

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!applicable(ego, p_, plnr))
          goto nada;

     n = X(tensor_sz)(p->sz);
     X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs);

     nbuf = X(nbuf)(n, vl, maxnbufs[ego->maxnbuf_ndx]);
     bufdist = X(bufdist)(n + 2, vl); /* complex-side rdft2 stores N+2
					 real numbers */
     A(nbuf > 0);

     /* attempt to keep real and imaginary part in the same order,
	so as to allow optimizations in the the copy plan */
     roffset = (p->cr - p->ci > 0) ? (INT)1 : (INT)0;
     ioffset = 1 - roffset;

     /* initial allocation for the purpose of planning */
     bufs = (R *) MALLOC(sizeof(R) * nbuf * bufdist, BUFFERS);

     id = ivs * (nbuf * (vl / nbuf));
     od = ovs * (nbuf * (vl / nbuf));

     if (p->kind == R2HC) {
	  /* allow destruction of input if problem is in place */
	  cld = X(mkplan_f_d)(
	       plnr, 
	       X(mkproblem_rdft2_d)(
		    X(mktensor_1d)(n, p->sz->dims[0].is, 2),
		    X(mktensor_1d)(nbuf, ivs, bufdist),
		    TAINT(p->r0, ivs * nbuf), TAINT(p->r1, ivs * nbuf),
		    bufs + roffset, bufs + ioffset, p->kind),
	       0, 0, (p->r0 == p->cr) ? NO_DESTROY_INPUT : 0);
	  if (!cld) goto nada;

	  /* copying back from the buffer is a rank-0 DFT: */
	  cldcpy = X(mkplan_d)(
	       plnr, 
	       X(mkproblem_dft_d)(
		    X(mktensor_0d)(),
		    X(mktensor_2d)(nbuf, bufdist, ovs,
				   n/2+1, 2, p->sz->dims[0].os),
		    bufs + roffset, bufs + ioffset,
		    TAINT(p->cr, ovs * nbuf), TAINT(p->ci, ovs * nbuf) ));
	  if (!cldcpy) goto nada;

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
	  /* allow destruction of buffer */
	  cld = X(mkplan_f_d)(
	       plnr, 
	       X(mkproblem_rdft2_d)(
		    X(mktensor_1d)(n, 2, p->sz->dims[0].os),
		    X(mktensor_1d)(nbuf, bufdist, ovs),
		    TAINT(p->r0, ovs * nbuf), TAINT(p->r1, ovs * nbuf),
		    bufs + roffset, bufs + ioffset, p->kind),
	       0, 0, NO_DESTROY_INPUT);
	  if (!cld) goto nada;

	  /* copying input into buffer is a rank-0 DFT: */
	  cldcpy = X(mkplan_d)(
	       plnr, 
	       X(mkproblem_dft_d)(
		    X(mktensor_0d)(),
		    X(mktensor_2d)(nbuf, ivs, bufdist,
				   n/2+1, p->sz->dims[0].is, 2),
		    TAINT(p->cr, ivs * nbuf), TAINT(p->ci, ivs * nbuf), 
		    bufs + roffset, bufs + ioffset));
	  if (!cldcpy) goto nada;

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
     pln->cldcpy = cldcpy;
     pln->cldrest = cldrest;
     pln->n = n;
     pln->vl = vl;
     pln->ivs_by_nbuf = ivs * nbuf;
     pln->ovs_by_nbuf = ovs * nbuf;
     pln->roffset = roffset;
     pln->ioffset = ioffset;

     pln->nbuf = nbuf;
     pln->bufdist = bufdist;

     {
	  opcnt t;
	  X(ops_add)(&cld->ops, &cldcpy->ops, &t);
	  X(ops_madd)(vl / nbuf, &t, &cldrest->ops, &pln->super.super.ops);
     }

     return &(pln->super.super);

 nada:
     X(ifree0)(bufs);
     X(plan_destroy_internal)(cldrest);
     X(plan_destroy_internal)(cldcpy);
     X(plan_destroy_internal)(cld);
     return (plan *) 0;
}

static solver *mksolver(size_t maxnbuf_ndx)
{
     static const solver_adt sadt = { PROBLEM_RDFT2, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->maxnbuf_ndx = maxnbuf_ndx;
     return &(slv->super);
}

void X(rdft2_buffered_register)(planner *p)
{
     size_t i;
     for (i = 0; i < NELEM(maxnbufs); ++i)
	  REGISTER_SOLVER(p, mksolver(i));
}
