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

/* express a twiddle problem in terms of dft + multiplication by
   twiddle factors */

#include "dft/ct.h"

typedef struct {
     ct_solver super;
     INT batchsz;
} S;

typedef struct {
     plan_dftw super;

     INT r, rs, m, ms, v, vs, mb, me;
     INT batchsz;
     plan *cld;

     triggen *t;
     const S *slv;
} P;


#define BATCHDIST(r) ((r) + 16)

/**************************************************************/
static void bytwiddle(const P *ego, INT mb, INT me, R *buf, R *rio, R *iio)
{
     INT j, k;
     INT r = ego->r, rs = ego->rs, ms = ego->ms;
     triggen *t = ego->t;
     for (j = 0; j < r; ++j) {
	  for (k = mb; k < me; ++k)
	       t->rotate(t, j * k,
			 rio[j * rs + k * ms],
			 iio[j * rs + k * ms],
			 &buf[j * 2 + 2 * BATCHDIST(r) * (k - mb) + 0]);
     }
}

static int applicable0(const S *ego,
		       INT r, INT irs, INT ors,
		       INT m, INT v,
		       INT mcount)
{
     return (1
	     && v == 1
	     && irs == ors
	     && mcount >= ego->batchsz
	     && mcount % ego->batchsz == 0
	     && r >= 64 
	     && m >= r
	  );
}

static int applicable(const S *ego,
		      INT r, INT irs, INT ors,
		      INT m, INT v,
		      INT mcount,
		      const planner *plnr)
{
     if (!applicable0(ego, r, irs, ors, m, v, mcount))
	  return 0;
     if (NO_UGLYP(plnr) && m * r < 65536)
	  return 0;

     return 1;
}

static void dobatch(const P *ego, INT mb, INT me, R *buf, R *rio, R *iio)
{
     plan_dft *cld;
     INT ms = ego->ms;

     bytwiddle(ego, mb, me, buf, rio, iio);

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, buf, buf + 1, buf, buf + 1);
     X(cpy2d_pair_co)(buf, buf + 1,
		      rio + ms * mb, iio + ms * mb,
		      me-mb, 2 * BATCHDIST(ego->r), ms,
		      ego->r, 2, ego->rs);
}

static void apply(const plan *ego_, R *rio, R *iio)
{
     const P *ego = (const P *) ego_;
     R *buf = (R *) MALLOC(sizeof(R) * 2 * BATCHDIST(ego->r) * ego->batchsz,
			   BUFFERS);
     INT m;

     for (m = ego->mb; m < ego->me; m += ego->batchsz)
	  dobatch(ego, m, m + ego->batchsz, buf, rio, iio);

     A(m == ego->me);

     X(ifree)(buf);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld, wakefulness);

     switch (wakefulness) {
	 case SLEEPY:
	      X(triggen_destroy)(ego->t); ego->t = 0;
	      break;
	 default:
	      ego->t = X(mktriggen)(AWAKE_SQRTN_TABLE, ego->r * ego->m);
	      break;
     }
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(dftw-genericbuf/%D-%D-%D%(%p%))",
	      ego->batchsz, ego->r, ego->m, ego->cld);
}

static plan *mkcldw(const ct_solver *ego_,
		    INT r, INT irs, INT ors,
		    INT m, INT ms,
		    INT v, INT ivs, INT ovs,
		    INT mstart, INT mcount,
		    R *rio, R *iio,
		    planner *plnr)
{
     const S *ego = (const S *)ego_;
     P *pln;
     plan *cld = 0;
     R *buf;

     static const plan_adt padt = {
	  0, awake, print, destroy
     };
     
     UNUSED(ivs); UNUSED(ovs); UNUSED(rio); UNUSED(iio);

     A(mstart >= 0 && mstart + mcount <= m);
     if (!applicable(ego, r, irs, ors, m, v, mcount, plnr))
          return (plan *)0;

     buf = (R *) MALLOC(sizeof(R) * 2 * BATCHDIST(r) * ego->batchsz, BUFFERS);
     cld = X(mkplan_d)(plnr,
			X(mkproblem_dft_d)(
			     X(mktensor_1d)(r, 2, 2),
			     X(mktensor_1d)(ego->batchsz,
					    2 * BATCHDIST(r),
					    2 * BATCHDIST(r)),
			     buf, buf + 1, buf, buf + 1
			     )
			);
     X(ifree)(buf);
     if (!cld) goto nada;

     pln = MKPLAN_DFTW(P, &padt, apply);
     pln->slv = ego;
     pln->cld = cld;
     pln->r = r;
     pln->m = m;
     pln->ms = ms;
     pln->rs = irs;
     pln->batchsz = ego->batchsz;
     pln->mb = mstart;
     pln->me = mstart + mcount;

     {
	  double n0 = (r - 1) * (mcount - 1);
	  pln->super.super.ops = cld->ops;
	  pln->super.super.ops.mul += 8 * n0;
	  pln->super.super.ops.add += 4 * n0;
	  pln->super.super.ops.other += 8 * n0;
     }
     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld);
     return (plan *) 0;
}

static void regsolver(planner *plnr, INT r, INT batchsz)
{
     S *slv = (S *)X(mksolver_ct)(sizeof(S), r, DECDIT, mkcldw, 0);
     slv->batchsz = batchsz;
     REGISTER_SOLVER(plnr, &(slv->super.super));

     if (X(mksolver_ct_hook)) {
	  slv = (S *)X(mksolver_ct_hook)(sizeof(S), r, DECDIT, mkcldw, 0);
	  slv->batchsz = batchsz;
	  REGISTER_SOLVER(plnr, &(slv->super.super));
     }

}

void X(ct_genericbuf_register)(planner *p)
{
     static const INT radices[] = { -1, -2, -4, -8, -16, -32, -64 };
     static const INT batchsizes[] = { 4, 8, 16, 32, 64 };
     unsigned i, j;

     for (i = 0; i < sizeof(radices) / sizeof(radices[0]); ++i)
	  for (j = 0; j < sizeof(batchsizes) / sizeof(batchsizes[0]); ++j)
	       regsolver(p, radices[i], batchsizes[j]);
}
