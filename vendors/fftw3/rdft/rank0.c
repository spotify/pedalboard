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


/* plans for rank-0 RDFTs (copy operations) */

#include "rdft/rdft.h"

#ifdef HAVE_STRING_H
#include <string.h>		/* for memcpy() */
#endif

#define MAXRNK 32 /* FIXME: should malloc() */

typedef struct {
     plan_rdft super;
     INT vl;
     int rnk;
     iodim d[MAXRNK];
     const char *nam;
} P;

typedef struct {
     solver super;
     rdftapply apply;
     int (*applicable)(const P *pln, const problem_rdft *p);
     const char *nam;
} S;

/* copy up to MAXRNK dimensions from problem into plan.  If a
   contiguous dimension exists, save its length in pln->vl */
static int fill_iodim(P *pln, const problem_rdft *p)
{
     int i;
     const tensor *vecsz = p->vecsz;

     pln->vl = 1;
     pln->rnk = 0;
     for (i = 0; i < vecsz->rnk; ++i) {
	  /* extract contiguous dimensions */
	  if (pln->vl == 1 &&
	      vecsz->dims[i].is == 1 && vecsz->dims[i].os == 1) 
	       pln->vl = vecsz->dims[i].n;
	  else if (pln->rnk == MAXRNK) 
	       return 0;
	  else 
	       pln->d[pln->rnk++] = vecsz->dims[i];
     }

     return 1;
}

/* generic higher-rank copy routine, calls cpy2d() to do the real work */
static void copy(const iodim *d, int rnk, INT vl,
		 R *I, R *O,
		 cpy2d_func cpy2d)
{
     A(rnk >= 2);
     if (rnk == 2)
	  cpy2d(I, O, d[0].n, d[0].is, d[0].os, d[1].n, d[1].is, d[1].os, vl);
     else {
	  INT i;
	  for (i = 0; i < d[0].n; ++i, I += d[0].is, O += d[0].os)
	       copy(d + 1, rnk - 1, vl, I, O, cpy2d);
     }
}

/* FIXME: should be more general */
static int transposep(const P *pln)
{
     int i;

     for (i = 0; i < pln->rnk - 2; ++i) 
	  if (pln->d[i].is != pln->d[i].os)
	       return 0;
     
     return (pln->d[i].n == pln->d[i+1].n &&
	     pln->d[i].is == pln->d[i+1].os &&
	     pln->d[i].os == pln->d[i+1].is);
}

/* generic higher-rank transpose routine, calls transpose2d() to do
 * the real work */
static void transpose(const iodim *d, int rnk, INT vl,
		      R *I,
		      transpose_func transpose2d)
{
     A(rnk >= 2);
     if (rnk == 2)
	  transpose2d(I, d[0].n, d[0].is, d[0].os, vl);
     else {
	  INT i;
	  for (i = 0; i < d[0].n; ++i, I += d[0].is)
	       transpose(d + 1, rnk - 1, vl, I, transpose2d);
     }
}

/**************************************************************/
/* rank 0,1,2, out of place, iterative */
static void apply_iter(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;

     switch (ego->rnk) {
	 case 0: 
	      X(cpy1d)(I, O, ego->vl, 1, 1, 1);
	      break;
	 case 1:
	      X(cpy1d)(I, O, 
		       ego->d[0].n, ego->d[0].is, ego->d[0].os, 
		       ego->vl);
	      break;
	 default:
	      copy(ego->d, ego->rnk, ego->vl, I, O, X(cpy2d_ci));
	      break;
     }
}

static int applicable_iter(const P *pln, const problem_rdft *p)
{
     UNUSED(pln);
     return (p->I != p->O);
}

/**************************************************************/
/* out of place, write contiguous output */
static void apply_cpy2dco(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     copy(ego->d, ego->rnk, ego->vl, I, O, X(cpy2d_co));
}

static int applicable_cpy2dco(const P *pln, const problem_rdft *p)
{
     int rnk = pln->rnk;
     return (1
	     && p->I != p->O
	     && rnk >= 2

	     /* must not duplicate apply_iter */
	     && (X(iabs)(pln->d[rnk - 2].is) <= X(iabs)(pln->d[rnk - 1].is)
		 ||
		 X(iabs)(pln->d[rnk - 2].os) <= X(iabs)(pln->d[rnk - 1].os))
	  );
}

/**************************************************************/
/* out of place, tiled, no buffering */
static void apply_tiled(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     copy(ego->d, ego->rnk, ego->vl, I, O, X(cpy2d_tiled));
}

static int applicable_tiled(const P *pln, const problem_rdft *p)
{
     return (1
	     && p->I != p->O
	     && pln->rnk >= 2

	     /* somewhat arbitrary */
	     && X(compute_tilesz)(pln->vl, 1) > 4
	  );
}

/**************************************************************/
/* out of place, tiled, with buffer */
static void apply_tiledbuf(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     copy(ego->d, ego->rnk, ego->vl, I, O, X(cpy2d_tiledbuf));
}

#define applicable_tiledbuf applicable_tiled

/**************************************************************/
/* rank 0, out of place, using memcpy */
static void apply_memcpy(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;

     A(ego->rnk == 0);
     memcpy(O, I, ego->vl * sizeof(R));
}

static int applicable_memcpy(const P *pln, const problem_rdft *p)
{
     return (1
	     && p->I != p->O 
	     && pln->rnk == 0
	     && pln->vl > 2 /* do not bother memcpy-ing complex numbers */
	     );
}

/**************************************************************/
/* rank > 0 vecloop, out of place, using memcpy (e.g. out-of-place
   transposes of vl-tuples ... for large vl it should be more
   efficient to use memcpy than the tiled stuff). */

static void memcpy_loop(size_t cpysz, int rnk, const iodim *d, R *I, R *O)
{
     INT i, n = d->n, is = d->is, os = d->os;
     if (rnk == 1)
	  for (i = 0; i < n; ++i, I += is, O += os)
	       memcpy(O, I, cpysz);
     else {
	  --rnk; ++d;
	  for (i = 0; i < n; ++i, I += is, O += os)
	       memcpy_loop(cpysz, rnk, d, I, O);
     }
}

static void apply_memcpy_loop(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     memcpy_loop(ego->vl * sizeof(R), ego->rnk, ego->d, I, O);
}

static int applicable_memcpy_loop(const P *pln, const problem_rdft *p)
{
     return (p->I != p->O
	     && pln->rnk > 0
             && pln->vl > 2 /* do not bother memcpy-ing complex numbers */);
}

/**************************************************************/
/* rank 2, in place, square transpose, iterative */
static void apply_ip_sq(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     UNUSED(O);
     transpose(ego->d, ego->rnk, ego->vl, I, X(transpose));
}


static int applicable_ip_sq(const P *pln, const problem_rdft *p)
{
     return (1
	     && p->I == p->O
	     && pln->rnk >= 2
	     && transposep(pln));
}

/**************************************************************/
/* rank 2, in place, square transpose, tiled */
static void apply_ip_sq_tiled(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     UNUSED(O);
     transpose(ego->d, ego->rnk, ego->vl, I, X(transpose_tiled));
}

static int applicable_ip_sq_tiled(const P *pln, const problem_rdft *p)
{
     return (1
	     && applicable_ip_sq(pln, p)

	     /* somewhat arbitrary */
	     && X(compute_tilesz)(pln->vl, 2) > 4
	  );
}

/**************************************************************/
/* rank 2, in place, square transpose, tiled, buffered */
static void apply_ip_sq_tiledbuf(const plan *ego_, R *I, R *O)
{
     const P *ego = (const P *) ego_;
     UNUSED(O);
     transpose(ego->d, ego->rnk, ego->vl, I, X(transpose_tiledbuf));
}

#define applicable_ip_sq_tiledbuf applicable_ip_sq_tiled

/**************************************************************/
static int applicable(const S *ego, const problem *p_)
{
     const problem_rdft *p = (const problem_rdft *) p_;
     P pln;
     return (1
	     && p->sz->rnk == 0
	     && FINITE_RNK(p->vecsz->rnk)
	     && fill_iodim(&pln, p)
	     && ego->applicable(&pln, p)
	  );
}

static void print(const plan *ego_, printer *p)
{
     const P *ego = (const P *) ego_;
     int i;
     p->print(p, "(%s/%D", ego->nam, ego->vl);
     for (i = 0; i < ego->rnk; ++i)
	  p->print(p, "%v", ego->d[i].n);
     p->print(p, ")");
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const problem_rdft *p;
     const S *ego = (const S *) ego_;
     P *pln;
     int retval;

     static const plan_adt padt = {
	  X(rdft_solve), X(null_awake), print, X(plan_null_destroy)
     };

     UNUSED(plnr);

     if (!applicable(ego, p_))
          return (plan *) 0;

     p = (const problem_rdft *) p_;
     pln = MKPLAN_RDFT(P, &padt, ego->apply);

     retval = fill_iodim(pln, p);
     (void)retval; /* UNUSED unless DEBUG */
     A(retval);
     A(pln->vl > 0); /* because FINITE_RNK(p->vecsz->rnk) holds */
     pln->nam = ego->nam;

     /* X(tensor_sz)(p->vecsz) loads, X(tensor_sz)(p->vecsz) stores */
     X(ops_other)(2 * X(tensor_sz)(p->vecsz), &pln->super.super.ops);
     return &(pln->super.super);
}


void X(rdft_rank0_register)(planner *p)
{
     unsigned i;
     static struct {
	  rdftapply apply;
	  int (*applicable)(const P *, const problem_rdft *);
	  const char *nam;
     } tab[] = {
	  { apply_memcpy,   applicable_memcpy,   "rdft-rank0-memcpy" },
	  { apply_memcpy_loop,   applicable_memcpy_loop,  
	    "rdft-rank0-memcpy-loop" },
	  { apply_iter,     applicable_iter,     "rdft-rank0-iter-ci" },
	  { apply_cpy2dco,  applicable_cpy2dco,  "rdft-rank0-iter-co" },
	  { apply_tiled,    applicable_tiled,    "rdft-rank0-tiled" },
	  { apply_tiledbuf, applicable_tiledbuf, "rdft-rank0-tiledbuf" },
	  { apply_ip_sq,    applicable_ip_sq,    "rdft-rank0-ip-sq" },
	  { 
	       apply_ip_sq_tiled,
	       applicable_ip_sq_tiled,
	       "rdft-rank0-ip-sq-tiled" 
	  },
	  { 
	       apply_ip_sq_tiledbuf,
	       applicable_ip_sq_tiledbuf,
	       "rdft-rank0-ip-sq-tiledbuf" 
	  },
     };

     for (i = 0; i < sizeof(tab) / sizeof(tab[0]); ++i) {
	  static const solver_adt sadt = { PROBLEM_RDFT, mkplan, 0 };
	  S *slv = MKSOLVER(S, &sadt);
	  slv->apply = tab[i].apply;
	  slv->applicable = tab[i].applicable;
	  slv->nam = tab[i].nam;
	  REGISTER_SOLVER(p, &(slv->super));
     }
}
