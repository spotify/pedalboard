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
     size_t maxnbuf_ndx;
} S;

static const INT maxnbufs[] = { 8, 256 };

typedef struct {
     plan_rdft super;

     plan *cld, *cldcpy, *cldrest;
     INT n, vl, nbuf, bufdist;
     INT ivs_by_nbuf, ovs_by_nbuf;
} P;

/* transform a vector input with the help of bufs */
static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld = (plan_rdft *) ego->cld;
     plan_rdft *cldcpy = (plan_rdft *) ego->cldcpy;
     plan_rdft *cldrest;
     INT i, vl = ego->vl, nbuf = ego->nbuf;
     INT ivs_by_nbuf = ego->ivs_by_nbuf, ovs_by_nbuf = ego->ovs_by_nbuf;
     R *bufs;

     bufs = (R *)MALLOC(sizeof(R) * nbuf * ego->bufdist, BUFFERS);

     for (i = nbuf; i <= vl; i += nbuf) {
          /* transform to bufs: */
          cld->apply((plan *) cld, I, bufs);
	  I += ivs_by_nbuf;

          /* copy back */
          cldcpy->apply((plan *) cldcpy, bufs, O);
	  O += ovs_by_nbuf;
     }

     X(ifree)(bufs);

     /* Do the remaining transforms, if any: */
     cldrest = (plan_rdft *) ego->cldrest;
     cldrest->apply((plan *) cldrest, I, O);
}

/* for hc2r problems, copy the input into buffer, and then
   transform buffer->output, which allows for destruction of the
   buffer */
static void apply_hc2r(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld = (plan_rdft *) ego->cld;
     plan_rdft *cldcpy = (plan_rdft *) ego->cldcpy;
     plan_rdft *cldrest;
     INT i, vl = ego->vl, nbuf = ego->nbuf;
     INT ivs_by_nbuf = ego->ivs_by_nbuf, ovs_by_nbuf = ego->ovs_by_nbuf;
     R *bufs;

     bufs = (R *)MALLOC(sizeof(R) * nbuf * ego->bufdist, BUFFERS);

     for (i = nbuf; i <= vl; i += nbuf) {
          /* copy input into bufs: */
          cldcpy->apply((plan *) cldcpy, I, bufs);
	  I += ivs_by_nbuf;

          /* transform to output */
          cld->apply((plan *) cld, bufs, O);
	  O += ovs_by_nbuf;
     }

     X(ifree)(bufs);

     /* Do the remaining transforms, if any: */
     cldrest = (plan_rdft *) ego->cldrest;
     cldrest->apply((plan *) cldrest, I, O);
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
     p->print(p, "(rdft-buffered-%D%v/%D-%D%(%p%)%(%p%)%(%p%))",
              ego->n, ego->nbuf,
              ego->vl, ego->bufdist % ego->n,
              ego->cld, ego->cldcpy, ego->cldrest);
}

static int applicable0(const S *ego, const problem *p_, const planner *plnr)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     iodim *d = p->sz->dims;

     if (1
	 && p->vecsz->rnk <= 1
	 && p->sz->rnk == 1
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

	  if (p->I != p->O) {
	       if (p->kind[0] == HC2R) {
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
		      greater than 1.
		    */
		    return (d[0].os > 1);
	       }
	  }

	  /*
	   * If the problem is in place, the input/output strides must
	   * be the same or the whole thing must fit in the buffer.
	   */
	  if (X(tensor_inplace_strides2)(p->sz, p->vecsz))
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
     const problem_rdft *p;

     if (NO_BUFFERINGP(plnr)) return 0;

     if (!applicable0(ego, p_, plnr)) return 0;

     p = (const problem_rdft *) p_;
     if (p->kind[0] == HC2R) {
	  if (NO_UGLYP(plnr)) {
	       /* UGLY if in-place and too big, since the problem
		  could be solved via transpositions */
	       if (p->I == p->O && X(toobig)(p->sz->dims[0].n)) 
		    return 0;
	  }
     } else {
	  if (NO_UGLYP(plnr)) {
	       if (p->I != p->O) return 0;
	       if (X(toobig)(p->sz->dims[0].n)) return 0;
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
     const problem_rdft *p = (const problem_rdft *) p_;
     R *bufs = (R *) 0;
     INT nbuf = 0, bufdist, n, vl;
     INT ivs, ovs;
     int hc2rp;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (!applicable(ego, p_, plnr))
          goto nada;

     n = X(tensor_sz)(p->sz);
     X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs);
     hc2rp = (p->kind[0] == HC2R);

     nbuf = X(nbuf)(n, vl, maxnbufs[ego->maxnbuf_ndx]);
     bufdist = X(bufdist)(n, vl);
     A(nbuf > 0);

     /* initial allocation for the purpose of planning */
     bufs = (R *) MALLOC(sizeof(R) * nbuf * bufdist, BUFFERS);

     if (hc2rp) {
	  /* allow destruction of buffer */
	  cld = X(mkplan_f_d)(plnr, 
			      X(mkproblem_rdft_d)(
				   X(mktensor_1d)(n, 1, p->sz->dims[0].os),
				   X(mktensor_1d)(nbuf, bufdist, ovs),
				   bufs, TAINT(p->O, ovs * nbuf), p->kind),
			      0, 0, NO_DESTROY_INPUT);
	  if (!cld) goto nada;

	  /* copying input into buffer buffer is a rank-0 transform: */
	  cldcpy = X(mkplan_d)(plnr, 
			       X(mkproblem_rdft_0_d)(
				    X(mktensor_2d)(nbuf, ivs, bufdist,
						   n, p->sz->dims[0].is, 1),
				    TAINT(p->I, ivs * nbuf), bufs));
	  if (!cldcpy) goto nada;
     } else {
	  /* allow destruction of input if problem is in place */
	  cld = X(mkplan_f_d)(plnr, 
			      X(mkproblem_rdft_d)(
				   X(mktensor_1d)(n, p->sz->dims[0].is, 1),
				   X(mktensor_1d)(nbuf, ivs, bufdist),
				   TAINT(p->I, ivs * nbuf), bufs, p->kind),
			      0, 0, (p->I == p->O) ? NO_DESTROY_INPUT : 0);
	  if (!cld) goto nada;

	  /* copying back from the buffer is a rank-0 transform: */
	  cldcpy = X(mkplan_d)(plnr, 
			       X(mkproblem_rdft_0_d)(
				    X(mktensor_2d)(nbuf, bufdist, ovs,
						   n, 1, p->sz->dims[0].os),
				    bufs, TAINT(p->O, ovs * nbuf)));
	  if (!cldcpy) goto nada;
     }

     /* deallocate buffers, let apply() allocate them for real */
     X(ifree)(bufs);
     bufs = 0;

     /* plan the leftover transforms (cldrest): */
     {
	  INT id = ivs * (nbuf * (vl / nbuf));
	  INT od = ovs * (nbuf * (vl / nbuf));
	  cldrest = X(mkplan_d)(plnr, 
				X(mkproblem_rdft_d)(
				     X(tensor_copy)(p->sz),
				     X(mktensor_1d)(vl % nbuf, ivs, ovs),
				     p->I + id, p->O + od, p->kind));
     }
     if (!cldrest) goto nada;

     pln = MKPLAN_RDFT(P, &padt, hc2rp ? apply_hc2r : apply);
     pln->cld = cld;
     pln->cldcpy = cldcpy;
     pln->cldrest = cldrest;
     pln->n = n;
     pln->vl = vl;
     pln->ivs_by_nbuf = ivs * nbuf;
     pln->ovs_by_nbuf = ovs * nbuf;

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
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->maxnbuf_ndx = maxnbuf_ndx;
     return &(slv->super);
}

void X(rdft_buffered_register)(planner *p)
{
     size_t i;
     for (i = 0; i < NELEM(maxnbufs); ++i)
	  REGISTER_SOLVER(p, mksolver(i));
}
