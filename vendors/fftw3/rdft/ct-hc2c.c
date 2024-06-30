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
#include "dft/dft.h"

typedef struct {
     plan_rdft2 super;
     plan *cld;
     plan *cldw;
     INT r;
} P;

static void apply_dit(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;
     plan_hc2c *cldw;
     UNUSED(r1);

     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, r0, cr);

     cldw = (plan_hc2c *) ego->cldw;
     cldw->apply(ego->cldw, cr, ci);
}

static void apply_dif(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld;
     plan_hc2c *cldw;
     UNUSED(r1);

     cldw = (plan_hc2c *) ego->cldw;
     cldw->apply(ego->cldw, cr, ci);

     cld = (plan_rdft *) ego->cld;
     cld->apply(ego->cld, cr, r0);
}

static void apply_dit_dft(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;
     plan_hc2c *cldw;

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, r0, r1, cr, ci);

     cldw = (plan_hc2c *) ego->cldw;
     cldw->apply(ego->cldw, cr, ci);
}

static void apply_dif_dft(const plan *ego_, R *r0, R *r1, R *cr, R *ci)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld;
     plan_hc2c *cldw;

     cldw = (plan_hc2c *) ego->cldw;
     cldw->apply(ego->cldw, cr, ci);

     cld = (plan_dft *) ego->cld;
     cld->apply(ego->cld, ci, cr, r1, r0);
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
     p->print(p, "(rdft2-ct-%s/%D%(%p%)%(%p%))",
	      (ego->super.apply == apply_dit || 
	       ego->super.apply == apply_dit_dft)
	      ? "dit" : "dif",
	      ego->r, ego->cldw, ego->cld);
}

static int applicable0(const hc2c_solver *ego, const problem *p_, planner *plnr)
{
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     INT r;

     return (1
	     && p->sz->rnk == 1
	     && p->vecsz->rnk <= 1 

	     && (/* either the problem is R2HC, which is solved by DIT */
		  (p->kind == R2HC)
		  ||
		  /* or the problem is HC2R, in which case it is solved
		     by DIF, which destroys the input */
		  (p->kind == HC2R && 
		   (p->r0 == p->cr || !NO_DESTROY_INPUTP(plnr))))
		  
	     && ((r = X(choose_radix)(ego->r, p->sz->dims[0].n)) > 0)
	     && p->sz->dims[0].n > r);
}

static int hc2c_applicable(const hc2c_solver *ego, const problem *p_,
                           planner *plnr)
{
     const problem_rdft2 *p;

     if (!applicable0(ego, p_, plnr))
          return 0;

     p = (const problem_rdft2 *) p_;

     return (0
	     || p->vecsz->rnk == 0
	     || !NO_VRECURSEP(plnr)
	  );
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const hc2c_solver *ego = (const hc2c_solver *) ego_;
     const problem_rdft2 *p;
     P *pln = 0;
     plan *cld = 0, *cldw = 0;
     INT n, r, m, v, ivs, ovs;
     iodim *d;

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!hc2c_applicable(ego, p_, plnr))
          return (plan *) 0;

     p = (const problem_rdft2 *) p_;
     d = p->sz->dims;
     n = d[0].n;
     r = X(choose_radix)(ego->r, n);
     A((r % 2) == 0);
     m = n / r;

     X(tensor_tornk1)(p->vecsz, &v, &ivs, &ovs);

     switch (p->kind) {
	 case R2HC:
	      cldw = ego->mkcldw(ego, R2HC, 
				 r, m * d[0].os, 
				 m, d[0].os,
				 v, ovs,
				 p->cr, p->ci, plnr);
	      if (!cldw) goto nada;

	      switch (ego->hc2ckind) {
		  case HC2C_VIA_RDFT:
		       cld = X(mkplan_d)(
			    plnr, 
			    X(mkproblem_rdft_1_d)(
				 X(mktensor_1d)(m, (r/2)*d[0].is, d[0].os),
				 X(mktensor_3d)(
				      2, p->r1 - p->r0, p->ci - p->cr,
				      r / 2, d[0].is, m * d[0].os,
				      v, ivs, ovs),
				 p->r0, p->cr, R2HC) 
			    );
		       if (!cld) goto nada;

		       pln = MKPLAN_RDFT2(P, &padt, apply_dit);
		       break;

		  case HC2C_VIA_DFT:
		       cld = X(mkplan_d)(
			    plnr, 
			    X(mkproblem_dft_d)(
				 X(mktensor_1d)(m, (r/2)*d[0].is, d[0].os),
				 X(mktensor_2d)(
				      r / 2, d[0].is, m * d[0].os,
				      v, ivs, ovs),
				 p->r0, p->r1, p->cr, p->ci) 
			    );
		       if (!cld) goto nada;

		       pln = MKPLAN_RDFT2(P, &padt, apply_dit_dft);
		       break;
	      }
	      break;

	 case HC2R:
	      cldw = ego->mkcldw(ego, HC2R, 
				 r, m * d[0].is, 
				 m, d[0].is,
				 v, ivs,
				 p->cr, p->ci, plnr);
	      if (!cldw) goto nada;

	      switch (ego->hc2ckind) {
		  case HC2C_VIA_RDFT:
		       cld = X(mkplan_d)(
			    plnr, 
			    X(mkproblem_rdft_1_d)(
				 X(mktensor_1d)(m, d[0].is, (r/2)*d[0].os),
				 X(mktensor_3d)(
				      2, p->ci - p->cr, p->r1 - p->r0, 
				      r / 2, m * d[0].is, d[0].os,
				      v, ivs, ovs),
				 p->cr, p->r0, HC2R) 
			    );
		       if (!cld) goto nada;

		       pln = MKPLAN_RDFT2(P, &padt, apply_dif);
		       break;

		  case HC2C_VIA_DFT:
		       cld = X(mkplan_d)(
			    plnr, 
			    X(mkproblem_dft_d)(
				 X(mktensor_1d)(m, d[0].is, (r/2)*d[0].os),
				 X(mktensor_2d)(
				      r / 2, m * d[0].is, d[0].os,
				      v, ivs, ovs),
				 p->ci, p->cr, p->r1, p->r0) 
			    );
		       if (!cld) goto nada;

		       pln = MKPLAN_RDFT2(P, &padt, apply_dif_dft);
		       break;
	      }
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

hc2c_solver *X(mksolver_hc2c)(size_t size, INT r, 
			      hc2c_kind hc2ckind,
			      hc2c_mkinferior mkcldw)
{
     static const solver_adt sadt = { PROBLEM_RDFT2, mkplan, 0 };
     hc2c_solver *slv = (hc2c_solver *)X(mksolver)(size, &sadt);
     slv->r = r;
     slv->hc2ckind = hc2ckind;
     slv->mkcldw = mkcldw;
     return slv;
}

plan *X(mkplan_hc2c)(size_t size, const plan_adt *adt, hc2capply apply)
{
     plan_hc2c *ego;

     ego = (plan_hc2c *) X(mkplan)(size, adt);
     ego->apply = apply;

     return &(ego->super);
}
