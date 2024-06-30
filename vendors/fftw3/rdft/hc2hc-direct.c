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


#include "rdft/hc2hc.h"

typedef struct {
     hc2hc_solver super;
     const hc2hc_desc *desc;
     khc2hc k;
     int bufferedp;
} S;

typedef struct {
     plan_hc2hc super;
     khc2hc k;
     plan *cld0, *cldm; /* children for 0th and middle butterflies */
     INT r, m, v;
     INT ms, vs, mb, me;
     stride rs, brs;
     twid *td;
     const S *slv;
} P;

/*************************************************************
  Nonbuffered code
*************************************************************/
static void apply(const plan *ego_, R *IO)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld0 = (plan_rdft *) ego->cld0;
     plan_rdft *cldm = (plan_rdft *) ego->cldm;
     INT i, m = ego->m, v = ego->v;
     INT mb = ego->mb, me = ego->me;
     INT ms = ego->ms, vs = ego->vs;

     for (i = 0; i < v; ++i, IO += vs) {
	  cld0->apply((plan *) cld0, IO, IO);
	  ego->k(IO + ms * mb, IO + (m - mb) * ms, 
		 ego->td->W, ego->rs, mb, me, ms);
	  cldm->apply((plan *) cldm, IO + (m/2) * ms, IO + (m/2) * ms);
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

static void dobatch(const P *ego, R *IOp, R *IOm,
		    INT mb, INT me, R *bufp)
{
     INT b = WS(ego->brs, 1);
     INT rs = WS(ego->rs, 1);
     INT r = ego->r;
     INT ms = ego->ms;
     R *bufm = bufp + b - 1;

     X(cpy2d_ci)(IOp + mb * ms, bufp, r, rs, b, me - mb,  ms,  1, 1);
     X(cpy2d_ci)(IOm - mb * ms, bufm, r, rs, b, me - mb, -ms, -1, 1);

     ego->k(bufp, bufm, ego->td->W, ego->brs, mb, me, 1);

     X(cpy2d_co)(bufp, IOp + mb * ms, r, b, rs, me - mb,  1,  ms, 1);
     X(cpy2d_co)(bufm, IOm - mb * ms, r, b, rs, me - mb, -1, -ms, 1);
}

static void apply_buf(const plan *ego_, R *IO)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld0 = (plan_rdft *) ego->cld0;
     plan_rdft *cldm = (plan_rdft *) ego->cldm;
     INT i, j, m = ego->m, v = ego->v, r = ego->r;
     INT mb = ego->mb, me = ego->me, ms = ego->ms;
     INT batchsz = compute_batchsize(r);
     R *buf;
     size_t bufsz = r * batchsz * 2 * sizeof(R);

     BUF_ALLOC(R *, buf, bufsz);

     for (i = 0; i < v; ++i, IO += ego->vs) {
	  R *IOp = IO;
	  R *IOm = IO + m * ms;

	  cld0->apply((plan *) cld0, IO, IO);

	  for (j = mb; j + batchsz < me; j += batchsz) 	       
	       dobatch(ego, IOp, IOm, j, j + batchsz, buf);

	  dobatch(ego, IOp, IOm, j, me, buf);

	  cldm->apply((plan *) cldm, IO + ms * (m/2), IO + ms * (m/2));
     }

     BUF_FREE(buf, bufsz);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;

     X(plan_awake)(ego->cld0, wakefulness);
     X(plan_awake)(ego->cldm, wakefulness);
     X(twiddle_awake)(wakefulness, &ego->td, ego->slv->desc->tw, 
		      ego->r * ego->m, ego->r, (ego->m - 1) / 2);
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
     const hc2hc_desc *e = slv->desc;
     INT batchsz = compute_batchsize(ego->r);

     if (slv->bufferedp)
	  p->print(p, "(hc2hc-directbuf/%D-%D/%D%v \"%s\"%(%p%)%(%p%))",
		   batchsz, ego->r, X(twiddle_length)(ego->r, e->tw), 
		   ego->v, e->nam, ego->cld0, ego->cldm);
     else
	  p->print(p, "(hc2hc-direct-%D/%D%v \"%s\"%(%p%)%(%p%))",
		   ego->r, X(twiddle_length)(ego->r, e->tw), ego->v, e->nam,
		   ego->cld0, ego->cldm);
}

static int applicable0(const S *ego, rdft_kind kind, INT r)
{
     const hc2hc_desc *e = ego->desc;

     return (1
	     && r == e->radix
	     && kind == e->genus->kind
	  );
}

static int applicable(const S *ego, rdft_kind kind, INT r, INT m, INT v,
		      const planner *plnr)
{
     if (!applicable0(ego, kind, r))
          return 0;

     if (NO_UGLYP(plnr) && X(ct_uglyp)((ego->bufferedp? (INT)512 : (INT)16),
				       v, m * r, r)) 
	  return 0;

     return 1;
}

#define CLDMP(m, mstart, mcount) (2 * ((mstart) + (mcount)) == (m) + 2)
#define CLD0P(mstart) ((mstart) == 0)

static plan *mkcldw(const hc2hc_solver *ego_, 
		    rdft_kind kind, INT r, INT m, INT ms, INT v, INT vs, 
		    INT mstart, INT mcount,
		    R *IO, planner *plnr)
{
     const S *ego = (const S *) ego_;
     P *pln;
     const hc2hc_desc *e = ego->desc;
     plan *cld0 = 0, *cldm = 0;
     INT imid = (m / 2) * ms;
     INT rs = m * ms;

     static const plan_adt padt = {
	  0, awake, print, destroy
     };

     if (!applicable(ego, kind, r, m, v, plnr))
          return (plan *)0;

     cld0 = X(mkplan_d)(
	  plnr, 
	  X(mkproblem_rdft_1_d)((CLD0P(mstart) ?
				 X(mktensor_1d)(r, rs, rs) : X(mktensor_0d)()),
				X(mktensor_0d)(),
				TAINT(IO, vs), TAINT(IO, vs), 
				kind));
     if (!cld0) goto nada;

     cldm = X(mkplan_d)(
	  plnr, 
	  X(mkproblem_rdft_1_d)((CLDMP(m, mstart, mcount) ?
				 X(mktensor_1d)(r, rs, rs) : X(mktensor_0d)()),
				X(mktensor_0d)(),
				TAINT(IO + imid, vs), TAINT(IO + imid, vs),
				kind == R2HC ? R2HCII : HC2RIII));
     if (!cldm) goto nada;
	  
     pln = MKPLAN_HC2HC(P, &padt, ego->bufferedp ? apply_buf : apply);

     pln->k = ego->k;
     pln->td = 0;
     pln->r = r; pln->rs = X(mkstride)(r, rs);
     pln->m = m; pln->ms = ms;
     pln->v = v; pln->vs = vs;
     pln->slv = ego;
     pln->brs = X(mkstride)(r, 2 * compute_batchsize(r));
     pln->cld0 = cld0;
     pln->cldm = cldm;
     pln->mb = mstart + CLD0P(mstart);
     pln->me = mstart + mcount - CLDMP(m, mstart, mcount);

     X(ops_zero)(&pln->super.super.ops);
     X(ops_madd2)(v * ((pln->me - pln->mb) / e->genus->vl),
		  &e->ops, &pln->super.super.ops);
     X(ops_madd2)(v, &cld0->ops, &pln->super.super.ops);
     X(ops_madd2)(v, &cldm->ops, &pln->super.super.ops);

     if (ego->bufferedp) 
	  pln->super.super.ops.other += 4 * r * (pln->me - pln->mb) * v;

     pln->super.super.could_prune_now_p =
	  (!ego->bufferedp && r >= 5 && r < 64 && m >= r);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld0);
     X(plan_destroy_internal)(cldm);
     return 0;
}

static void regone(planner *plnr, khc2hc codelet, const hc2hc_desc *desc,
		   int bufferedp)
{
     S *slv = (S *)X(mksolver_hc2hc)(sizeof(S), desc->radix, mkcldw);
     slv->k = codelet;
     slv->desc = desc;
     slv->bufferedp = bufferedp;
     REGISTER_SOLVER(plnr, &(slv->super.super));
     if (X(mksolver_hc2hc_hook)) {
	  slv = (S *)X(mksolver_hc2hc_hook)(sizeof(S), desc->radix, mkcldw);
	  slv->k = codelet;
	  slv->desc = desc;
	  slv->bufferedp = bufferedp;
	  REGISTER_SOLVER(plnr, &(slv->super.super));
     }
}

void X(regsolver_hc2hc_direct)(planner *plnr, khc2hc codelet,
			       const hc2hc_desc *desc)
{
     regone(plnr, codelet, desc, /* bufferedp */0);
     regone(plnr, codelet, desc, /* bufferedp */1);
}
