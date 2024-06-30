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

/* Recursive "radix-r" distributed transpose, which breaks a transpose
   over p processes into p/r transposes over r processes plus r
   transposes over p/r processes.  If performed recursively, this
   produces a total of O(p log p) messages vs. O(p^2) messages for a
   direct approach.

   However, this is not necessarily an improvement.  The total size of
   all the messages is actually increased from O(N) to O(N log p)
   where N is the total data size.  Also, the amount of local data
   rearrangement is increased.  So, it's not clear, a priori, what the
   best algorithm will be, and we'll leave it to the planner.  (In
   theory and practice, it looks like this becomes advantageous for
   large p, in the limit where the message sizes are small and
   latency-dominated.)
*/

#include "mpi-transpose.h"
#include <string.h>

typedef struct {
     solver super;
     int (*radix)(int np);
     const char *nam;
     int preserve_input; /* preserve input even if DESTROY_INPUT was passed */
} S;

typedef struct {
     plan_mpi_transpose super;

     plan *cld1, *cldtr, *cldtm;
     int preserve_input;

     int r; /* "radix" */
     const char *nam;
} P;

static void apply(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     plan_rdft *cld1, *cldtr, *cldtm;

     cld1 = (plan_rdft *) ego->cld1;
     if (cld1) cld1->apply((plan *) cld1, I, O);

     if (ego->preserve_input) I = O;

     cldtr = (plan_rdft *) ego->cldtr;
     if (cldtr) cldtr->apply((plan *) cldtr, O, I);

     cldtm = (plan_rdft *) ego->cldtm;
     if (cldtm) cldtm->apply((plan *) cldtm, I, O);
}

static int radix_sqrt(int np)
{
     int r;
     for (r = (int) (X(isqrt)(np)); np % r != 0; ++r)
	  ;
     return r;
}

static int radix_first(int np)
{
     int r = (int) (X(first_divisor)(np));
     return (r >= (int) (X(isqrt)(np)) ? 0 : r);
}

/* the local allocated space on process pe required for the given transpose
   dimensions and block sizes */
static INT transpose_space(INT nx, INT ny, INT block, INT tblock, int pe)
{
     return X(imax)(XM(block)(nx, block, pe) * ny,
		    nx * XM(block)(ny, tblock, pe));
}

/* check whether the recursive transposes fit within the space
   that must have been allocated on each process for this transpose;
   this must be modified if the subdivision in mkplan is changed! */
static int enough_space(INT nx, INT ny, INT block, INT tblock,
			int r, int n_pes)
{
     int pe;
     int m = n_pes / r;
     for (pe = 0; pe < n_pes; ++pe) {
	  INT space = transpose_space(nx, ny, block, tblock, pe);
	  INT b1 = XM(block)(nx, r * block, pe / r);
	  INT b2 = XM(block)(ny, m * tblock, pe % r);
	  if (transpose_space(b1, ny, block, m*tblock, pe % r) > space
	      || transpose_space(nx, b2, r*block, tblock, pe / r) > space)
	       return 0;
     }
     return 1;
}

/* In theory, transpose-recurse becomes advantageous for message sizes
   below some minimum, assuming that the time is dominated by
   communications.  In practice, we want to constrain the minimum
   message size for transpose-recurse to keep the planning time down.
   I've set this conservatively according to some simple experiments
   on a Cray XT3 where the crossover message size was 128, although on
   a larger-latency machine the crossover will be larger. */
#define SMALL_MESSAGE 2048

static int applicable(const S *ego, const problem *p_,
		      const planner *plnr, int *r)
{
     const problem_mpi_transpose *p = (const problem_mpi_transpose *) p_;
     int n_pes;
     MPI_Comm_size(p->comm, &n_pes);
     return (1
	     && p->tblock * n_pes == p->ny
	     && (!ego->preserve_input || (!NO_DESTROY_INPUTP(plnr)
                                          && p->I != p->O))
	     && (*r = ego->radix(n_pes)) && *r < n_pes && *r > 1
	     && enough_space(p->nx, p->ny, p->block, p->tblock, *r, n_pes)
	     && (!CONSERVE_MEMORYP(plnr) || *r > 8
		 || !X(toobig)((p->nx * (p->ny / n_pes) * p->vn) / *r))
	     && (!NO_SLOWP(plnr) || 
		 (p->nx * (p->ny / n_pes) * p->vn) / n_pes <= SMALL_MESSAGE)
	     && ONLY_TRANSPOSEDP(p->flags)
	  );
}

static void awake(plan *ego_, enum wakefulness wakefulness)
{
     P *ego = (P *) ego_;
     X(plan_awake)(ego->cld1, wakefulness);
     X(plan_awake)(ego->cldtr, wakefulness);
     X(plan_awake)(ego->cldtm, wakefulness);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     X(plan_destroy_internal)(ego->cldtm);
     X(plan_destroy_internal)(ego->cldtr);
     X(plan_destroy_internal)(ego->cld1);
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     p->print(p, "(mpi-transpose-recurse/%s/%d%s%(%p%)%(%p%)%(%p%))",
	      ego->nam, ego->r, ego->preserve_input==2 ?"/p":"",
	      ego->cld1, ego->cldtr, ego->cldtm);
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_mpi_transpose *p;
     P *pln;
     plan *cld1 = 0, *cldtr = 0, *cldtm = 0;
     R *I, *O;
     int me, np, r, m;
     INT b;
     MPI_Comm comm2;
     static const plan_adt padt = {
          XM(transpose_solve), awake, print, destroy
     };

     UNUSED(ego);

     if (!applicable(ego, p_, plnr, &r))
          return (plan *) 0;

     p = (const problem_mpi_transpose *) p_;

     MPI_Comm_size(p->comm, &np);
     MPI_Comm_rank(p->comm, &me);
     m = np / r;
     A(r * m == np);

     I = p->I; O = p->O;

     b = XM(block)(p->nx, p->block, me);
     A(p->tblock * np == p->ny); /* this is currently required for cld1 */
     if (p->flags & TRANSPOSED_IN) { 
          /* m x r x (bt x b x vn) -> r x m x (bt x b x vn) */
	  INT vn = p->vn * b * p->tblock;
	  cld1 = X(mkplan_f_d)(plnr,
                               X(mkproblem_rdft_0_d)(X(mktensor_3d)
						     (m, r*vn, vn,
						      r, vn, m*vn,
						      vn, 1, 1),
                                                     I, O),
                               0, 0, NO_SLOW);
     }
     else if (I != O) { /* combine cld1 with TRANSPOSED_IN permutation */
          /* b x m x r x bt x vn -> r x m x bt x b x vn */
	  INT vn = p->vn;
	  INT bt = p->tblock;
	  cld1 = X(mkplan_f_d)(plnr,
                               X(mkproblem_rdft_0_d)(X(mktensor_5d)
						     (b, m*r*bt*vn, vn,
						      m, r*bt*vn, bt*b*vn,
						      r, bt*vn, m*bt*b*vn,
						      bt, vn, b*vn,
						      vn, 1, 1),
                                                     I, O),
                               0, 0, NO_SLOW);
     }
     else { /* TRANSPOSED_IN permutation must be separate for in-place */
	  /* b x (m x r) x bt x vn -> b x (r x m) x bt x vn */
	  INT vn = p->vn * p->tblock;
	  cld1 = X(mkplan_f_d)(plnr,
                               X(mkproblem_rdft_0_d)(X(mktensor_4d)
						     (m, r*vn, vn,
						      r, vn, m*vn,
						      vn, 1, 1,
						      b, np*vn, np*vn),
                                                     I, O),
                               0, 0, NO_SLOW);
     }
     if (XM(any_true)(!cld1, p->comm)) goto nada;

     if (ego->preserve_input || NO_DESTROY_INPUTP(plnr)) I = O;

     b = XM(block)(p->nx, r * p->block, me / r);
     MPI_Comm_split(p->comm, me / r, me, &comm2);
     if (b)
	  cldtr = X(mkplan_d)(plnr, XM(mkproblem_transpose)
			      (b, p->ny, p->vn,
			       O, I, p->block, m * p->tblock, comm2, 
			       p->I != p->O
			       ? TRANSPOSED_IN : (p->flags & TRANSPOSED_IN)));
     MPI_Comm_free(&comm2);
     if (XM(any_true)(b && !cldtr, p->comm)) goto nada;
     
     b = XM(block)(p->ny, m * p->tblock, me % r);
     MPI_Comm_split(p->comm, me % r, me, &comm2);
     if (b)
	  cldtm = X(mkplan_d)(plnr, XM(mkproblem_transpose)
			      (p->nx, b, p->vn,
			       I, O, r * p->block, p->tblock, comm2, 
			       TRANSPOSED_IN | (p->flags & TRANSPOSED_OUT)));
     MPI_Comm_free(&comm2);
     if (XM(any_true)(b && !cldtm, p->comm)) goto nada;

     pln = MKPLAN_MPI_TRANSPOSE(P, &padt, apply);

     pln->cld1 = cld1;
     pln->cldtr = cldtr;
     pln->cldtm = cldtm;
     pln->preserve_input = ego->preserve_input ? 2 : NO_DESTROY_INPUTP(plnr);
     pln->r = r;
     pln->nam = ego->nam;

     pln->super.super.ops = cld1->ops;
     if (cldtr) X(ops_add2)(&cldtr->ops, &pln->super.super.ops);
     if (cldtm) X(ops_add2)(&cldtm->ops, &pln->super.super.ops);

     return &(pln->super.super);

 nada:
     X(plan_destroy_internal)(cldtm);
     X(plan_destroy_internal)(cldtr);
     X(plan_destroy_internal)(cld1);
     return (plan *) 0;
}

static solver *mksolver(int preserve_input,
			int (*radix)(int np), const char *nam)
{
     static const solver_adt sadt = { PROBLEM_MPI_TRANSPOSE, mkplan, 0 };
     S *slv = MKSOLVER(S, &sadt);
     slv->preserve_input = preserve_input;
     slv->radix = radix;
     slv->nam = nam;
     return &(slv->super);
}

void XM(transpose_recurse_register)(planner *p)
{
     int preserve_input;
     for (preserve_input = 0; preserve_input <= 1; ++preserve_input) {
	  REGISTER_SOLVER(p, mksolver(preserve_input, radix_sqrt, "sqrt"));
	  REGISTER_SOLVER(p, mksolver(preserve_input, radix_first, "first"));
     }
}
