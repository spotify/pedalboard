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


#include "ct-hc2c.h"

typedef struct {
     hc2c_solver super;
     const hc2c_desc *desc;
     int bufferedp;
     khc2c k;
} S;

typedef struct {
     plan_hc2c super;
     khc2c k;
     plan *cld0, *cldm; /* children for 0th and middle butterflies */
     INT r, m, v, extra_iter;
     INT ms, vs;
     stride rs, brs;
     twid *td;
     const S *slv;
} P;

/*************************************************************
  Nonbuffered code
 *************************************************************/
static void apply(const plan *ego_, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld0 = (plan_rdft2 *) ego->cld0;
     plan_rdft2 *cldm = (plan_rdft2 *) ego->cldm;
     INT i, m = ego->m, v = ego->v;
     INT ms = ego->ms, vs = ego->vs;

     for (i = 0; i < v; ++i, cr += vs, ci += vs) {
	  cld0->apply((plan *) cld0, cr, ci, cr, ci);
	  ego->k(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
		 ego->td->W, ego->rs, 1, (m+1)/2, ms);
	  cldm->apply((plan *) cldm, cr + (m/2)*ms, ci + (m/2)*ms, 
		      cr + (m/2)*ms, ci + (m/2)*ms);
     }
}

static void apply_extra_iter(const plan *ego_, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld0 = (plan_rdft2 *) ego->cld0;
     plan_rdft2 *cldm = (plan_rdft2 *) ego->cldm;
     INT i, m = ego->m, v = ego->v;
     INT ms = ego->ms, vs = ego->vs;
     INT mm = (m-1)/2;

     for (i = 0; i < v; ++i, cr += vs, ci += vs) {
	  cld0->apply((plan *) cld0, cr, ci, cr, ci);

	  /* for 4-way SIMD when (m+1)/2-1 is odd: iterate over an
	     even vector length MM-1, and then execute the last
	     iteration as a 2-vector with vector stride 0.  The
	     twiddle factors of the second half of the last iteration
	     are bogus, but we only store the results of the first
	     half. */
	  ego->k(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
		 ego->td->W, ego->rs, 1, mm, ms);
	  ego->k(cr + mm*ms, ci + mm*ms, cr + (m-mm)*ms, ci + (m-mm)*ms,
		 ego->td->W, ego->rs, mm, mm+2, 0);
	  cldm->apply((plan *) cldm, cr + (m/2)*ms, ci + (m/2)*ms, 
		      cr + (m/2)*ms, ci + (m/2)*ms);
     }

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

static void dobatch(const P *ego, R *Rp, R *Ip, R *Rm, R *Im,
		    INT mb, INT me, INT extra_iter, R *bufp)
{
     INT b = WS(ego->brs, 1);
     INT rs = WS(ego->rs, 1);
     INT ms = ego->ms;
     R *bufm = bufp + b - 2;
     INT n = me - mb;

     X(cpy2d_pair_ci)(Rp + mb * ms, Ip + mb * ms, bufp, bufp + 1,
		      ego->r / 2, rs, b,
		      n, ms, 2);
     X(cpy2d_pair_ci)(Rm - mb * ms, Im - mb * ms, bufm, bufm + 1,
		      ego->r / 2, rs, b,
		      n, -ms, -2);

     if (extra_iter) {
          /* initialize the extra_iter element to 0.  It would be ok
             to leave it uninitialized, since we transform uninitialized
             data and ignore the result.  However, we want to avoid
             FP exceptions in case somebody is trapping them. */
          A(n < compute_batchsize(ego->r));
          X(zero1d_pair)(bufp + 2*n, bufp + 1 + 2*n, ego->r / 2, b);
          X(zero1d_pair)(bufm - 2*n, bufm + 1 - 2*n, ego->r / 2, b);
     }

     ego->k(bufp, bufp + 1, bufm, bufm + 1, ego->td->W, 
	    ego->brs, mb, me + extra_iter, 2);
     X(cpy2d_pair_co)(bufp, bufp + 1, Rp + mb * ms, Ip + mb * ms, 
		      ego->r / 2, b, rs,
		      n, 2, ms);
     X(cpy2d_pair_co)(bufm, bufm + 1, Rm - mb * ms, Im - mb * ms,
		      ego->r / 2, b, rs,
		      n, -2, -ms);
}

static void apply_buf(const plan *ego_, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft2 *cld0 = (plan_rdft2 *) ego->cld0;
     plan_rdft2 *cldm = (plan_rdft2 *) ego->cldm;
     INT i, j, ms = ego->ms, v = ego->v;
     INT batchsz = compute_batchsize(ego->r);
     R *buf;
     INT mb = 1, me = (ego->m+1) / 2;
     size_t bufsz = ego->r * batchsz * 2 * sizeof(R);

     BUF_ALLOC(R *, buf, bufsz);

     for (i = 0; i < v; ++i, cr += ego->vs, ci += ego->vs) {
	  R *Rp = cr;
	  R *Ip = ci;
	  R *Rm = cr + ego->m * ms;
	  R *Im = ci + ego->m * ms;

	  cld0->apply((plan *) cld0, Rp, Ip, Rp, Ip);

	  for (j = mb; j + batchsz < me; j += batchsz) 
	       dobatch(ego, Rp, Ip, Rm, Im, j, j + batchsz, 0, buf);

	  dobatch(ego, Rp, Ip, Rm, Im, j, me, ego->extra_iter, buf);

	  cldm->apply((plan *) cldm, 
		      Rp + me * ms, Ip + me * ms,
		      Rp + me * ms, Ip + me * ms);

     }

     BUF_FREE(buf, bufsz);
}

/*************************************************************
  common code
 *************************************************************/
static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;

     X(plan_awake)(ego->cld0, wakefulness);
     X(plan_awake)(ego->cldm, wakefulness);
     X(twiddle_awake)(wakefulness, &ego->td, ego->slv->desc->tw, 
		      ego->r * ego->m, ego->r, 
		      (ego->m - 1) / 2 + ego->extra_iter);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld0);
     X(plan_destroy_internal)(ego->cldm);
     X(stride_destroy)(ego->rs);
     X(stride_destroy)(ego->brs);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *slv = ego->slv;
     const hc2c_desc *e = slv->desc;

     if (slv->bufferedp)
	  p->print(p, "(hc2c-directbuf/%D-%D/%D/%D%v \"%s\"%(%p%)%(%p%))",
		   compute_batchsize(ego->r),
		   ego->r, X(twiddle_length)(ego->r, e->tw),
		   ego->extra_iter, ego->v, e->nam, 
		   ego->cld0, ego->cldm);
     else
	  p->print(p, "(hc2c-direct-%D/%D/%D%v \"%s\"%(%p%)%(%p%))",
		   ego->r, X(twiddle_length)(ego->r, e->tw), 
		   ego->extra_iter, ego->v, e->nam, 
		   ego->cld0, ego->cldm);
}

static int applicable0(const S *ego, rdft_kind kind,
		       INT r, INT rs,
		       INT m, INT ms, 
		       INT v, INT vs,
		       const R *cr, const R *ci,
		       const planner *plnr,
		       INT *extra_iter)
{
     const hc2c_desc *e = ego->desc;
     UNUSED(v);

     return (
	  1
	  && r == e->radix
	  && kind == e->genus->kind

	  /* first v-loop iteration */
	  && ((*extra_iter = 0,
	       e->genus->okp(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
			     rs, 1, (m+1)/2, ms, plnr))
              ||
	      (*extra_iter = 1,
	       ((e->genus->okp(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
			       rs, 1, (m-1)/2, ms, plnr))
		&&
		(e->genus->okp(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
			       rs, (m-1)/2, (m-1)/2 + 2, 0, plnr)))))
	  
	  /* subsequent v-loop iterations */
	  && (cr += vs, ci += vs, 1)

	  && e->genus->okp(cr + ms, ci + ms, cr + (m-1)*ms, ci + (m-1)*ms,
			   rs, 1, (m+1)/2 - *extra_iter, ms, plnr)
	  );
}

static int applicable0_buf(const S *ego, rdft_kind kind,
			   INT r, INT rs,
			   INT m, INT ms, 
			   INT v, INT vs,
			   const R *cr, const R *ci,
			   const planner *plnr, INT *extra_iter)
{
     const hc2c_desc *e = ego->desc;
     INT batchsz, brs;
     UNUSED(v); UNUSED(rs); UNUSED(ms); UNUSED(vs);

     return (
	  1
	  && r == e->radix
	  && kind == e->genus->kind

	  /* ignore cr, ci, use buffer */
	  && (cr = (const R *)0, ci = cr + 1, 
	      batchsz = compute_batchsize(r), 
	      brs = 4 * batchsz, 1)

	  && e->genus->okp(cr, ci, cr + brs - 2, ci + brs - 2, 
			   brs, 1, 1+batchsz, 2, plnr)

	  && ((*extra_iter = 0,
	       e->genus->okp(cr, ci, cr + brs - 2, ci + brs - 2, 
			     brs, 1, 1 + (((m-1)/2) % batchsz), 2, plnr))
	      ||
	      (*extra_iter = 1,
	       e->genus->okp(cr, ci, cr + brs - 2, ci + brs - 2, 
			     brs, 1, 1 + 1 + (((m-1)/2) % batchsz), 2, plnr)))
	      
	  );
}

static int applicable(const S *ego, rdft_kind kind,
		      INT r, INT rs,
		      INT m, INT ms, 
		      INT v, INT vs,
		      R *cr, R *ci,
		      const planner *plnr, INT *extra_iter)
{
     if (ego->bufferedp) {
	  if (!applicable0_buf(ego, kind, r, rs, m, ms, v, vs, cr, ci, plnr,
			       extra_iter))
	       return 0;
     } else {
	  if (!applicable0(ego, kind, r, rs, m, ms, v, vs, cr, ci, plnr,
			   extra_iter))
	       return 0;
     }

     if (NO_UGLYP(plnr) && X(ct_uglyp)((ego->bufferedp? (INT)512 : (INT)16),
				       v, m * r, r))
	  return 0;

     return 1;
}

static plan *mkcldw(const hc2c_solver *ego_, rdft_kind kind,
		    INT r, INT rs,
		    INT m, INT ms, 
		    INT v, INT vs,
		    R *cr, R *ci,
		    planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     const hc2c_desc *e = ego->desc;
     plan *cld0 = 0, *cldm = 0;
     INT imid = (m / 2) * ms;
     INT extra_iter;

     static const plan_adt padt = {
	  0, awake, print, destroy
     };

     if (!applicable(ego, kind, r, rs, m, ms, v, vs, cr, ci, plnr, 
		     &extra_iter))
          return (plan *)0;

     cld0 = X(mkplan_d)(
	  plnr, 
	  X(mkproblem_rdft2_d)(X(mktensor_1d)(r, rs, rs),
			       X(mktensor_0d)(),
			       TAINT(cr, vs), TAINT(ci, vs),
			       TAINT(cr, vs), TAINT(ci, vs),
			       kind));
     if (!cld0) goto nada;

     cldm = X(mkplan_d)(
	  plnr, 
	  X(mkproblem_rdft2_d)(((m % 2) ?
				X(mktensor_0d)() : X(mktensor_1d)(r, rs, rs) ),
			       X(mktensor_0d)(),
			       TAINT(cr + imid, vs), TAINT(ci + imid, vs),
			       TAINT(cr + imid, vs), TAINT(ci + imid, vs),
			       kind == R2HC ? R2HCII : HC2RIII));
     if (!cldm) goto nada;

     if (ego->bufferedp)
	  pln = MKPLAN_HC2C(P, &padt, apply_buf);
     else
	  pln = MKPLAN_HC2C(P, &padt, extra_iter ? apply_extra_iter : apply);

     pln->k = ego->k;
     pln->td = 0;
     pln->r = r; pln->rs = X(mkstride)(r, rs);
     pln->m = m; pln->ms = ms;
     pln->v = v; pln->vs = vs;
     pln->slv = ego;
     pln->brs = X(mkstride)(r, 4 * compute_batchsize(r));
     pln->cld0 = cld0;
     pln->cldm = cldm;
     pln->extra_iter = extra_iter;

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(v * (((m - 1) / 2) / e->genus->vl),
		  &e->ops, &pln->super.super.ops);
     X(ops_madd2)(v, &cld0->ops, &pln->super.super.ops);
     X(ops_madd2)(v, &cldm->ops, &pln->super.super.ops);

     if (ego->bufferedp) 
	  pln->super.super.ops.other += 4 * r * m * v;

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld0);
     X(plan_destroy_internal)(cldm);
     return 0;
}

static void regone(planner *plnr, khc2c codelet,
		   const hc2c_desc *desc, 
		   hc2c_kind hc2ckind, 
		   int bufferedp)
{
     S *slv = (S *)X(mksolver_hc2c)(sizeof(S), desc->radix, hc2ckind, mkcldw);
     slv->k = codelet;
     slv->desc = desc;
     slv->bufferedp = bufferedp;
     REGISTER_SOLVER(plnr, &(slv->super.super));
}

void X(regsolver_hc2c_direct)(planner *plnr, khc2c codelet,
			      const hc2c_desc *desc,
			      hc2c_kind hc2ckind)
{
     regone(plnr, codelet, desc, hc2ckind, /* bufferedp */0);
     regone(plnr, codelet, desc, hc2ckind, /* bufferedp */1);
}
