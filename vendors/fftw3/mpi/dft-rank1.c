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

/* Complex DFTs of rank == 1 via six-step algorithm. */

#include "mpi-dft.h"
#include "mpi-transpose.h"
#include "dft/dft.h"

typedef struct {
     solver super;
     rdftapply apply; /* apply_ddft_first or apply_ddft_last */
     int preserve_input; /* preserve input even if DESTROY_INPUT was passed */
} S;

typedef struct {
     plan_mpi_dft super;

     triggen *t;
     plan *cldt, *cld_ddft, *cld_dft;
     INT roff, ioff;
     int preserve_input;
     INT vn, xmin, xmax, xs, m, r;
} P;

static void do_twiddle(triggen *t, INT ir, INT m, INT vn, R *xr, R *xi)
{
     void (*rotate)(triggen *, INT, R, R, R *) = t->rotate;
     INT im, iv;
     for (im = 0; im < m; ++im)
	  for (iv = 0; iv < vn; ++iv) {
	       /* TODO: modify/inline rotate function
		  so that it can do whole vn vector at once? */
	       R c[2];
	       rotate(t, ir * im, *xr, *xi, c);
	       *xr = c[0]; *xi = c[1];
	       xr += 2; xi += 2;
	  }
}

/* radix-r DFT of size r*m.  This is equivalent to an m x r 2d DFT,
   plus twiddle factors between the size-m and size-r 1d DFTs, where
   the m dimension is initially distributed.  The output is transposed
   to r x m where the r dimension is distributed. 

   This algorithm follows the general sequence:
        global transpose (m x r -> r x m)
        DFTs of size m
	multiply by twiddles + global transpose (r x m -> m x r)
	DFTs of size r
	global transpose (m x r -> r x m)
   where the multiplication by twiddles can come before or after
   the middle transpose.  The first/last transposes are omitted
   for SCRAMBLED_IN/OUT formats, respectively.

   However, we wish to exploit our dft-rank1-bigvec solver, which
   solves a vector of distributed DFTs via transpose+dft+transpose.
   Therefore, we can group *either* the DFTs of size m *or* the
   DFTs of size r with their surrounding transposes as a single
   distributed-DFT (ddft) plan.  These two variations correspond to
   apply_ddft_first or apply_ddft_last, respectively.
*/

static void apply_ddft_first(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld_dft;
     plan_rdft *cldt, *cld_ddft;
     INT roff, ioff, im, mmax, ms, r, vn;
     triggen *t;
     R *dI, *dO;

     /* distributed size-m DFTs, with output in m x r format */
     cld_ddft = (plan_rdft *) ego->cld_ddft;
     cld_ddft->apply(ego->cld_ddft, I, O);

     cldt = (plan_rdft *) ego->cldt;
     if (ego->preserve_input || !cldt) I = O;

     /* twiddle multiplications, followed by 1d DFTs of size-r */
     cld_dft = (plan_dft *) ego->cld_dft;
     roff = ego->roff; ioff = ego->ioff;
     mmax = ego->xmax; ms = ego->xs;
     t = ego->t; r = ego->r; vn = ego->vn;
     dI = O; dO = I;
     for (im = ego->xmin; im <= mmax; ++im) {
	  do_twiddle(t, im, r, vn, dI+roff, dI+ioff);
	  cld_dft->apply((plan *) cld_dft, dI+roff, dI+ioff, dO+roff, dO+ioff);
	  dI += ms; dO += ms;
     }

     /* final global transpose (m x r -> r x m), if not SCRAMBLED_OUT */
     if (cldt) 
	  cldt->apply((plan *) cldt, I, O);
}

static void apply_ddft_last(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_dft *cld_dft;
     plan_rdft *cldt, *cld_ddft;
     INT roff, ioff, ir, rmax, rs, m, vn;
     triggen *t;
     R *dI, *dO0, *dO;

     /* initial global transpose (m x r -> r x m), if not SCRAMBLED_IN */
     cldt = (plan_rdft *) ego->cldt;
     if (cldt) {
	  cldt->apply((plan *) cldt, I, O);
	  dI = O;
     }
     else 
	  dI = I;
     if (ego->preserve_input) dO = O; else dO = I;
     dO0 = dO;

     /* 1d DFTs of size m, followed by twiddle multiplications */
     cld_dft = (plan_dft *) ego->cld_dft;
     roff = ego->roff; ioff = ego->ioff;
     rmax = ego->xmax; rs = ego->xs;
     t = ego->t; m = ego->m; vn = ego->vn;
     for (ir = ego->xmin; ir <= rmax; ++ir) {
	  cld_dft->apply((plan *) cld_dft, dI+roff, dI+ioff, dO+roff, dO+ioff);
	  do_twiddle(t, ir, m, vn, dO+roff, dO+ioff);
	  dI += rs; dO += rs;
     }

     /* distributed size-r DFTs, with output in r x m format */
     cld_ddft = (plan_rdft *) ego->cld_ddft;
     cld_ddft->apply(ego->cld_ddft, dO0, O);
}

static int applicable(const S *ego, const problem *p_,
		      const planner *plnr,
		      INT *r, INT rblock[2], INT mblock[2])
{
     const problem_mpi_dft *p = (const problem_mpi_dft *) p_;
     int n_pes;
     MPI_Comm_size(p->comm, &n_pes);
     return (1
	     && p->sz->rnk == 1

	     && ONLY_SCRAMBLEDP(p->flags)

	     && (!ego->preserve_input || (!NO_DESTROY_INPUTP(plnr)
                                          && p->I != p->O))

	     && (!(p->flags & SCRAMBLED_IN) || ego->apply == apply_ddft_last)
	     && (!(p->flags & SCRAMBLED_OUT) || ego->apply == apply_ddft_first)

	     && (!NO_SLOWP(plnr) /* slow if dft-serial is applicable */
                 || !XM(dft_serial_applicable)(p))

	     /* disallow if dft-rank1-bigvec is applicable since the
		data distribution may be slightly different (ugh!) */
	     && (p->vn < n_pes || p->flags)

	     && (*r = XM(choose_radix)(p->sz->dims[0], n_pes,
				       p->flags, p->sign,
				       rblock, mblock))

	     /* ddft_first or last has substantial advantages in the
		bigvec transpositions for the common case where
		n_pes == n/r or r, respectively */
	     && (!NO_UGLYP(plnr)
		 || !(*r == n_pes && ego->apply == apply_ddft_first)
		 || !(p->sz->dims[0].n / *r == n_pes 
		      && ego->apply == apply_ddft_last))
	  );
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cldt, wakefulness);
     X(plan_awake)(ego->cld_dft, wakefulness);
     X(plan_awake)(ego->cld_ddft, wakefulness);

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
     X(plan_destroy_internal)(ego->cldt);
     X(plan_destroy_internal)(ego->cld_dft);
     X(plan_destroy_internal)(ego->cld_ddft);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(mpi-dft-rank1/%D%s%s%(%p%)%(%p%)%(%p%))",
	      ego->r,
	      ego->super.apply == apply_ddft_first ? "/first" : "/last",
	      ego->preserve_input==2 ?"/p":"",
	      ego->cld_ddft, ego->cld_dft, ego->cldt);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_mpi_dft *p;
     P *pln;
     plan *cld_dft = 0, *cld_ddft = 0, *cldt = 0;
     R *ri, *ii, *ro, *io, *I, *O;
     INT r, rblock[2], m, mblock[2], rp, mp, mpblock[2], mpb;
     int my_pe, n_pes, preserve_input, ddft_first;
     dtensor *sz;
     static const plan_adt padt = {
          XM(dft_solve), awake, print, destroy
     };

     UNUSED(ego);

     if (!applicable(ego, p_, plnr, &r, rblock, mblock))
          return (plan *) 0;

     p = (const problem_mpi_dft *) p_;

     MPI_Comm_rank(p->comm, &my_pe);
     MPI_Comm_size(p->comm, &n_pes);

     m = p->sz->dims[0].n / r;

     /* some hackery so that we can plan both ddft_first and ddft_last
	as if they were ddft_first */
     if ((ddft_first = (ego->apply == apply_ddft_first))) {
	  rp = r; mp = m;
	  mpblock[IB] = mblock[IB]; mpblock[OB] = mblock[OB];
	  mpb = XM(block)(mp, mpblock[OB], my_pe);
     }
     else {
	  rp = m; mp = r;
	  mpblock[IB] = rblock[IB]; mpblock[OB] = rblock[OB];
	  mpb = XM(block)(mp, mpblock[IB], my_pe);
     }

     preserve_input = ego->preserve_input ? 2 : NO_DESTROY_INPUTP(plnr);

     sz = XM(mkdtensor)(1);
     sz->dims[0].n = mp;
     sz->dims[0].b[IB] = mpblock[IB];
     sz->dims[0].b[OB] = mpblock[OB];
     I = (ddft_first || !preserve_input) ? p->I : p->O;
     O = p->O;
     cld_ddft = X(mkplan_d)(plnr, XM(mkproblem_dft_d)(sz, rp * p->vn,
						      I, O, p->comm, p->sign,
						      RANK1_BIGVEC_ONLY));
     if (XM(any_true)(!cld_ddft, p->comm)) goto nada;

     I = TAINT((ddft_first || !p->flags) ? p->O : p->I, rp * p->vn * 2);
     O = TAINT((preserve_input || (ddft_first && p->flags)) ? p->O : p->I, 
	       rp * p->vn * 2);
     X(extract_reim)(p->sign, I, &ri, &ii);
     X(extract_reim)(p->sign, O, &ro, &io);
     cld_dft = X(mkplan_d)(plnr,
			X(mkproblem_dft_d)(X(mktensor_1d)(rp, p->vn*2,p->vn*2),
					   X(mktensor_1d)(p->vn, 2, 2),
					   ri, ii, ro, io));
     if (XM(any_true)(!cld_dft, p->comm)) goto nada;
     
     if (!p->flags) { /* !(SCRAMBLED_IN or SCRAMBLED_OUT) */
	  I = (ddft_first && preserve_input) ? p->O : p->I;
	  O = p->O;
	  cldt = X(mkplan_d)(plnr,
			     XM(mkproblem_transpose)(
				  m, r, p->vn * 2,
				  I, O,
				  ddft_first ? mblock[OB] : mblock[IB],
				  ddft_first ? rblock[OB] : rblock[IB],
				  p->comm, 0));
	  if (XM(any_true)(!cldt, p->comm)) goto nada;	  
     }

     pln = MKPLAN_MPI_DFT(P, &padt, ego->apply);

     pln->cld_ddft = cld_ddft;
     pln->cld_dft = cld_dft;
     pln->cldt = cldt;
     pln->preserve_input = preserve_input;
     X(extract_reim)(p->sign, p->O, &ro, &io);
     pln->roff = ro - p->O;
     pln->ioff = io - p->O;
     pln->vn = p->vn;
     pln->m = m;
     pln->r = r;
     pln->xmin = (ddft_first ? mblock[OB] : rblock[IB]) * my_pe;
     pln->xmax = pln->xmin + mpb - 1;
     pln->xs = rp * p->vn * 2;
     pln->t = 0;

     X(ops_add)(&cld_ddft->ops, &cld_dft->ops, &pln->super.super.ops);
     if (cldt) X(ops_add2)(&cldt->ops, &pln->super.super.ops);
     {
          double n0 = (1 + pln->xmax - pln->xmin) * (mp - 1) * pln->vn;
          pln->super.super.ops.mul += 8 * n0;
          pln->super.super.ops.add += 4 * n0;
          pln->super.super.ops.other += 8 * n0;
     }

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cldt);
     X(plan_destroy_internal)(cld_dft);
     X(plan_destroy_internal)(cld_ddft);
     return (plan *) 0;
}

static solver *mksolver(rdftapply apply, int preserve_input)
{
     static const solver_adt sadt = { PROBLEM_MPI_DFT, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->apply = apply;
     slv->preserve_input = preserve_input;
     return &(slv->super);
}

void XM(dft_rank1_register)(planner *p)
{
     rdftapply apply[] = { apply_ddft_first, apply_ddft_last };
     unsigned int iapply;
     int preserve_input;
     for (iapply = 0; iapply < sizeof(apply) / sizeof(apply[0]); ++iapply)
	  for (preserve_input = 0; preserve_input <= 1; ++preserve_input)
	       REGISTER_SOLVER(p, mksolver(apply[iapply], preserve_input));
}
