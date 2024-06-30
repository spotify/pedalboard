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

/* copy A into B, using output stride of A and input stride of B */
typedef struct {
     dotens2_closure k;
     R *ra; R *ia;
     R *rb; R *ib;
     int scalea, scaleb;
} cpy_closure;

static void cpy0(dotens2_closure *k_, 
		 int indxa, int ondxa, int indxb, int ondxb)
{
     cpy_closure *k = (cpy_closure *)k_;
     k->rb[indxb * k->scaleb] = k->ra[ondxa * k->scalea];
     k->ib[indxb * k->scaleb] = k->ia[ondxa * k->scalea];
     UNUSED(indxa); UNUSED(ondxb);
}

static void cpy(R *ra, R *ia, const bench_tensor *sza, int scalea,
		R *rb, R *ib, const bench_tensor *szb, int scaleb)
{
     cpy_closure k;
     k.k.apply = cpy0;
     k.ra = ra; k.ia = ia; k.rb = rb; k.ib = ib;
     k.scalea = scalea; k.scaleb = scaleb;
     bench_dotens2(sza, szb, &k.k);
}

typedef struct {
     dofft_closure k;
     bench_problem *p;
} dofft_dft_closure;

static void dft_apply(dofft_closure *k_, bench_complex *in, bench_complex *out)
{
     dofft_dft_closure *k = (dofft_dft_closure *)k_;
     bench_problem *p = k->p;
     bench_tensor *totalsz, *pckdsz;
     bench_tensor *totalsz_swap, *pckdsz_swap;
     bench_real *ri, *ii, *ro, *io;
     int totalscale;

     totalsz = tensor_append(p->vecsz, p->sz);
     pckdsz = verify_pack(totalsz, 2);
     ri = (bench_real *) p->in;
     ro = (bench_real *) p->out;

     totalsz_swap = tensor_copy_swapio(totalsz);
     pckdsz_swap = tensor_copy_swapio(pckdsz);

     /* confusion: the stride is the distance between complex elements
	when using interleaved format, but it is the distance between
	real elements when using split format */
     if (p->split) {
	  ii = p->ini ? (bench_real *) p->ini : ri + p->iphyssz;
	  io = p->outi ? (bench_real *) p->outi : ro + p->ophyssz;
	  totalscale = 1;
     } else {
	  ii = p->ini ? (bench_real *) p->ini : ri + 1;
	  io = p->outi ? (bench_real *) p->outi : ro + 1;
	  totalscale = 2;
     }

     cpy(&c_re(in[0]), &c_im(in[0]), pckdsz, 1,
	    ri, ii, totalsz, totalscale);
     after_problem_ccopy_from(p, ri, ii);
     doit(1, p);
     after_problem_ccopy_to(p, ro, io);
     if (k->k.recopy_input)
	  cpy(ri, ii, totalsz_swap, totalscale,
	      &c_re(in[0]), &c_im(in[0]), pckdsz_swap, 1);
     cpy(ro, io, totalsz, totalscale,
	 &c_re(out[0]), &c_im(out[0]), pckdsz, 1);

     tensor_destroy(totalsz);
     tensor_destroy(pckdsz);
     tensor_destroy(totalsz_swap);
     tensor_destroy(pckdsz_swap);
}

void verify_dft(bench_problem *p, int rounds, double tol, errors *e)
{
     C *inA, *inB, *inC, *outA, *outB, *outC, *tmp;
     int n, vecn, N;
     dofft_dft_closure k;

     BENCH_ASSERT(p->kind == PROBLEM_COMPLEX);

     k.k.apply = dft_apply;
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
     e->l = linear(&k.k, 0, N, inA, inB, inC, outA, outB, outC,
		   tmp, rounds, tol);

     e->s = 0.0;
     e->s = dmax(e->s, tf_shift(&k.k, 0, p->sz, n, vecn, p->sign,
				inA, inB, outA, outB, 
				tmp, rounds, tol, TIME_SHIFT));
     e->s = dmax(e->s, tf_shift(&k.k, 0, p->sz, n, vecn, p->sign,
				inA, inB, outA, outB, 
				tmp, rounds, tol, FREQ_SHIFT));

     if (!p->in_place && !p->destroy_input)
	  preserves_input(&k.k, 0, N, inA, inB, outB, rounds);

     bench_free(tmp);
     bench_free(outC);
     bench_free(outB);
     bench_free(outA);
     bench_free(inC);
     bench_free(inB);
     bench_free(inA);
}


void accuracy_dft(bench_problem *p, int rounds, int impulse_rounds,
		  double t[6])
{
     dofft_dft_closure k;
     int n;
     C *a, *b;

     BENCH_ASSERT(p->kind == PROBLEM_COMPLEX);
     BENCH_ASSERT(p->sz->rnk == 1);
     BENCH_ASSERT(p->vecsz->rnk == 0);

     k.k.apply = dft_apply;
     k.k.recopy_input = 0;
     k.p = p;
     n = tensor_sz(p->sz);

     a = (C *) bench_malloc(n * sizeof(C));
     b = (C *) bench_malloc(n * sizeof(C));
     accuracy_test(&k.k, 0, p->sign, n, a, b, rounds, impulse_rounds, t);
     bench_free(b);
     bench_free(a);
}
