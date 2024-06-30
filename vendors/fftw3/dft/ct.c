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


#include "dft/ct.h"

ct_solver *(*X(mksolver_ct_hook))(size_t, INT, int, 
				  ct_mkinferior, ct_force_vrecursion) = 0;

typedef struct {
     plan_dft super;
     plan *cld;
     plan *cldw;
     INT r;
} P;

static void apply_dit(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;
     plan_dftw *cldw;

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, ri, ii, ro, io);

     cldw = (plan_dftw *) ego->cldw;
     cldw->apply(ego->cldw, ro, io);
}

static void apply_dif(const plan *ego_, R *ri, R *ii, R *ro, R *io)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;
     plan_dftw *cldw;

     cldw = (plan_dftw *) ego->cldw;
     cldw->apply(ego->cldw, ri, ii);

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, ri, ii, ro, io);
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld, wakefulness);
     X(plan_awake)(ego->cldw, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldw);
     X(plan_destroy_internal)(ego->cld);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(dft-ct-%s/%D%(%p%)%(%p%))",
	      ego->super.apply == apply_dit ? "dit" : "dif",
	      ego->r, ego->cldw, ego->cld);
}

static int applicable0(const ct_solver *ego, const problem *p_, planner *plnr)
{
     const problem_dft *p = (const problem_dft *) p_;
     INT r;

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1

	     /* DIF destroys the input and we don't like it */
	     && (ego->dec == DECDIT ||
		 p->ri == p->ro ||
		 !NO_DESTROY_INPUTP(plnr))

	     && ((r = X(choose_radix)(ego->r, p->sz->dims[0].n)) > 1)
	     && p->sz->dims[0].n > r);
}


int X(ct_applicable)(const ct_solver *ego, const problem *p_, planner *plnr)
{
     const problem_dft *p;

     if (!applicable0(ego, p_, plnr))
          return 0;

     p = (const problem_dft *) p_;

     return (0
	     || ego->dec == DECDIF+TRANSPOSE
	     || p->vecsz->rnk == 0
	     || !NO_VRECURSEP(plnr)
	     || (ego->force_vrecursionp && ego->force_vrecursionp(ego, p))
	  );
}


static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const ct_solver *ego = (const ct_solver *) ego_;
     const problem_dft *p;
     P *pln = 0;
     plan *cld = 0, *cldw = 0;
     INT n, r, m, v, ivs, ovs;
     iodim *d;

     static const plan_adt padt = {
	  X(dft_solve), awake, print, destroy
     };

     if ((NO_NONTHREADEDP(plnr)) || !X(ct_applicable)(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_dft *) p_;
     d = p->sz->dims;
     n = d[0].n;
     r = X(choose_radix)(ego->r, n);
     m = n / r;

     X(tensor_tornk1)(p->vecsz, &v, &ivs, &ovs);

     switch (ego->dec) {
	 case DECDIT:
	 {
	      cldw = ego->mkcldw(ego,
				 r, m * d[0].os, m * d[0].os,
				 m, d[0].os,
				 v, ovs, ovs,
				 0, m,
				 p->ro, p->io, plnr);
	      if (!cldw) goto nada;

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

	      cldw = ego->mkcldw(ego,
				 r, m * d[0].is, cors,
				 m, d[0].is,
				 v, ivs, covs,
				 0, m,
				 p->ri, p->ii, plnr);
	      if (!cldw) goto nada;

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
     pln->cldw = cldw;
     pln->r = r;
     X(ops_add)(&cld->ops, &cldw->ops, &pln->super.super.ops);

     /* inherit could_prune_now_p attribute from cldw */
     pln->super.super.could_prune_now_p = cldw->could_prune_now_p;
     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cldw);
     X(plan_destroy_internal)(cld);
     return (plan *) 0;
}

ct_solver *X(mksolver_ct)(size_t size, INT r, int dec, 
			  ct_mkinferior mkcldw,
			  ct_force_vrecursion force_vrecursionp)
{
     static const solver_adt sadt = { PROBLEM_DFT, mkplan, 0 };
     ct_solver *slv = (ct_solver *)X(mksolver)(size, &sadt);
     slv->r = r;
     slv->dec = dec;
     slv->mkcldw = mkcldw;
     slv->force_vrecursionp = force_vrecursionp;
     return slv;
}

plan *X(mkplan_dftw)(size_t size, const plan_adt *adt, dftwapply apply)
{
     plan_dftw *ego;

     ego = (plan_dftw *) X(mkplan)(size, adt);
     ego->apply = apply;

     return &(ego->super);
}
