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

/* Lots of ugly duplication from verify-lib.c, plus lots of ugliness in
   general for all of the r2r variants...oh well, for now */

#include "verify.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
     bench_problem *p;
     bench_tensor *probsz;
     bench_tensor *totalsz;
     bench_tensor *pckdsz;
     bench_tensor *pckdvecsz;
} info;

/*
 * Utility functions:
 */

static double dabs(double x) { return (x < 0.0) ? -x : x; }
static double dmin(double x, double y) { return (x < y) ? x : y; }

static double raerror(R *a, R *b, int n)
{
     if (n > 0) {
          /* compute the relative Linf error */
          double e = 0.0, mag = 0.0;
          int i;

          for (i = 0; i < n; ++i) {
               e = dmax(e, dabs(a[i] - b[i]));
               mag = dmax(mag, dmin(dabs(a[i]), dabs(b[i])));
          }
	  if (dabs(mag) < 1e-14 && dabs(e) < 1e-14)
	       e = 0.0;
	  else
	       e /= mag;

#ifdef HAVE_ISNAN
          BENCH_ASSERT(!isnan(e));
#endif
          return e;
     } else
          return 0.0;
}

#define by2pi(m, n) ((K2PI * (m)) / (n))

/*
 * Improve accuracy by reducing x to range [0..1/8]
 * before multiplication by 2 * PI.
 */

static trigreal bench_sincos(trigreal m, trigreal n, int sinp)
{
     /* waiting for C to get tail recursion... */
     trigreal half_n = n * 0.5;
     trigreal quarter_n = half_n * 0.5;
     trigreal eighth_n = quarter_n * 0.5;
     trigreal sgn = 1.0;

     if (sinp) goto sin;
 cos:
     if (m < 0) { m = -m; /* goto cos; */ }
     if (m > half_n) { m = n - m; goto cos; }
     if (m > eighth_n) { m = quarter_n - m; goto sin; }
     return sgn * COS(by2pi(m, n));

 msin:
     sgn = -sgn;
 sin:
     if (m < 0) { m = -m; goto msin; }
     if (m > half_n) { m = n - m; goto msin; }
     if (m > eighth_n) { m = quarter_n - m; goto cos; }
     return sgn * SIN(by2pi(m, n));
}

static trigreal cos2pi(int m, int n)
{
     return bench_sincos((trigreal)m, (trigreal)n, 0);
}

static trigreal sin2pi(int m, int n)
{
     return bench_sincos((trigreal)m, (trigreal)n, 1);
}

static trigreal cos00(int i, int j, int n)
{
     return cos2pi(i * j, n);
}

static trigreal cos01(int i, int j, int n)
{
     return cos00(i, 2*j + 1, 2*n);
}

static trigreal cos10(int i, int j, int n)
{
     return cos00(2*i + 1, j, 2*n);
}

static trigreal cos11(int i, int j, int n)
{
     return cos00(2*i + 1, 2*j + 1, 4*n);
}

static trigreal sin00(int i, int j, int n)
{
     return sin2pi(i * j, n);
}

static trigreal sin01(int i, int j, int n)
{
     return sin00(i, 2*j + 1, 2*n);
}

static trigreal sin10(int i, int j, int n)
{
     return sin00(2*i + 1, j, 2*n);
}

static trigreal sin11(int i, int j, int n)
{
     return sin00(2*i + 1, 2*j + 1, 4*n);
}

static trigreal realhalf(int i, int j, int n)
{
     UNUSED(i);
     if (j <= n - j)
	  return 1.0;
     else
	  return 0.0;
}

static trigreal coshalf(int i, int j, int n)
{
     if (j <= n - j)
	  return cos00(i, j, n);
     else
	  return cos00(i, n - j, n);
}

static trigreal unity(int i, int j, int n)
{
     UNUSED(i);
     UNUSED(j);
     UNUSED(n);
     return 1.0;
}

typedef trigreal (*trigfun)(int, int, int);

static void rarand(R *a, int n)
{
     int i;

     /* generate random inputs */
     for (i = 0; i < n; ++i) {
	  a[i] = mydrand();
     }
}

/* C = A + B */
static void raadd(R *c, R *a, R *b, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  c[i] = a[i] + b[i];
     }
}

/* C = A - B */
static void rasub(R *c, R *a, R *b, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  c[i] = a[i] - b[i];
     }
}

/* B = rotate left A + rotate right A */
static void rarolr(R *b, R *a, int n, int nb, int na, 
		   r2r_kind_t k)
{
     int isL0 = 0, isL1 = 0, isR0 = 0, isR1 = 0;
     int i, ib, ia;

     for (ib = 0; ib < nb; ++ib) {
	  for (i = 0; i < n - 1; ++i)
	       for (ia = 0; ia < na; ++ia)
		    b[(ib * n + i) * na + ia] =
			 a[(ib * n + i + 1) * na + ia];

	  /* ugly switch to do boundary conditions for various r2r types */
	  switch (k) {
	       /* periodic boundaries */
	      case R2R_DHT:
	      case R2R_R2HC:
		   for (ia = 0; ia < na; ++ia) {
			b[(ib * n + n - 1) * na + ia] = 
			     a[(ib * n + 0) * na + ia];
			b[(ib * n + 0) * na + ia] += 
			     a[(ib * n + n - 1) * na + ia];
		   }
		   break;
		   
	      case R2R_HC2R: /* ugh (hermitian halfcomplex boundaries) */
		   if (n > 2) {
			if (n % 2 == 0)
			     for (ia = 0; ia < na; ++ia) {
				  b[(ib * n + n - 1) * na + ia] = 0.0;
				  b[(ib * n + 0) * na + ia] += 
				       a[(ib * n + 1) * na + ia];
				  b[(ib * n + n/2) * na + ia] += 
				       + a[(ib * n + n/2 - 1) * na + ia]
				       - a[(ib * n + n/2 + 1) * na + ia];
				  b[(ib * n + n/2 + 1) * na + ia] += 
				       - a[(ib * n + n/2) * na + ia];
			     }
			else 
			     for (ia = 0; ia < na; ++ia) {
				  b[(ib * n + n - 1) * na + ia] = 0.0;
				  b[(ib * n + 0) * na + ia] += 
				       a[(ib * n + 1) * na + ia];
				  b[(ib * n + n/2) * na + ia] += 
				       + a[(ib * n + n/2) * na + ia]
				       - a[(ib * n + n/2 + 1) * na + ia];
				  b[(ib * n + n/2 + 1) * na + ia] += 
				       - a[(ib * n + n/2 + 1) * na + ia]
				       - a[(ib * n + n/2) * na + ia];
			     }
		   } else /* n <= 2 */ {
			for (ia = 0; ia < na; ++ia) {
			     b[(ib * n + n - 1) * na + ia] =
				  a[(ib * n + 0) * na + ia];
			     b[(ib * n + 0) * na + ia] += 
				  a[(ib * n + n - 1) * na + ia];
			}
		   }
		   break;
		   
	      /* various even/odd boundary conditions */
	      case R2R_REDFT00:
		   isL1 = isR1 = 1;
		   goto mirrors;
	      case R2R_REDFT01:
		   isL1 = 1;
		   goto mirrors;
	      case R2R_REDFT10:
		   isL0 = isR0 = 1;
		   goto mirrors;
	      case R2R_REDFT11:
		   isL0 = 1;
		   isR0 = -1;
		   goto mirrors;
	      case R2R_RODFT00:
		   goto mirrors;
	      case R2R_RODFT01:
		   isR1 = 1;
		   goto mirrors;
	      case R2R_RODFT10:
		   isL0 = isR0 = -1;
		   goto mirrors;
	      case R2R_RODFT11:
		   isL0 = -1;
		   isR0 = 1;
		   goto mirrors;

	  mirrors:
		   
		   for (ia = 0; ia < na; ++ia)
			b[(ib * n + n - 1) * na + ia] = 
			     isR0 * a[(ib * n + n - 1) * na + ia]
			     + (n > 1 ? isR1 * a[(ib * n + n - 2) * na + ia]
				: 0);
		   
		   for (ia = 0; ia < na; ++ia)
			b[(ib * n) * na + ia] += 
			     isL0 * a[(ib * n) * na + ia]
			     + (n > 1 ? isL1 * a[(ib * n + 1) * na + ia] : 0);
		   
	  }

	  for (i = 1; i < n; ++i)
	       for (ia = 0; ia < na; ++ia)
		    b[(ib * n + i) * na + ia] +=
			 a[(ib * n + i - 1) * na + ia];
     }
}

static void raphase_shift(R *b, R *a, int n, int nb, int na,
			 int n0, int k0, trigfun t)
{
     int j, jb, ja;
 
     for (jb = 0; jb < nb; ++jb)
          for (j = 0; j < n; ++j) {
               trigreal c = 2.0 * t(1, j + k0, n0);

               for (ja = 0; ja < na; ++ja) {
                    int k = (jb * n + j) * na + ja;
                    b[k] = a[k] * c;
               }
          }
}

/* A = alpha * A  (real, in place) */
static void rascale(R *a, R alpha, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  a[i] *= alpha;
     }
}

/*
 * compute rdft:
 */

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

static void cpyr(R *ra, bench_tensor *sza, R *rb, bench_tensor *szb)
{
     cpyr_closure k;
     k.k.apply = cpyr0;
     k.ra = ra; k.rb = rb;
     bench_dotens2(sza, szb, &k.k);
}

static void dofft(info *nfo, R *in, R *out)
{
     cpyr(in, nfo->pckdsz, (R *) nfo->p->in, nfo->totalsz);
     after_problem_rcopy_from(nfo->p, (bench_real *)nfo->p->in);
     doit(1, nfo->p);
     after_problem_rcopy_to(nfo->p, (bench_real *)nfo->p->out);
     cpyr((R *) nfo->p->out, nfo->totalsz, out, nfo->pckdsz);
}

static double racmp(R *a, R *b, int n, const char *test, double tol)
{
     double d = raerror(a, b, n);
     if (d > tol) {
	  ovtpvt_err("Found relative error %e (%s)\n", d, test);
	  {
	       int i, N;
	       N = n > 300 && verbose <= 2 ? 300 : n;
	       for (i = 0; i < N; ++i)
		    ovtpvt_err("%8d %16.12f   %16.12f\n", i, 
			       (double) a[i],
			       (double) b[i]);
	  }
	  bench_exit(EXIT_FAILURE);
     }
     return d;
}

/***********************************************************************/

typedef struct {
     int n; /* physical size */
     int n0; /* "logical" transform size */
     int i0, k0; /* shifts of input/output */
     trigfun ti, ts;  /* impulse/shift trig functions */
} dim_stuff;

static void impulse_response(int rnk, dim_stuff *d, R impulse_amp,
			     R *A, int N)
{
     if (rnk == 0)
	  A[0] = impulse_amp;
     else {
	  int i;
	  N /= d->n;
	  for (i = 0; i < d->n; ++i) {
	       impulse_response(rnk - 1, d + 1,
				impulse_amp * d->ti(d->i0, d->k0 + i, d->n0),
				A + i * N, N);
	  }
     }
}

/***************************************************************************/

/*
 * Implementation of the FFT tester described in
 *
 * Funda Ergün. Testing multivariate linear functions: Overcoming the
 * generator bottleneck. In Proceedings of the Twenty-Seventh Annual
 * ACM Symposium on the Theory of Computing, pages 407-416, Las Vegas,
 * Nevada, 29 May--1 June 1995.
 *
 * Also: F. Ergun, S. R. Kumar, and D. Sivakumar, "Self-testing without
 * the generator bottleneck," SIAM J. on Computing 29 (5), 1630-51 (2000).
 */

static double rlinear(int n, info *nfo, R *inA, R *inB, R *inC, R *outA,
		      R *outB, R *outC, R *tmp, int rounds, double tol)
{
     double e = 0.0;
     int j;

     for (j = 0; j < rounds; ++j) {
	  R alpha, beta;
	  alpha = mydrand();
	  beta = mydrand();
	  rarand(inA, n);
	  rarand(inB, n);
	  dofft(nfo, inA, outA);
	  dofft(nfo, inB, outB);

	  rascale(outA, alpha, n);
	  rascale(outB, beta, n);
	  raadd(tmp, outA, outB, n);
	  rascale(inA, alpha, n);
	  rascale(inB, beta, n);
	  raadd(inC, inA, inB, n);
	  dofft(nfo, inC, outC);

	  e = dmax(e, racmp(outC, tmp, n, "linear", tol));
     }
     return e;
}

static double rimpulse(dim_stuff *d, R impulse_amp,
		       int n, int vecn, info *nfo, 
		       R *inA, R *inB, R *inC,
		       R *outA, R *outB, R *outC,
		       R *tmp, int rounds, double tol)
{
     double e = 0.0;
     int N = n * vecn;
     int i;
     int j;

     /* test 2: check that the unit impulse is transformed properly */

     for (i = 0; i < N; ++i) {
	  /* pls */
	  inA[i] = 0.0;
     }
     for (i = 0; i < vecn; ++i) {
	  inA[i * n] = (i+1) / (double)(vecn+1);
     
	  /* transform of the pls */
	  impulse_response(nfo->probsz->rnk, d, impulse_amp * inA[i * n],
			   outA + i * n, n);
     }

     dofft(nfo, inA, tmp);
     e = dmax(e, racmp(tmp, outA, N, "impulse 1", tol));

     for (j = 0; j < rounds; ++j) {
          rarand(inB, N);
          rasub(inC, inA, inB, N);
          dofft(nfo, inB, outB);
          dofft(nfo, inC, outC);
          raadd(tmp, outB, outC, N);
          e = dmax(e, racmp(tmp, outA, N, "impulse", tol));
     }
     return e;
}

static double t_shift(int n, int vecn, info *nfo, 
		      R *inA, R *inB, R *outA, R *outB, R *tmp,
		      int rounds, double tol,
		      dim_stuff *d)
{
     double e = 0.0;
     int nb, na, dim, N = n * vecn;
     int i, j;
     bench_tensor *sz = nfo->probsz;

     /* test 3: check the time-shift property */
     /* the paper performs more tests, but this code should be fine too */

     nb = 1;
     na = n;

     /* check shifts across all SZ dimensions */
     for (dim = 0; dim < sz->rnk; ++dim) {
	  int ncur = sz->dims[dim].n;

	  na /= ncur;

	  for (j = 0; j < rounds; ++j) {
	       rarand(inA, N);

	       for (i = 0; i < vecn; ++i) {
		    rarolr(inB + i * n, inA + i*n, ncur, nb,na, 
			  nfo->p->k[dim]);
	       }
	       dofft(nfo, inA, outA);
	       dofft(nfo, inB, outB);
	       for (i = 0; i < vecn; ++i) 
		    raphase_shift(tmp + i * n, outA + i * n, ncur, 
				 nb, na, d[dim].n0, d[dim].k0, d[dim].ts);
	       e = dmax(e, racmp(tmp, outB, N, "time shift", tol));
	  }

	  nb *= ncur;
     }
     return e;
}

/***********************************************************************/

void verify_r2r(bench_problem *p, int rounds, double tol, errors *e)
{
     R *inA, *inB, *inC, *outA, *outB, *outC, *tmp;
     info nfo;
     int n, vecn, N;
     double impulse_amp = 1.0;
     dim_stuff *d;
     int i;

     if (rounds == 0)
	  rounds = 20;  /* default value */

     n = tensor_sz(p->sz);
     vecn = tensor_sz(p->vecsz);
     N = n * vecn;

     d = (dim_stuff *) bench_malloc(sizeof(dim_stuff) * p->sz->rnk);
     for (i = 0; i < p->sz->rnk; ++i) {
	  int n0, i0, k0;
	  trigfun ti, ts;

	  d[i].n = n0 = p->sz->dims[i].n;
	  if (p->k[i] > R2R_DHT)
	       n0 = 2 * (n0 + (p->k[i] == R2R_REDFT00 ? -1 : 
			       (p->k[i] == R2R_RODFT00 ? 1 : 0)));
	  
	  switch (p->k[i]) {
	      case R2R_R2HC:
		   i0 = k0 = 0;
		   ti = realhalf;
		   ts = coshalf;
		   break;
	      case R2R_DHT:
		   i0 = k0 = 0;
		   ti = unity;
		   ts = cos00;
		   break;
	      case R2R_HC2R:
		   i0 = k0 = 0;
		   ti = unity;
		   ts = cos00;
		   break;
	      case R2R_REDFT00:
		   i0 = k0 = 0;
		   ti = ts = cos00;
		   break;
	      case R2R_REDFT01:
		   i0 = k0 = 0;
		   ti = ts = cos01;
		   break;
	      case R2R_REDFT10:
		   i0 = k0 = 0;
		   ti = cos10; impulse_amp *= 2.0;
		   ts = cos00;
		   break;
	      case R2R_REDFT11:
		   i0 = k0 = 0;
		   ti = cos11; impulse_amp *= 2.0;
		   ts = cos01;
		   break;
	      case R2R_RODFT00:
		   i0 = k0 = 1;
		   ti = sin00; impulse_amp *= 2.0;
		   ts = cos00;
		   break;
	      case R2R_RODFT01:
		   i0 = 1; k0 = 0;
		   ti = sin01; impulse_amp *= n == 1 ? 1.0 : 2.0;
		   ts = cos01;
		   break;
	      case R2R_RODFT10:
		   i0 = 0; k0 = 1;
		   ti = sin10; impulse_amp *= 2.0;
		   ts = cos00;
		   break;
	      case R2R_RODFT11:
		   i0 = k0 = 0;
		   ti = sin11; impulse_amp *= 2.0;
		   ts = cos01;
		   break;
	      default:
		   BENCH_ASSERT(0);
		   return;
	  }

	  d[i].n0 = n0;
	  d[i].i0 = i0;
	  d[i].k0 = k0;
	  d[i].ti = ti;
	  d[i].ts = ts;
     }


     inA = (R *) bench_malloc(N * sizeof(R));
     inB = (R *) bench_malloc(N * sizeof(R));
     inC = (R *) bench_malloc(N * sizeof(R));
     outA = (R *) bench_malloc(N * sizeof(R));
     outB = (R *) bench_malloc(N * sizeof(R));
     outC = (R *) bench_malloc(N * sizeof(R));
     tmp = (R *) bench_malloc(N * sizeof(R));

     nfo.p = p;
     nfo.probsz = p->sz;
     nfo.totalsz = tensor_append(p->vecsz, nfo.probsz);
     nfo.pckdsz = verify_pack(nfo.totalsz, 1);
     nfo.pckdvecsz = verify_pack(p->vecsz, tensor_sz(nfo.probsz));

     e->i = rimpulse(d, impulse_amp, n, vecn, &nfo,
		     inA, inB, inC, outA, outB, outC, tmp, rounds, tol);
     e->l = rlinear(N, &nfo, inA, inB, inC, outA, outB, outC, tmp, rounds,tol);
     e->s = t_shift(n, vecn, &nfo, inA, inB, outA, outB, tmp, 
		    rounds, tol, d);

     /* grr, verify-lib.c:preserves_input() only works for complex */
     if (!p->in_place && !p->destroy_input) {
	  bench_tensor *totalsz_swap, *pckdsz_swap;
	  totalsz_swap = tensor_copy_swapio(nfo.totalsz);
	  pckdsz_swap = tensor_copy_swapio(nfo.pckdsz);

	  for (i = 0; i < rounds; ++i) {
	       rarand(inA, N);
	       dofft(&nfo, inA, outB);
	       cpyr((R *) nfo.p->in, totalsz_swap, inB, pckdsz_swap);
	       racmp(inB, inA, N, "preserves_input", 0.0);
	  }

	  tensor_destroy(totalsz_swap);
	  tensor_destroy(pckdsz_swap);
     }

     tensor_destroy(nfo.totalsz);
     tensor_destroy(nfo.pckdsz);
     tensor_destroy(nfo.pckdvecsz);
     bench_free(tmp);
     bench_free(outC);
     bench_free(outB);
     bench_free(outA);
     bench_free(inC);
     bench_free(inB);
     bench_free(inA);
     bench_free(d);
}


typedef struct {
     dofft_closure k;
     bench_problem *p;
     int n0;
} dofft_r2r_closure;

static void cpyr1(int n, R *in, int is, R *out, int os, R scale)
{
     int i;
     for (i = 0; i < n; ++i)
	  out[i * os] = in[i * is] * scale;
}

static void mke00(C *a, int n, int c)
{
     int i;
     for (i = 1; i + i < n; ++i)
	  a[n - i][c] = a[i][c];
}

static void mkre00(C *a, int n)
{
     mkreal(a, n);
     mke00(a, n, 0);
}

static void mkimag(C *a, int n)
{
     int i;
     for (i = 0; i < n; ++i)
	  c_re(a[i]) = 0.0;
}

static void mko00(C *a, int n, int c)
{
     int i;
     a[0][c] = 0.0;
     for (i = 1; i + i < n; ++i)
	  a[n - i][c] = -a[i][c];
     if (i + i == n)
	  a[i][c] = 0.0;
}

static void mkro00(C *a, int n)
{
     mkreal(a, n);
     mko00(a, n, 0);
}

static void mkio00(C *a, int n)
{
     mkimag(a, n);
     mko00(a, n, 1);
}

static void mkre01(C *a, int n) /* n should be be multiple of 4 */
{
     R a0;
     a0 = c_re(a[0]);
     mko00(a, n/2, 0);
     c_re(a[n/2]) = -(c_re(a[0]) = a0);
     mkre00(a, n);
}

static void mkro01(C *a, int n) /* n should be be multiple of 4 */
{
     c_re(a[0]) = c_im(a[0]) = 0.0;
     mkre00(a, n/2);
     mkro00(a, n);
}

static void mkoddonly(C *a, int n)
{
     int i;
     for (i = 0; i < n; i += 2)
	  c_re(a[i]) = c_im(a[i]) = 0.0;
}

static void mkre10(C *a, int n)
{
     mkoddonly(a, n);
     mkre00(a, n);
}

static void mkio10(C *a, int n)
{
     mkoddonly(a, n);
     mkio00(a, n);
}

static void mkre11(C *a, int n)
{
     mkoddonly(a, n);
     mko00(a, n/2, 0);
     mkre00(a, n);
}

static void mkro11(C *a, int n)
{
     mkoddonly(a, n);
     mkre00(a, n/2);
     mkro00(a, n);
}

static void mkio11(C *a, int n)
{
     mkoddonly(a, n);
     mke00(a, n/2, 1);
     mkio00(a, n);
}

static void r2r_apply(dofft_closure *k_, bench_complex *in, bench_complex *out)
{
     dofft_r2r_closure *k = (dofft_r2r_closure *)k_;
     bench_problem *p = k->p;
     bench_real *ri, *ro;
     int n, is, os;

     n = p->sz->dims[0].n;
     is = p->sz->dims[0].is;
     os = p->sz->dims[0].os;

     ri = (bench_real *) p->in;
     ro = (bench_real *) p->out;

     switch (p->k[0]) {
	 case R2R_R2HC:
	      cpyr1(n, &c_re(in[0]), 2, ri, is, 1.0);
	      break;
	 case R2R_HC2R:
	      cpyr1(n/2 + 1, &c_re(in[0]), 2, ri, is, 1.0);
	      cpyr1((n+1)/2 - 1, &c_im(in[n-1]), -2, ri + is*(n-1), -is, 1.0);
	      break;
	 case R2R_REDFT00:
	      cpyr1(n, &c_re(in[0]), 2, ri, is, 1.0);
	      break;
	 case R2R_RODFT00:
	      cpyr1(n, &c_re(in[1]), 2, ri, is, 1.0);
	      break;
	 case R2R_REDFT01:
	      cpyr1(n, &c_re(in[0]), 2, ri, is, 1.0);
	      break;
	 case R2R_REDFT10:
	      cpyr1(n, &c_re(in[1]), 4, ri, is, 1.0);
	      break;
	 case R2R_RODFT01:
	      cpyr1(n, &c_re(in[1]), 2, ri, is, 1.0);
	      break;
	 case R2R_RODFT10:
	      cpyr1(n, &c_im(in[1]), 4, ri, is, 1.0);
	      break;
	 case R2R_REDFT11:
	      cpyr1(n, &c_re(in[1]), 4, ri, is, 1.0);
	      break;
	 case R2R_RODFT11:
	      cpyr1(n, &c_re(in[1]), 4, ri, is, 1.0);
	      break;
	 default:
	      BENCH_ASSERT(0); /* not yet implemented */
     }

     after_problem_rcopy_from(p, ri);
     doit(1, p);
     after_problem_rcopy_to(p, ro);

     switch (p->k[0]) {
	 case R2R_R2HC:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[0]), 2, 1.0);
	      cpyr1(n/2 + 1, ro, os, &c_re(out[0]), 2, 1.0);
	      cpyr1((n+1)/2 - 1, ro + os*(n-1), -os, &c_im(out[1]), 2, 1.0);
	      c_im(out[0]) = 0.0;
	      if (n % 2 == 0)
		   c_im(out[n/2]) = 0.0;
	      mkhermitian1(out, n);
	      break;
	 case R2R_HC2R:
	      if (k->k.recopy_input) {
		   cpyr1(n/2 + 1, ri, is, &c_re(in[0]), 2, 1.0);
		   cpyr1((n+1)/2 - 1, ri + is*(n-1), -is, &c_im(in[1]), 2,1.0);
	      }
	      cpyr1(n, ro, os, &c_re(out[0]), 2, 1.0);
	      mkreal(out, n);
	      break;
	 case R2R_REDFT00:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[0]), 2, 1.0);
	      cpyr1(n, ro, os, &c_re(out[0]), 2, 1.0);
	      mkre00(out, k->n0);
	      break;
	 case R2R_RODFT00:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_im(in[1]), 2, -1.0);
	      cpyr1(n, ro, os, &c_im(out[1]), 2, -1.0);
	      mkio00(out, k->n0);
	      break;
	 case R2R_REDFT01:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[0]), 2, 1.0);
	      cpyr1(n, ro, os, &c_re(out[1]), 4, 2.0);
	      mkre10(out, k->n0);
	      break;
	 case R2R_REDFT10:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[1]), 4, 2.0);
	      cpyr1(n, ro, os, &c_re(out[0]), 2, 1.0);
	      mkre01(out, k->n0);
	      break;
	 case R2R_RODFT01:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[1]), 2, 1.0);
	      cpyr1(n, ro, os, &c_im(out[1]), 4, -2.0);
	      mkio10(out, k->n0);
	      break;
	 case R2R_RODFT10:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_im(in[1]), 4, -2.0);
	      cpyr1(n, ro, os, &c_re(out[1]), 2, 1.0);
	      mkro01(out, k->n0);
	      break;
	 case R2R_REDFT11:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_re(in[1]), 4, 2.0);
	      cpyr1(n, ro, os, &c_re(out[1]), 4, 2.0);
	      mkre11(out, k->n0);
	      break;
	 case R2R_RODFT11:
	      if (k->k.recopy_input)
		   cpyr1(n, ri, is, &c_im(in[1]), 4, -2.0);
	      cpyr1(n, ro, os, &c_im(out[1]), 4, -2.0);
	      mkio11(out, k->n0);
	      break;
	 default:
	      BENCH_ASSERT(0); /* not yet implemented */
     }
}

void accuracy_r2r(bench_problem *p, int rounds, int impulse_rounds,
		  double t[6])
{
     dofft_r2r_closure k;
     int n, n0 = 1;
     C *a, *b;
     aconstrain constrain = 0;

     BENCH_ASSERT(p->kind == PROBLEM_R2R);
     BENCH_ASSERT(p->sz->rnk == 1);
     BENCH_ASSERT(p->vecsz->rnk == 0);

     k.k.apply = r2r_apply;
     k.k.recopy_input = 0;
     k.p = p;
     n = tensor_sz(p->sz);
     
     switch (p->k[0]) {
         case R2R_R2HC: constrain = mkreal; n0 = n; break;
         case R2R_HC2R: constrain = mkhermitian1; n0 = n; break;
         case R2R_REDFT00: constrain = mkre00; n0 = 2*(n-1); break;
         case R2R_RODFT00: constrain = mkro00; n0 = 2*(n+1); break;
         case R2R_REDFT01: constrain = mkre01; n0 = 4*n; break;
         case R2R_REDFT10: constrain = mkre10; n0 = 4*n; break;
         case R2R_RODFT01: constrain = mkro01; n0 = 4*n; break;
         case R2R_RODFT10: constrain = mkio10; n0 = 4*n; break;
         case R2R_REDFT11: constrain = mkre11; n0 = 8*n; break;
         case R2R_RODFT11: constrain = mkro11; n0 = 8*n; break;
	 default: BENCH_ASSERT(0); /* not yet implemented */
     }
     k.n0 = n0;

     a = (C *) bench_malloc(n0 * sizeof(C));
     b = (C *) bench_malloc(n0 * sizeof(C));
     accuracy_test(&k.k, constrain, -1, n0, a, b, rounds, impulse_rounds, t);
     bench_free(b);
     bench_free(a);
}
