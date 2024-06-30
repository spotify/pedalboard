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

/* express a hc2hc problem in terms of rdft + multiplication by
   twiddle factors */

#include "rdft/hc2hc.h"

typedef hc2hc_solver S;

typedef struct {
     plan_hc2hc super;

     INT r, m, s, vl, vs, mstart1, mcount1;
     plan *cld0;
     plan *cld;
     twid *td;
} P;


/**************************************************************/
static void mktwiddle(P *ego, enum wakefulness wakefulness)
{
     static const tw_instr tw[] = { { TW_HALF, 0, 0 }, { TW_NEXT, 1, 0 } };

     /* note that R and M are swapped, to allow for sequential
	access both to data and twiddles */
     X(twiddle_awake)(wakefulness, &ego->td, tw, 
		      ego->r * ego->m, ego->m, ego->r);
}

static void bytwiddle(const P *ego, R *IO, R sign)
{
     INT i, j, k;
     INT r = ego->r, m = ego->m, s = ego->s, vl = ego->vl, vs = ego->vs;
     INT ms = m * s;
     INT mstart1 = ego->mstart1, mcount1 = ego->mcount1;
     INT wrem = 2 * ((m-1)/2 - mcount1);

     for (i = 0; i < vl; ++i, IO += vs) {
	  const R *W = ego->td->W;

	  A(m % 2 == 1);
	  for (k = 1, W += (m - 1) + 2*(mstart1-1); k < r; ++k) {
	       /* pr := IO + (j + mstart1) * s + k * ms */
	       R *pr = IO + mstart1 * s + k * ms;

	       /* pi := IO + (m - j - mstart1) * s + k * ms */
	       R *pi = IO - mstart1 * s + (k + 1) * ms;

	       for (j = 0; j < mcount1; ++j, pr += s, pi -= s) {
		    E xr = *pr;
		    E xi = *pi;
		    E wr = W[0];
		    E wi = sign * W[1];
		    *pr = xr * wr - xi * wi;
		    *pi = xi * wr + xr * wi;
		    W += 2;
	       }
	       W += wrem;
	  }
     }
}

static void swapri(R *IO, INT r, INT m, INT s, INT jstart, INT jend)
{
     INT k;
     INT ms = m * s;
     INT js = jstart * s;
     for (k = 0; k + k < r; ++k) {
	  /* pr := IO + (m - j) * s + k * ms */
	  R *pr = IO + (k + 1) * ms - js;
	  /* pi := IO + (m - j) * s + (r - 1 - k) * ms */
	  R *pi = IO + (r - k) * ms - js;
	  INT j;
	  for (j = jstart; j < jend; j += 1, pr -= s, pi -= s) {
	       R t = *pr;
	       *pr = *pi;
	       *pi = t;
	  }
     }
}

static void reorder_dit(const P *ego, R *IO)
{
     INT i, k;
     INT r = ego->r, m = ego->m, s = ego->s, vl = ego->vl, vs = ego->vs;
     INT ms = m * s;
     INT mstart1 = ego->mstart1, mend1 = mstart1 + ego->mcount1;

     for (i = 0; i < vl; ++i, IO += vs) {
	  for (k = 1; k + k < r; ++k) {
	       R *p0 = IO + k * ms;
	       R *p1 = IO + (r - k) * ms;
	       INT j;

	       for (j = mstart1; j < mend1; ++j) {
		    E rp, ip, im, rm;
		    rp = p0[j * s];
		    im = p1[ms - j * s];
		    rm = p1[j * s];
		    ip = p0[ms - j * s];
		    p0[j * s] = rp - im;
		    p1[ms - j * s] = rp + im;
		    p1[j * s] = rm - ip;
		    p0[ms - j * s] = ip + rm;
	       }
	  }

	  swapri(IO, r, m, s, mstart1, mend1);
     }
}

static void reorder_dif(const P *ego, R *IO)
{
     INT i, k;
     INT r = ego->r, m = ego->m, s = ego->s, vl = ego->vl, vs = ego->vs;
     INT ms = m * s;
     INT mstart1 = ego->mstart1, mend1 = mstart1 + ego->mcount1;

     for (i = 0; i < vl; ++i, IO += vs) {
	  swapri(IO, r, m, s, mstart1, mend1);

	  for (k = 1; k + k < r; ++k) {
	       R *p0 = IO + k * ms;
	       R *p1 = IO + (r - k) * ms;
	       const R half = K(0.5);
	       INT j;

	       for (j = mstart1; j < mend1; ++j) {
		    E rp, ip, im, rm;
		    rp = half * p0[j * s];
		    im = half * p1[ms - j * s];
		    rm = half * p1[j * s];
		    ip = half * p0[ms - j * s];
		    p0[j * s] = rp + im;
		    p1[ms - j * s] = im - rp;
		    p1[j * s] = rm + ip;
		    p0[ms - j * s] = ip - rm;
	       }
	  }
     }
}

static int applicable(rdft_kind kind, INT r, INT m, const planner *plnr)
{
     return (1 
	     && (kind == R2HC || kind == HC2R)
	     && (m % 2)
	     && (r % 2)
	     && !NO_SLOWP(plnr)
	  );
}

/**************************************************************/

static void apply_dit(const plan *ego_, R *IO)
{
     const P *ego = (const P *) ego_;
     INT start;
     plan_rdft *cld, *cld0;

     bytwiddle(ego, IO, K(-1.0));

     cld0 = (plan_rdft *) ego->cld0;
     cld0->apply(ego->cld0, IO, IO);

     start = ego->mstart1 * ego->s;
     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, IO + start, IO + start);

     reorder_dit(ego, IO);
}

static void apply_dif(const plan *ego_, R *IO)
{
     const P *ego = (const P *) ego_;
     INT start;
     plan_rdft *cld, *cld0;

     reorder_dif(ego, IO);

     cld0 = (plan_rdft *) ego->cld0;
     cld0->apply(ego->cld0, IO, IO);

     start = ego->mstart1 * ego->s;
     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, IO + start, IO + start);

     bytwiddle(ego, IO, K(1.0));
}


static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld0, wakefulness);
     X(plan_awake)(ego->cld, wakefulness);
     mktwiddle(ego, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cld);
     X(plan_destroy_internal)(ego->cld0);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(hc2hc-generic-%s-%D-%D%v%(%p%)%(%p%))", 
	      ego->super.apply == apply_dit ? "dit" : "dif",
	      ego->r, ego->m, ego->vl, ego->cld0, ego->cld);
}

static plan *mkcldw(const hc2hc_solver *ego_, 
		    rdft_kind kind, INT r, INT m, INT s, INT vl, INT vs, 
		    INT mstart, INT mcount,
		    R *IO, planner *plnr)
{
     P *pln;
     plan *cld0 = 0, *cld = 0;
     INT mstart1, mcount1, mstride;

     static const plan_adt padt = {
	  0, awake, print, destroy
     };

     UNUSED(ego_);

     A(mstart >= 0 && mcount > 0 && mstart + mcount <= (m+2)/2);

     if (!applicable(kind, r, m, plnr))
          return (plan *)0;

     A(m % 2);
     mstart1 = mstart + (mstart == 0);
     mcount1 = mcount - (mstart == 0);
     mstride = m - (mstart + mcount - 1) - mstart1;

     /* 0th (DC) transform (vl of these), if mstart == 0 */
     cld0 = X(mkplan_d)(plnr, 
			X(mkproblem_rdft_1_d)(
			     mstart == 0 ? X(mktensor_1d)(r, m * s, m * s)
			     : X(mktensor_0d)(),
			     X(mktensor_1d)(vl, vs, vs),
			     IO, IO, kind)
			);
     if (!cld0) goto nada;

     /* twiddle transforms: there are 2 x mcount1 x vl of these
	(where 2 corresponds to the real and imaginary parts) ...
        the 2 x mcount1 loops are combined if mstart=0 and mcount=(m+2)/2. */
     cld = X(mkplan_d)(plnr, 
			X(mkproblem_rdft_1_d)(
			     X(mktensor_1d)(r, m * s, m * s),
			     X(mktensor_3d)(2, mstride * s, mstride * s,
					    mcount1, s, s, 
					    vl, vs, vs),
			     IO + s * mstart1, IO + s * mstart1, kind)
	                );
     if (!cld) goto nada;
     
     pln = MKPLAN_HC2HC(P, &padt, (kind == R2HC) ? apply_dit : apply_dif);
     pln->cld = cld;
     pln->cld0 = cld0;
     pln->r = r;
     pln->m = m;
     pln->s = s;
     pln->vl = vl;
     pln->vs = vs;
     pln->td = 0;
     pln->mstart1 = mstart1;
     pln->mcount1 = mcount1;

     {
	  double n0 = 0.5 * (r - 1) * (2 * mcount1) * vl;
	  pln->super.super.ops = cld->ops;
	  pln->super.super.ops.mul += (kind == R2HC ? 5.0 : 7.0) * n0;
	  pln->super.super.ops.add += 4.0 * n0;
	  pln->super.super.ops.other += 11.0 * n0;
     }
     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cld);
     X(plan_destroy_internal)(cld0);
     return (plan *) 0;
}

static void regsolver(planner *plnr, INT r)
{
     S *slv = (S *)X(mksolver_hc2hc)(sizeof(S), r, mkcldw);
     REGISTER_SOLVER(plnr, &(slv->super));
     if (X(mksolver_hc2hc_hook)) {
	  slv = (S *)X(mksolver_hc2hc_hook)(sizeof(S), r, mkcldw);
	  REGISTER_SOLVER(plnr, &(slv->super));
     }
}

void X(hc2hc_generic_register)(planner *p)
{
     regsolver(p, 0);
}
