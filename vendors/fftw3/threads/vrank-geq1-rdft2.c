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
     solver super;
     int vecloop_dim;
     const int *buddies;
     size_t nbuddies;
} S;

typedef struct {
     plan_rdft2 super;

     plan **cldrn;
     INT its, ots;
     int nthr;
     const S *solver;
} P;

typedef struct {
     INT its, ots;
     R *r0, *r1, *cr, *ci;
     plan **cldrn;
} PD;

static void *spawn_apply(spawn_data *d)
{
     PD *ego = (PD *) d->data;
     INT its = ego->its;
     INT ots = ego->ots;
     int thr_num = d->thr_num;
     plan_rdft2 *cld = (plan_rdft2 *) ego->cldrn[d->thr_num];

     cld->apply((plan *) cld,
		ego->r0 + thr_num * its, ego->r1 + thr_num * its,
		ego->cr + thr_num * ots, ego->ci + thr_num * ots);
     return 0;
}

static void apply(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     PD d;

     d.its = ego->its;
     d.ots = ego->ots;
     d.cldrn = ego->cldrn;
     d.r0 = r0; d.r1 = r1; d.cr = cr; d.ci = ci;

     X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*) &d);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     int i;
     for (i = 0; i < ego->nthr; ++i)
	  X(plan_awake)(ego->cldrn[i], wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     int i;
     for (i = 0; i < ego->nthr; ++i)
	  X(plan_destroy_internal)(ego->cldrn[i]);
     X(ifree)(ego->cldrn);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     const S *s = ego->solver;
     int i;
     p->print(p, "(rdft2-thr-vrank>=1-x%d/%d)", ego->nthr, s->vecloop_dim);
     for (i = 0; i < ego->nthr; ++i)
	  if (i == 0 || (ego->cldrn[i] != ego->cldrn[i-1] &&
			 (i <= 1 || ego->cldrn[i] != ego->cldrn[i-2])))
	       p->print(p, "%(%p%)", ego->cldrn[i]);
     p->putchr(p, ')');
}

static int pickdim(const S *ego, const tensor *vecsz, int oop, int *dp)
{
     return X(pickdim)(ego->vecloop_dim, ego->buddies, ego->nbuddies,
		       vecsz, oop, dp);
}

static int applicable0(const solver *ego_, const problem *p_,
		       const planner *plnr, int *dp)
{
     const S *ego = (const S *) ego_;
     const problem_rdft2 *p = (const problem_rdft2 *) p_;

     if (FINITE_RNK(p->vecsz->rnk)
	 && p->vecsz->rnk > 0
	 && plnr->nthr > 1
	 && pickdim(ego, p->vecsz, p->r0 != p->cr, dp)) {
	  if (p->r0 != p->cr)
	       return 1;  /* can always operate out-of-place */

	  return(X(rdft2_inplace_strides)(p, *dp));
     }

     return 0;
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr, int *dp)
{
     const S *ego = (const S *)ego_;

     if (!applicable0(ego_, p_, plnr, dp)) return 0;

     /* fftw2 behavior */
     if (NO_VRANK_SPLITSP(plnr) && (ego->vecloop_dim != ego->buddies[0]))
	  return 0;

     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_rdft2 *p;
     P *pln;
     problem *cldp;
     int vdim;
     iodim *d;
     plan **cldrn = (plan **) 0;
     int i, nthr;
     INT its, ots, block_size;
     tensor *vecsz;

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr, &vdim))
          return (plan *) 0;
     p = (const problem_rdft2 *) p_;

     d = p->vecsz->dims + vdim;

     block_size = (d->n + plnr->nthr - 1) / plnr->nthr;
     nthr = (int)((d->n + block_size - 1) / block_size);
     plnr->nthr = (plnr->nthr + nthr - 1) / nthr;
     X(rdft2_strides)(p->kind, d, &its, &ots);
     its *= block_size; ots *= block_size;

     cldrn = (plan **)MALLOC(sizeof(plan *) * nthr, PLANS);
     for (i = 0; i < nthr; ++i) cldrn[i] = (plan *) 0;
     
     vecsz = X(tensor_copy)(p->vecsz);
     for (i = 0; i < nthr; ++i) {
	  vecsz->dims[vdim].n =
	       (i == nthr - 1) ? (d->n - i*block_size) : block_size;
	  cldp = X(mkproblem_rdft2)(p->sz, vecsz,
				    p->r0 + i*its, p->r1 + i*its,
				    p->cr + i*ots, p->ci + i*ots, 
				    p->kind);
	  cldrn[i] = X(mkplan_d)(plnr, cldp);
	  if (!cldrn[i]) goto nada;
     }
     X(tensor_destroy)(vecsz);

     pln = MKPLAN_RDFT2(P, &padt, apply);

     pln->cldrn = cldrn;
     pln->its = its;
     pln->ots = ots;
     pln->nthr = nthr;

     pln->solver = ego;
     X(ops_zero)(&pln->super.super.ops);
     pln->super.super.pcost = 0;
     for (i = 0; i < nthr; ++i) {
	  X(ops_add2)(&cldrn[i]->ops, &pln->super.super.ops);
	  pln->super.super.pcost += cldrn[i]->pcost;
     }

     return &(pln->super.super);

 nada:
     if (cldrn) {
	  for (i = 0; i < nthr; ++i)
	       X(plan_destroy_internal)(cldrn[i]);
	  X(ifree)(cldrn);
     }
     X(tensor_destroy)(vecsz);
     return (plan *) 0;
}

static solver *mksolver(int vecloop_dim, const int *buddies, size_t nbuddies)
{
     static const solver_adt sadt = { PROBLEM_RDFT2, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->vecloop_dim = vecloop_dim;
     slv->buddies = buddies;
     slv->nbuddies = nbuddies;
     return &(slv->super);
}

void X(rdft2_thr_vrank_geq1_register)(planner *p)
{
     /* FIXME: Should we try other vecloop_dim values? */
     static const int buddies[] = { 1, -1 };
     size_t i;

     for (i = 0; i < NELEM(buddies); ++i)
          REGISTER_SOLVER(p, mksolver(buddies[i], buddies, NELEM(buddies)));
}
