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


/* direct RDFT solver, using r2c codelets */

#include "rdft/rdft.h"

typedef struct {
     solver super;
     const kr2c_desc *desc;
     kr2c k;
     int bufferedp;
} S;

typedef struct {
     plan_rdft super;

     stride rs, csr, csi;
     stride brs, bcsr, bcsi;
     INT n, vl, rs0, ivs, ovs, ioffset, bioffset;
     kr2c k;
     const S *slv;
} P;

/*************************************************************
  Nonbuffered code
 *************************************************************/
static void apply_r2hc(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     ASSERT_ALIGNED_DOUBLE;
     ego->k(I, I + ego->rs0, O, O + ego->ioffset, 
	    ego->rs, ego->csr, ego->csi,
	    ego->vl, ego->ivs, ego->ovs);
}

static void apply_hc2r(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     ASSERT_ALIGNED_DOUBLE;
     ego->k(O, O + ego->rs0, I, I + ego->ioffset, 
	    ego->rs, ego->csr, ego->csi,
	    ego->vl, ego->ivs, ego->ovs);
}

/*************************************************************
  Buffered code
 *************************************************************/
/* should not be 2^k to avoid associativity conflicts */
static INT compute_batchsize(INT radix)
{
     /* round up to multiple of 4 */
     radix += 3;
     radix &= -4;

     return (radix + 2);
}

static void dobatch_r2hc(const P *ego, R *I, R *O, R *buf, INT batchsz)
{
     X(cpy2d_ci)(I, buf,
		 ego->n, ego->rs0, WS(ego->bcsr /* hack */, 1),
		 batchsz, ego->ivs, 1, 1);

     if (IABS(WS(ego->csr, 1)) < IABS(ego->ovs)) {
	  /* transform directly to output */
	  ego->k(buf, buf + WS(ego->bcsr /* hack */, 1), 
		 O, O + ego->ioffset, 
		 ego->brs, ego->csr, ego->csi,
		 batchsz, 1, ego->ovs);
     } else {
	  /* transform to buffer and copy back */
	  ego->k(buf, buf + WS(ego->bcsr /* hack */, 1), 
		 buf, buf + ego->bioffset, 
		 ego->brs, ego->bcsr, ego->bcsi,
		 batchsz, 1, 1);
	  X(cpy2d_co)(buf, O,
		      ego->n, WS(ego->bcsr, 1), WS(ego->csr, 1),  
		      batchsz, 1, ego->ovs, 1);
     }
}

static void dobatch_hc2r(const P *ego, R *I, R *O, R *buf, INT batchsz)
{
     if (IABS(WS(ego->csr, 1)) < IABS(ego->ivs)) {
	  /* transform directly from input */
	  ego->k(buf, buf + WS(ego->bcsr /* hack */, 1),
		 I, I + ego->ioffset, 
		 ego->brs, ego->csr, ego->csi,
		 batchsz, ego->ivs, 1);
     } else {
	  /* copy into buffer and transform in place */
	  X(cpy2d_ci)(I, buf,
		      ego->n, WS(ego->csr, 1), WS(ego->bcsr, 1),
		      batchsz, ego->ivs, 1, 1);
	  ego->k(buf, buf + WS(ego->bcsr /* hack */, 1),
		 buf, buf + ego->bioffset, 
		 ego->brs, ego->bcsr, ego->bcsi,
		 batchsz, 1, 1);
     }
     X(cpy2d_co)(buf, O,
		 ego->n, WS(ego->bcsr /* hack */, 1), ego->rs0,
		 batchsz, 1, ego->ovs, 1);
}

static void iterate(const P *ego, R *I, R *O,
		    void (*dobatch)(const P *ego, R *I, R *O, 
				    R *buf, INT batchsz))
{
     R *buf;
     INT vl = ego->vl;
     INT n = ego->n;
     INT i;
     INT batchsz = compute_batchsize(n);
     size_t bufsz = n * batchsz * sizeof(R);

     BUF_ALLOC(R *, buf, bufsz);

     for (i = 0; i < vl - batchsz; i += batchsz) {
	  dobatch(ego, I, O, buf, batchsz);
	  I += batchsz * ego->ivs;
	  O += batchsz * ego->ovs;
     }
     dobatch(ego, I, O, buf, vl - i);

     BUF_FREE(buf, bufsz);
}

static void apply_buf_r2hc(const plan *ego_, R *I, R *O)
{
     iterate((const P *) ego_, I, O, dobatch_r2hc);
}

static void apply_buf_hc2r(const plan *ego_, R *I, R *O)
{
     iterate((const P *) ego_, I, O, dobatch_hc2r);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(stride_destroy)(ego->rs);
     X(stride_destroy)(ego->csr);
     X(stride_destroy)(ego->csi);
     X(stride_destroy)(ego->brs);
     X(stride_destroy)(ego->bcsr);
     X(stride_destroy)(ego->bcsi);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->slv;

     if (ego->slv->bufferedp)
	  p->print(p, "(rdft-%s-directbuf/%D-r2c-%D%v \"%s\")", 
		   X(rdft_kind_str)(s->desc->genus->kind), 
		   /* hack */ WS(ego->bcsr, 1), ego->n, 
		   ego->vl, s->desc->nam);

     else 
	  p->print(p, "(rdft-%s-direct-r2c-%D%v \"%s\")", 
		   X(rdft_kind_str)(s->desc->genus->kind), ego->n, 
		   ego->vl, s->desc->nam);
}

static INT ioffset(rdft_kind kind, INT sz, INT s)
{
     return(s * ((kind == R2HC || kind == HC2R) ? sz : (sz - 1)));
}

static int applicable(const solver *ego_, const problem *p_)
{
     const S *ego = (const S *) ego_;
     const kr2c_desc *desc = ego->desc;
     const problem_rdft *p = (const problem_rdft *) p_;
     INT vl, ivs, ovs;

     return (
	  1
	  && p->sz->rnk == 1
	  && p->vecsz->rnk <= 1
	  && p->sz->dims[0].n == desc->n
	  && p->kind[0] == desc->genus->kind

	  /* check strides etc */
	  && X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs)

	  && (0
	      /* can operate out-of-place */
	      || p->I != p->O

	      /* computing one transform */
	      || vl == 1

	      /* can operate in-place as long as strides are the same */
	      || X(tensor_inplace_strides2)(p->sz, p->vecsz)
	       )
	  );
}

static int applicable_buf(const solver *ego_, const problem *p_)
{
     const S *ego = (const S *) ego_;
     const kr2c_desc *desc = ego->desc;
     const problem_rdft *p = (const problem_rdft *) p_;
     INT vl, ivs, ovs, batchsz;

     return (
	  1
	  && p->sz->rnk == 1
	  && p->vecsz->rnk <= 1
	  && p->sz->dims[0].n == desc->n
	  && p->kind[0] == desc->genus->kind

	  /* check strides etc */
	  && X(tensor_tornk1)(p->vecsz, &vl, &ivs, &ovs)

	  && (batchsz = compute_batchsize(desc->n), 1)

	  && (0
	      /* can operate out-of-place */
	      || p->I != p->O

	      /* can operate in-place as long as strides are the same */
	      || X(tensor_inplace_strides2)(p->sz, p->vecsz)

	      /* can do it if the problem fits in the buffer, no matter
		 what the strides are */
	      || vl <= batchsz
	       )
	  );
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     const problem_rdft *p;
     iodim *d;
     INT rs, cs, b, n;

     static const plan_adt padt = {
	  X(rdft_solve), X(null_awake), print, destroy
     };

     UNUSED(plnr);

     if (ego->bufferedp) {
	  if (!applicable_buf(ego_, p_))
	       return (plan *)0;
     } else {
	  if (!applicable(ego_, p_))
	       return (plan *)0;
     }

     p = (const problem_rdft *) p_;

     if (R2HC_KINDP(p->kind[0])) {
	  rs = p->sz->dims[0].is; cs = p->sz->dims[0].os;
	  pln = MKPLAN_RDFT(P, &padt, 
			    ego->bufferedp ? apply_buf_r2hc : apply_r2hc);
     } else {
	  rs = p->sz->dims[0].os; cs = p->sz->dims[0].is;
	  pln = MKPLAN_RDFT(P, &padt, 
			    ego->bufferedp ? apply_buf_hc2r : apply_hc2r);
     }

     d = p->sz->dims;
     n = d[0].n;

     pln->k = ego->k;
     pln->n = n;

     pln->rs0 = rs;
     pln->rs = X(mkstride)(n, 2 * rs);
     pln->csr = X(mkstride)(n, cs);
     pln->csi = X(mkstride)(n, -cs);
     pln->ioffset = ioffset(p->kind[0], n, cs);

     b = compute_batchsize(n);
     pln->brs = X(mkstride)(n, 2 * b);
     pln->bcsr = X(mkstride)(n, b);
     pln->bcsi = X(mkstride)(n, -b);
     pln->bioffset = ioffset(p->kind[0], n, b);

     X(tensor_tornk1)(p->vecsz, &pln->vl, &pln->ivs, &pln->ovs);

     pln->slv = ego;
     X(ops_zero)(&pln->super.super.ops);

     X(ops_madd2)(pln->vl / ego->desc->genus->vl,
		  &ego->desc->ops,
		  &pln->super.super.ops);

     if (ego->bufferedp) 
	  pln->super.super.ops.other += 2 * n * pln->vl;

     pln->super.super.could_prune_now_p = !ego->bufferedp;

     return &(pln->super.super);
}

/* constructor */
static solver *mksolver(kr2c k, const kr2c_desc *desc, int bufferedp)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->k = k;
     slv->desc = desc;
     slv->bufferedp = bufferedp;
     return &(slv->super);
}

solver *X(mksolver_rdft_r2c_direct)(kr2c k, const kr2c_desc *desc)
{
     return mksolver(k, desc, 0);
}

solver *X(mksolver_rdft_r2c_directbuf)(kr2c k, const kr2c_desc *desc)
{
     return mksolver(k, desc, 1);
}
