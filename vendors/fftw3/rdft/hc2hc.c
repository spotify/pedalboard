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

hc2hc_solver *(*X(mksolver_hc2hc_hook))(size_t, INT, hc2hc_mkinferior) = 0;

typedef struct {
     plan_rdft super;
     plan *cld;
     plan *cldw;
     INT r;
} P;

static void apply_dit(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;
     plan_hc2hc *cldw;

     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, I, O);

     cldw = (plan_hc2hc *) ego->cldw;
     cldw->apply(ego->cldw, O);
}

static void apply_dif(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;
     plan_hc2hc *cldw;

     cldw = (plan_hc2hc *) ego->cldw;
     cldw->apply(ego->cldw, I);

     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, I, O);
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
     p->print(p, "(rdft-ct-%s/%D%(%p%)%(%p%))",
	      ego->super.apply == apply_dit ? "dit" : "dif",
	      ego->r, ego->cldw, ego->cld);
}

static int applicable0(const hc2hc_solver *ego, const problem *p_, planner *plnr)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     INT r;

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1 

	     && (/* either the problem is R2HC, which is solved by DIT */
		  (p->kind[0] == R2HC)
		  ||
		  /* or the problem is HC2R, in which case it is solved
		     by DIF, which destroys the input */
		  (p->kind[0] == HC2R && 
		   (p->I == p->O || !NO_DESTROY_INPUTP(plnr))))
		  
	     && ((r = X(choose_radix)(ego->r, p->sz->dims[0].n)) > 0)
	     && p->sz->dims[0].n > r);
}

int X(hc2hc_applicable)(const hc2hc_solver *ego, const problem *p_, planner *plnr)
{
     const problem_rdft *p;

     if (!applicable0(ego, p_, plnr))
          return 0;

     p = (const problem_rdft *) p_;

     return (0
	     || p->vecsz->rnk == 0
	     || !NO_VRECURSEP(plnr)
	  );
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const hc2hc_solver *ego = (const hc2hc_solver *) ego_;
     const problem_rdft *p;
     P *pln = 0;
     plan *cld = 0, *cldw = 0;
     INT n, r, m, v, ivs, ovs;
     iodim *d;

     static const plan_adt padt = {
	  X(rdft_solve), awake, print, destroy
     };

     if (NO_NONTHREADEDP(plnr) || !X(hc2hc_applicable)(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_rdft *) p_;
     d = p->sz->dims;
     n = d[0].n;
     r = X(choose_radix)(ego->r, n);
     m = n / r;

     X(tensor_tornk1)(p->vecsz, &v, &ivs, &ovs);

     switch (p->kind[0]) {
	 case R2HC:
	      cldw = ego->mkcldw(ego, 
				 R2HC, r, m, d[0].os, v, ovs, 0, (m+2)/2, 
				 p->O, plnr);
	      if (!cldw) goto nada;

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
	      cldw = ego->mkcldw(ego,
				 HC2R, r, m, d[0].is, v, ivs, 0, (m+2)/2, 
				 p->I, plnr);
	      if (!cldw) goto nada;

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

hc2hc_solver *X(mksolver_hc2hc)(size_t size, INT r, hc2hc_mkinferior mkcldw)
{
     static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
     hc2hc_solver *slv = (hc2hc_solver *)X(mksolver)(size, &sadt);
     slv->r = r;
     slv->mkcldw = mkcldw;
     return slv;
}

plan *X(mkplan_hc2hc)(size_t size, const plan_adt *adt, hc2hcapply apply)
{
     plan_hc2hc *ego;

     ego = (plan_hc2hc *) X(mkplan)(size, adt);
     ego->apply = apply;

     return &(ego->super);
}
