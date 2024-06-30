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

#include "threads/threads.h"

typedef struct {
     plan_rdft super;
     plan *cld;
     plan **cldws;
     int nthr;
     INT r;
} P;

typedef struct {
     plan **cldws;
     R *IO;
} PD;

static void *spawn_apply(spawn_data *d)
{
     PD *ego = (PD *) d->data;
     
     plan_hc2hc *cldw = (plan_hc2hc *) (ego->cldws[d->thr_num]);
     cldw->apply((plan *) cldw, ego->IO);
     return 0;
}

static void apply_dit(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;

     cld = (plan_rdft *) ego->cld;
     cld->apply((plan *) cld, I, O);

     {
	  PD d;
	  
	  d.IO = O;
	  d.cldws = ego->cldws;

	  X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*)&d);
     }
}

static void apply_dif(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;

     {
	  PD d;
	  
	  d.IO = I;
	  d.cldws = ego->cldws;

	  X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*)&d);
     }

     cld = (plan_rdft *) ego->cld;
     cld->apply((plan *) cld, I, O);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     int i;
     X(plan_awake)(ego->cld, wakefulness);
     for (i = 0; i < ego->nthr; ++i)
	  X(plan_awake)(ego->cldws[i], wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     int i;
     X(plan_destroy_internal)(ego->cld);
     for (i = 0; i < ego->nthr; ++i)
	  X(plan_destroy_internal)(ego->cldws[i]);
     X(ifree)(ego->cldws);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     int i;
     p->print(p, "(rdft-thr-ct-%s-x%d/%D",
	      ego->super.apply == apply_dit ? "dit" : "dif",
	      ego->nthr, ego->r);
     for (i = 0; i < ego->nthr; ++i)
          if (i == 0 || (ego->cldws[i] != ego->cldws[i-1] &&
                         (i <= 1 || ego->cldws[i] != ego->cldws[i-2])))
               p->print(p, "%(%p%)", ego->cldws[i]);
     p->print(p, "%(%p%))", ego->cld);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const hc2hc_solver *ego = (const hc2hc_solver *) ego_;
     const problem_rdft *p;
     P *pln = 0;
     plan *cld = 0, **cldws = 0;
     INT n, r, m, v, ivs, ovs, mcount;
     int i, nthr, plnr_nthr_save;
     INT block_size;
     iodim *d;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (plnr->nthr <= 1 || !X(hc2hc_applicable)(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_rdft *) p_;
     d = p->sz->dims;
     n = d[0].n;
     r = X(choose_radix)(ego->r, n);
     m = n / r;
     mcount = (m + 2) / 2;

     X(tensor_tornk1)(p->vecsz, &v, &ivs, &ovs);

     block_size = (mcount + plnr->nthr - 1) / plnr->nthr;
     nthr = (int)((mcount + block_size - 1) / block_size);
     plnr_nthr_save = plnr->nthr;
     plnr->nthr = (plnr->nthr + nthr - 1) / nthr;

     cldws = (plan **) MALLOC(sizeof(plan *) * nthr, PLANS);
     for (i = 0; i < nthr; ++i) cldws[i] = (plan *) 0;

     switch (p->kind[0]) {
	 case R2HC:
	      for (i = 0; i < nthr; ++i) {
		   cldws[i] = ego->mkcldw(ego, 
					  R2HC, r, m, d[0].os, v, ovs, 
					  i*block_size, 
					  (i == nthr - 1) ? 
					  (mcount - i*block_size) : block_size,
					  p->O, plnr);
		   if (!cldws[i]) goto nada;
	      }

	      plnr->nthr = plnr_nthr_save;

	      cld = X(mkplan_d)(plnr, 
				X(mkproblem_rdft_d)(
				     X(mktensor_1d)(m, r * d[0].is, d[0].os),
				     X(mktensor_2d)(r, d[0].is, m * d[0].os,
						    v, ivs, ovs),
				     p->I, p->O, p->kind)
		   );
	      if (!cld) goto nada;

	      pln = MKPLAN_RDFT(P, &padt, apply_dit);
	      break;

	 case HC2R:
	      for (i = 0; i < nthr; ++i) {
		   cldws[i] = ego->mkcldw(ego, 
					  HC2R, r, m, d[0].is, v, ivs, 
					  i*block_size, 
					  (i == nthr - 1) ? 
					  (mcount - i*block_size) : block_size,
					  p->I, plnr);
		   if (!cldws[i]) goto nada;
	      }

	      plnr->nthr = plnr_nthr_save;

	      cld = X(mkplan_d)(plnr, 
				X(mkproblem_rdft_d)(
				     X(mktensor_1d)(m, d[0].is, r * d[0].os),
				     X(mktensor_2d)(r, m * d[0].is, d[0].os,
						    v, ivs, ovs),
				     p->I, p->O, p->kind)
		   );
	      if (!cld) goto nada;
	      
	      pln = MKPLAN_RDFT(P, &padt, apply_dif);
	      break;

	 default: 
	      A(0);
     }

     pln->cld = cld;
     pln->cldws = cldws;
     pln->nthr = nthr;
     pln->r = r;
     X(ops_zero)(&pln->super.super.ops);
     for (i = 0; i < nthr; ++i) {
          X(ops_add2)(&cldws[i]->ops, &pln->super.super.ops);
	  pln->super.super.could_prune_now_p |= cldws[i]->could_prune_now_p;
     }
     X(ops_add2)(&cld->ops, &pln->super.super.ops);
     return &(pln->super.super);

 nada:
     if (cldws) {
	  for (i = 0; i < nthr; ++i)
	       X(plan_destroy_internal)(cldws[i]);
	  X(ifree)(cldws);
     }
     X(plan_destroy_internal)(cld);
     return (plan *) 0;
}

hc2hc_solver *X(mksolver_hc2hc_threads)(size_t size, INT r, 
					hc2hc_mkinferior mkcldw)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     hc2hc_solver *slv = (hc2hc_solver *)X(mksolver)(size, &sadt);
     slv->r = r;
     slv->mkcldw = mkcldw;
     return slv;
}
