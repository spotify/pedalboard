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


#include "verify.h"

/* copy real A into real B, using output stride of A and input stride of B */
typedef struct {
     dotens2_closure k;
     R *ra;
     R *rb;
} cpyr_closure;

static void cpyr0(dotens2_closure *k_,
                  int indxa, int ondxa, int indxb, int ondxb)
{
     cpyr_closure *k = (cpyr_closure *)k_;
     k->rb[indxb] = k->ra[ondxa];
     UNUSED(indxa); UNUSED(ondxb);
}

static void cpyr(R *ra, const bench_tensor *sza, 
		 R *rb, const bench_tensor *szb)
{
     cpyr_closure k;
     k.k.apply = cpyr0;
     k.ra = ra; k.rb = rb;
     bench_dotens2(sza, szb, &k.k);
}

/* copy unpacked halfcomplex A[n] into packed-complex B[n], using output stride
   of A and input stride of B.  Only copies non-redundant half; other
   half must be copied via mkhermitian. */
typedef struct {
     dotens2_closure k;
     int n;
     int as;
     int scalea;
     R *ra, *ia;
     R *rb, *ib;
} cpyhc2_closure;

static void cpyhc20(dotens2_closure *k_, 
		    int indxa, int ondxa, int indxb, int ondxb)
{
     cpyhc2_closure *k = (cpyhc2_closure *)k_;
     int i, n = k->n;
     int scalea = k->scalea;
     int as = k->as * scalea;
     R *ra = k->ra + ondxa * scalea, *ia = k->ia + ondxa * scalea;
     R *rb = k->rb + indxb, *ib = k->ib + indxb;
     UNUSED(indxa); UNUSED(ondxb);

     for (i = 0; i < n/2 + 1; ++i) {
	  rb[2*i] = ra[as*i];
	  ib[2*i] = ia[as*i];
     }
}

static void cpyhc2(R *ra, R *ia,
		   const bench_tensor *sza, const bench_tensor *vecsza,
		   int scalea,
		   R *rb, R *ib, const bench_tensor *szb)
{
     cpyhc2_closure k;
     BENCH_ASSERT(sza->rnk <= 1);
     k.k.apply = cpyhc20;
     k.n = tensor_sz(sza);
     k.scalea = scalea;
     if (!BENCH_FINITE_RNK(sza->rnk) || sza->rnk == 0)
	  k.as = 0;
     else
	  k.as = sza->dims[0].os;
     k.ra = ra; k.ia = ia; k.rb = rb; k.ib = ib;
     bench_dotens2(vecsza, szb, &k.k);
}

/* icpyhc2 is the inverse of cpyhc2 */

static void icpyhc20(dotens2_closure *k_, 
		     int indxa, int ondxa, int indxb, int ondxb)
{
     cpyhc2_closure *k = (cpyhc2_closure *)k_;
     int i, n = k->n;
     int scalea = k->scalea;
     int as = k->as * scalea;
     R *ra = k->ra + indxa * scalea, *ia = k->ia + indxa * scalea;
     R *rb = k->rb + ondxb, *ib = k->ib + ondxb;
     UNUSED(ondxa); UNUSED(indxb);

     for (i = 0; i < n/2 + 1; ++i) {
	  ra[as*i] = rb[2*i];
	  ia[as*i] = ib[2*i];
     }
}

static void icpyhc2(R *ra, R *ia, 
		    const bench_tensor *sza, const bench_tensor *vecsza,
		    int scalea,
		    R *rb, R *ib, const bench_tensor *szb)
{
     cpyhc2_closure k;
     BENCH_ASSERT(sza->rnk <= 1);
     k.k.apply = icpyhc20;
     k.n = tensor_sz(sza);
     k.scalea = scalea;
     if (!BENCH_FINITE_RNK(sza->rnk) || sza->rnk == 0)
	  k.as = 0;
     else
	  k.as = sza->dims[0].is;
     k.ra = ra; k.ia = ia; k.rb = rb; k.ib = ib;
     bench_dotens2(vecsza, szb, &k.k);
}

typedef struct {
     dofft_closure k;
     bench_problem *p;
} dofft_rdft2_closure;

static void rdft2_apply(dofft_closure *k_, 
			bench_complex *in, bench_complex *out)
{
     dofft_rdft2_closure *k = (dofft_rdft2_closure *)k_;
     bench_problem *p = k->p;
     bench_tensor *totalsz, *pckdsz, *totalsz_swap, *pckdsz_swap;
     bench_tensor *probsz2, *totalsz2, *pckdsz2;
     bench_tensor *probsz2_swap, *totalsz2_swap, *pckdsz2_swap;
     bench_real *ri, *ii, *ro, *io;
     int n2, totalscale;

     totalsz = tensor_append(p->vecsz, p->sz);
     pckdsz = verify_pack(totalsz, 2);
     n2 = tensor_sz(totalsz);
     if (BENCH_FINITE_RNK(p->sz->rnk) && p->sz->rnk > 0)
	  n2 = (n2 / p->sz->dims[p->sz->rnk - 1].n) * 
	       (p->sz->dims[p->sz->rnk - 1].n / 2 + 1);
     ri = (bench_real *) p->in;
     ro = (bench_real *) p->out;

     if (BENCH_FINITE_RNK(p->sz->rnk) && p->sz->rnk > 0 && n2 > 0) {
	  probsz2 = tensor_copy_sub(p->sz, p->sz->rnk - 1, 1);
	  totalsz2 = tensor_copy_sub(totalsz, 0, totalsz->rnk - 1);
	  pckdsz2 = tensor_copy_sub(pckdsz, 0, pckdsz->rnk - 1);
     }
     else {
	  probsz2 = mktensor(0);
	  totalsz2 = tensor_copy(totalsz);
	  pckdsz2 = tensor_copy(pckdsz);
     }

     totalsz_swap = tensor_copy_swapio(totalsz);
     pckdsz_swap = tensor_copy_swapio(pckdsz);
     totalsz2_swap = tensor_copy_swapio(totalsz2);
     pckdsz2_swap = tensor_copy_swapio(pckdsz2);
     probsz2_swap = tensor_copy_swapio(probsz2);

     /* confusion: the stride is the distance between complex elements
	when using interleaved format, but it is the distance between
	real elements when using split format */
     if (p->split) {
	  ii = p->ini ? (bench_real *) p->ini : ri + n2;
	  io = p->outi ? (bench_real *) p->outi : ro + n2;
	  totalscale = 1;
     } else {
	  ii = p->ini ? (bench_real *) p->ini : ri + 1;
	  io = p->outi ? (bench_real *) p->outi : ro + 1;
	  totalscale = 2;
     }

     if (p->sign < 0) { /* R2HC */
	  int N, vN, i;
	  cpyr(&c_re(in[0]), pckdsz, ri, totalsz);
	  after_problem_rcopy_from(p, ri);
	  doit(1, p);
	  after_problem_hccopy_to(p, ro, io);
	  if (k->k.recopy_input)
	       cpyr(ri, totalsz_swap, &c_re(in[0]), pckdsz_swap);
	  cpyhc2(ro, io, probsz2, totalsz2, totalscale,
		 &c_re(out[0]), &c_im(out[0]), pckdsz2);
	  N = tensor_sz(p->sz);
	  vN = tensor_sz(p->vecsz);
	  for (i = 0; i < vN; ++i)
	       mkhermitian(out + i*N, p->sz->rnk, p->sz->dims, 1);
     }
     else { /* HC2R */
	  icpyhc2(ri, ii, probsz2, totalsz2, totalscale,
		  &c_re(in[0]), &c_im(in[0]), pckdsz2);
	  after_problem_hccopy_from(p, ri, ii);
	  doit(1, p);
	  after_problem_rcopy_to(p, ro);
	  if (k->k.recopy_input)
	       cpyhc2(ri, ii, probsz2_swap, totalsz2_swap, totalscale,
		      &c_re(in[0]), &c_im(in[0]), pckdsz2_swap);
	  mkreal(out, tensor_sz(pckdsz));
	  cpyr(ro, totalsz, &c_re(out[0]), pckdsz);
     }

     tensor_destroy(totalsz);
     tensor_destroy(pckdsz);
     tensor_destroy(totalsz_swap);
     tensor_destroy(pckdsz_swap);
     tensor_destroy(probsz2);
     tensor_destroy(totalsz2);
     tensor_destroy(pckdsz2);
     tensor_destroy(probsz2_swap);
     tensor_destroy(totalsz2_swap);
     tensor_destroy(pckdsz2_swap);
}

void verify_rdft2(bench_problem *p, int rounds, double tol, errors *e)
{
     C *inA, *inB, *inC, *outA, *outB, *outC, *tmp;
     int n, vecn, N;
     dofft_rdft2_closure k;

     BENCH_ASSERT(p->kind == PROBLEM_REAL);

     if (!BENCH_FINITE_RNK(p->sz->rnk) || !BENCH_FINITE_RNK(p->vecsz->rnk))
	  return;      /* give up */

     k.k.apply = rdft2_apply;
     k.k.recopy_input = 0;
     k.p = p;

     if (rounds == 0)
	  rounds = 20;  /* default value */

     n = tensor_sz(p->sz);
     vecn = tensor_sz(p->vecsz);
     N = n * vecn;

     inA = (C *) bench_malloc(N * sizeof(C));
     inB = (C *) bench_malloc(N * sizeof(C));
     inC = (C *) bench_malloc(N * sizeof(C));
     outA = (C *) bench_malloc(N * sizeof(C));
     outB = (C *) bench_malloc(N * sizeof(C));
     outC = (C *) bench_malloc(N * sizeof(C));
     tmp = (C *) bench_malloc(N * sizeof(C));

     e->i = impulse(&k.k, n, vecn, inA, inB, inC, outA, outB, outC, 
		    tmp, rounds, tol);
     e->l = linear(&k.k, 1, N, inA, inB, inC, outA, outB, outC,
		   tmp, rounds, tol);

     e->s = 0.0;
     if (p->sign < 0)
	  e->s = dmax(e->s, tf_shift(&k.k, 1, p->sz, n, vecn, p->sign,
				     inA, inB, outA, outB, 
				     tmp, rounds, tol, TIME_SHIFT));
     else
	  e->s = dmax(e->s, tf_shift(&k.k, 1, p->sz, n, vecn, p->sign,
				     inA, inB, outA, outB, 
				     tmp, rounds, tol, FREQ_SHIFT));
     
     if (!p->in_place && !p->destroy_input)
	  preserves_input(&k.k, p->sign < 0 ? mkreal : mkhermitian1,
			  N, inA, inB, outB, rounds);

     bench_free(tmp);
     bench_free(outC);
     bench_free(outB);
     bench_free(outA);
     bench_free(inC);
     bench_free(inB);
     bench_free(inA);
}

void accuracy_rdft2(bench_problem *p, int rounds, int impulse_rounds,
		    double t[6])
{
     dofft_rdft2_closure k;
     int n;
     C *a, *b;

     BENCH_ASSERT(p->kind == PROBLEM_REAL);
     BENCH_ASSERT(p->sz->rnk == 1);
     BENCH_ASSERT(p->vecsz->rnk == 0);

     k.k.apply = rdft2_apply;
     k.k.recopy_input = 0;
     k.p = p;
     n = tensor_sz(p->sz);

     a = (C *) bench_malloc(n * sizeof(C));
     b = (C *) bench_malloc(n * sizeof(C));
     accuracy_test(&k.k, p->sign < 0 ? mkreal : mkhermitian1, p->sign, 
		   n, a, b, rounds, impulse_rounds, t);
     bench_free(b);
     bench_free(a);
}
