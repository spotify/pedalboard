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
     plan_dft super;
     plan *cld;
     plan **cldws;
     int nthr;
     INT r;
} P;

typedef struct {
     plan **cldws;
     R *r, *i;
} PD;

static void *spawn_apply(spawn_data *d)
{
     PD *ego = (PD *) d->data;
     INT thr_num = d->thr_num;

     plan_dftw *cldw = (plan_dftw *) (ego->cldws[thr_num]);
     cldw->apply((plan *) cldw, ego->r, ego->i);
     return 0;
}

static void apply_dit(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, ri, ii, ro, io);

     {
	  PD d;

	  d.r = ro; d.i = io;
	  d.cldws = ego->cldws;

	  X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*)&d);
     }
}

static void apply_dif(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;

     {
	  PD d;

	  d.r = ri; d.i = ii;
	  d.cldws = ego->cldws;

	  X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*)&d);
     }

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, ri, ii, ro, io);
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
     p->print(p, "(dft-thr-ct-%s-x%d/%D",
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
     const ct_solver *ego = (const ct_solver *) ego_;
     const problem_dft *p;
     P *pln = 0;
     plan *cld = 0, **cldws = 0;
     INT n, r, m, v, ivs, ovs;
     INT block_size;
     int i, nthr, plnr_nthr_save;
     iodim *d;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if (plnr->nthr <= 1 || !X(ct_applicable)(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_dft *) p_;
     d = p->sz->dims;
     n = d[0].n;
     r = X(choose_radix)(ego->r, n);
     m = n / r;

     X(tensor_tornk1)(p->vecsz, &v, &ivs, &ovs);

     block_size = (m + plnr->nthr - 1) / plnr->nthr;
     nthr = (int)((m + block_size - 1) / block_size);
     plnr_nthr_save = plnr->nthr;
     plnr->nthr = (plnr->nthr + nthr - 1) / nthr;

     cldws = (plan **) MALLOC(sizeof(plan *) * nthr, PLANS);
     for (i = 0; i < nthr; ++i) cldws[i] = (plan *) 0;

     switch (ego->dec) {
	 case DECDIT:
	 {
	      for (i = 0; i < nthr; ++i) {
		   cldws[i] = ego->mkcldw(ego,
					  r, m * d[0].os, m * d[0].os,
					  m, d[0].os,
					  v, ovs, ovs,
					  i*block_size,
					  (i == nthr - 1) ?
					  (m - i*block_size) : block_size,
					  p->ro, p->io, plnr);
		   if (!cldws[i]) goto nada;
	      }

	      plnr->nthr = plnr_nthr_save;

	      cld = X(mkplan_d)(plnr,
				X(mkproblem_dft_d)(
				     X(mktensor_1d)(m, r * d[0].is, d[0].os),
				     X(mktensor_2d)(r, d[0].is, m * d[0].os,
						    v, ivs, ovs),
				     p->ri, p->ii, p->ro, p->io)
		   );
	      if (!cld) goto nada;

	      pln = MKPLAN_DFT(P, &padt, apply_dit);
	      break;
	 }
	 case DECDIF:
	 case DECDIF+TRANSPOSE:
	 {
	      INT cors, covs; /* cldw ors, ovs */
	      if (ego->dec == DECDIF+TRANSPOSE) {
		   cors = ivs;
		   covs = m * d[0].is;
		   /* ensure that we generate well-formed dftw subproblems */
		   /* FIXME: too conservative */
		   if (!(1
			 && r == v
			 && d[0].is == r * cors))
			goto nada;

		   /* FIXME: allow in-place only for now, like in
		      fftw-3.[01] */
		   if (!(1
			 && p->ri == p->ro
			 && d[0].is == r * d[0].os
			 && cors == d[0].os
			 && covs == ovs
			    ))
			goto nada;
	      } else {
		   cors = m * d[0].is;
		   covs = ivs;
	      }

	      for (i = 0; i < nthr; ++i) {
		   cldws[i] = ego->mkcldw(ego,
					  r, m * d[0].is, cors,
					  m, d[0].is,
					  v, ivs, covs,
					  i*block_size,
					  (i == nthr - 1) ?
					  (m - i*block_size) : block_size,
					  p->ri, p->ii, plnr);
		   if (!cldws[i]) goto nada;
	      }

	      plnr->nthr = plnr_nthr_save;

	      cld = X(mkplan_d)(plnr,
				X(mkproblem_dft_d)(
				     X(mktensor_1d)(m, d[0].is, r * d[0].os),
				     X(mktensor_2d)(r, cors, d[0].os,
						    v, covs, ovs),
				     p->ri, p->ii, p->ro, p->io)
		   );
	      if (!cld) goto nada;

	      pln = MKPLAN_DFT(P, &padt, apply_dif);
	      break;
	 }

	 default: A(0);

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

ct_solver *X(mksolver_ct_threads)(size_t size, INT r, int dec,
				  ct_mkinferior mkcldw,
				  ct_force_vrecursion force_vrecursionp)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     ct_solver *slv = (ct_solver *) X(mksolver)(size, &sadt);
     slv->r = r;
     slv->dec = dec;
     slv->mkcldw = mkcldw;
     slv->force_vrecursionp = force_vrecursionp;
     return slv;
}
