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


/* direct DFT solver, if we have a codelet */

#include "dft/dft.h"

typedef struct {
     solver super;
     const kdft_desc *desc;
     kdft k;
     int bufferedp;
} S;

typedef struct {
     plan_dft super;

     stride is, os, bufstride;
     INT n, vl, ivs, ovs;
     kdft k;
     const S *slv;
} P;

static void dobatch(const P *ego, R *ri, R *ii, R *ro, R *io, 
		    R *buf, INT batchsz)
{
     X(cpy2d_pair_ci)(ri, ii, buf, buf+1,
		      ego->n, WS(ego->is, 1), WS(ego->bufstride, 1),
		      batchsz, ego->ivs, 2);
     
     if (IABS(WS(ego->os, 1)) < IABS(ego->ovs)) {
	  /* transform directly to output */
	  ego->k(buf, buf+1, ro, io, 
		 ego->bufstride, ego->os, batchsz, 2, ego->ovs);
     } else {
	  /* transform to buffer and copy back */
	  ego->k(buf, buf+1, buf, buf+1, 
		 ego->bufstride, ego->bufstride, batchsz, 2, 2);
	  X(cpy2d_pair_co)(buf, buf+1, ro, io,
			   ego->n, WS(ego->bufstride, 1), WS(ego->os, 1), 
			   batchsz, 2, ego->ovs);
     }
}

static INT compute_batchsize(INT n)
{
     /* round up to multiple of 4 */
     n += 3;
     n &= -4;

     return (n + 2);
}

static void apply_buf(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     R *buf;
     INT vl = ego->vl, n = ego->n, batchsz = compute_batchsize(n);
     INT i;
     size_t bufsz = n * batchsz * 2 * sizeof(R);

     BUF_ALLOC(R *, buf, bufsz);

     for (i = 0; i < vl - batchsz; i += batchsz) {
	  dobatch(ego, ri, ii, ro, io, buf, batchsz);
	  ri += batchsz * ego->ivs; ii += batchsz * ego->ivs;
	  ro += batchsz * ego->ovs; io += batchsz * ego->ovs;
     }
     dobatch(ego, ri, ii, ro, io, buf, vl - i);

     BUF_FREE(buf, bufsz);
}

static void apply(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     ASSERT_ALIGNED_DOUBLE;
     ego->k(ri, ii, ro, io, ego->is, ego->os, ego->vl, ego->ivs, ego->ovs);
}

static void apply_extra_iter(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     INT vl = ego->vl;

     ASSERT_ALIGNED_DOUBLE;

     /* for 4-way SIMD when VL is odd: iterate over an
	even vector length VL, and then execute the last
	iteration as a 2-vector with vector stride 0. */
     ego->k(ri, ii, ro, io, ego->is, ego->os, vl - 1, ego->ivs, ego->ovs);

     ego->k(ri + (vl - 1) * ego->ivs, ii + (vl - 1) * ego->ivs,
	    ro + (vl - 1) * ego->ovs, io + (vl - 1) * ego->ovs,
	    ego->is, ego->os, 1, 0, 0);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(stride_destroy)(ego->is);
     X(stride_destroy)(ego->os);
     X(stride_destroy)(ego->bufstride);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->slv;
     const kdft_desc *d = s->desc;

     if (ego->slv->bufferedp)
	  p->print(p, "(dft-directbuf/%D-%D%v \"%s\")", 
		   compute_batchsize(d->sz), d->sz, ego->vl, d->nam);
     else
	  p->print(p, "(dft-direct-%D%v \"%s\")", d->sz, ego->vl, d->nam);
}

static int applicable_buf(const solver *ego_, const problem *p_,
			  const planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_dft *p = (const problem_dft *) p_;
     const kdft_desc *d = ego->desc;
     INT vl;
     INT ivs, ovs;
     INT batchsz;

     return (
	  1
	  && p->sz->rnk == 1
	  && p->vecsz->rnk == 1
	  && p->sz->dims[0].n == d->sz

	  /* check strides etc */
	  && X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs)

	  /* UGLY if IS <= IVS */
	  && !(NO_UGLYP(plnr) &&
	       X(iabs)(p->sz->dims[0].is) <= X(iabs)(ivs))

	  && (batchsz = compute_batchsize(d->sz), 1)
	  && (d->genus->okp(d, 0, ((const R *)0) + 1, p->ro, p->io,
			    2 * batchsz, p->sz->dims[0].os,
			    batchsz, 2, ovs, plnr))
	  && (d->genus->okp(d, 0, ((const R *)0) + 1, p->ro, p->io,
			    2 * batchsz, p->sz->dims[0].os,
			    vl % batchsz, 2, ovs, plnr))


	  && (0
	      /* can operate out-of-place */
	      || p->ri != p->ro

	      /* can operate in-place as long as strides are the same */
	      || X(tensor_inplace_strides2)(p->sz, p->vecsz)

	      /* can do it if the problem fits in the buffer, no matter
		 what the strides are */
	      || vl <= batchsz
	       )
	  );
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr, int *extra_iterp)
{
     const S *ego = (const S *) ego_;
     const problem_dft *p = (const problem_dft *) p_;
     const kdft_desc *d = ego->desc;
     INT vl;
     INT ivs, ovs;

     return (
	  1
	  && p->sz->rnk == 1
	  && p->vecsz->rnk <= 1
	  && p->sz->dims[0].n == d->sz

	  /* check strides etc */
	  && X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs)

	  && ((*extra_iterp = 0,
	       (d->genus->okp(d, p->ri, p->ii, p->ro, p->io,
			      p->sz->dims[0].is, p->sz->dims[0].os,
			      vl, ivs, ovs, plnr)))
	      ||
	      (*extra_iterp = 1,
	       ((d->genus->okp(d, p->ri, p->ii, p->ro, p->io,
			       p->sz->dims[0].is, p->sz->dims[0].os,
			       vl - 1, ivs, ovs, plnr))
		&&
		(d->genus->okp(d, p->ri, p->ii, p->ro, p->io,
			       p->sz->dims[0].is, p->sz->dims[0].os,
			       2, 0, 0, plnr)))))

	  && (0
	      /* can operate out-of-place */
	      || p->ri != p->ro

	      /* can always compute one transform */
	      || vl == 1

	      /* can operate in-place as long as strides are the same */
	      || X(tensor_inplace_strides2)(p->sz, p->vecsz)
	       )
	  );
}


static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     const problem_dft *p;
     iodim *d;
     const kdft_desc *e = ego->desc;

     static const plan_adt padt = {
	  X(dft_solve), X(null_awake), print, destroy
     };

     UNUSED(plnr);

     if (ego->bufferedp) {
	  if (!applicable_buf(ego_, p_, plnr))
	       return (plan *)0;
	  pln = MKPLAN_DFT(P, &padt, apply_buf);
     } else {
	  int extra_iterp = 0;
	  if (!applicable(ego_, p_, plnr, &extra_iterp))
	       return (plan *)0;
	  pln = MKPLAN_DFT(P, &padt, extra_iterp ? apply_extra_iter : apply);
     }

     p = (const problem_dft *) p_;
     d = p->sz->dims;
     pln->k = ego->k;
     pln->n = d[0].n;
     pln->is = X(mkstride)(pln->n, d[0].is);
     pln->os = X(mkstride)(pln->n, d[0].os);
     pln->bufstride = X(mkstride)(pln->n, 2 * compute_batchsize(pln->n));

     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);
     pln->slv = ego;

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(pln->vl / e->genus->vl, &e->ops, &pln->super.super.ops);

     if (ego->bufferedp) 
	  pln->super.super.ops.other += 4 * pln->n * pln->vl;

     pln->super.super.could_prune_now_p = !ego->bufferedp;
     return &(pln->super.super);
}

static solver *mksolver(kdft k, const kdft_desc *desc, int bufferedp)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->k = k;
     slv->desc = desc;
     slv->bufferedp = bufferedp;
     return &(slv->super);
}

solver *X(mksolver_dft_direct)(kdft k, const kdft_desc *desc)
{
     return mksolver(k, desc, 0);
}

solver *X(mksolver_dft_directbuf)(kdft k, const kdft_desc *desc)
{
     return mksolver(k, desc, 1);
}
