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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Utility functions:
 */
static double dabs(double x) { return (x < 0.0) ? -x : x; }
static double dmin(double x, double y) { return (x < y) ? x : y; }
static double norm2(double x, double y) { return dmax(dabs(x), dabs(y)); }

double dmax(double x, double y) { return (x > y) ? x : y; }

static double aerror(C *a, C *b, int n)
{
     if (n > 0) {
	  /* compute the relative Linf error */
	  double e = 0.0, mag = 0.0;
	  int i;

	  for (i = 0; i < n; ++i) {
	       e = dmax(e, norm2(c_re(a[i]) - c_re(b[i]),
				 c_im(a[i]) - c_im(b[i])));
	       mag = dmax(mag, 
			  dmin(norm2(c_re(a[i]), c_im(a[i])),
			       norm2(c_re(b[i]), c_im(b[i]))));
	  }
	  e /= mag;

#ifdef HAVE_ISNAN
	  BENCH_ASSERT(!isnan(e));
#endif
	  return e;
     } else
	  return 0.0;
}

#ifdef HAVE_DRAND48
#  if defined(HAVE_DECL_DRAND48) && !HAVE_DECL_DRAND48
extern double drand48(void);
#  endif
double mydrand(void)
{
     return drand48() - 0.5;
}
#else
double mydrand(void)
{
     double d = rand();
     return (d / (double) RAND_MAX) - 0.5;
}
#endif

void arand(C *a, int n)
{
     int i;

     /* generate random inputs */
     for (i = 0; i < n; ++i) {
	  c_re(a[i]) = mydrand();
	  c_im(a[i]) = mydrand();
     }
}

/* make array real */
void mkreal(C *A, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
          c_im(A[i]) = 0.0;
     }
}

static void assign_conj(C *Ac, C *A, int rank, const bench_iodim *dim, int stride)
{
     if (rank == 0) {
          c_re(*Ac) = c_re(*A);
          c_im(*Ac) = -c_im(*A);
     }
     else {
          int i, n0 = dim[rank - 1].n, s = stride;
          rank -= 1;
	  stride *= n0;
          assign_conj(Ac, A, rank, dim, stride);
          for (i = 1; i < n0; ++i)
               assign_conj(Ac + (n0 - i) * s, A + i * s, rank, dim, stride);
     }
}

/* make array hermitian */
void mkhermitian(C *A, int rank, const bench_iodim *dim, int stride)
{
     if (rank == 0)
          c_im(*A) = 0.0;
     else {
          int i, n0 = dim[rank - 1].n, s = stride;
          rank -= 1;
	  stride *= n0;
          mkhermitian(A, rank, dim, stride);
          for (i = 1; 2*i < n0; ++i)
               assign_conj(A + (n0 - i) * s, A + i * s, rank, dim, stride);
          if (2*i == n0)
               mkhermitian(A + i * s, rank, dim, stride);
     }
}

void mkhermitian1(C *a, int n)
{
     bench_iodim d;

     d.n = n;
     d.is = d.os = 1;
     mkhermitian(a, 1, &d, 1);
}

/* C = A */
void acopy(C *c, C *a, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  c_re(c[i]) = c_re(a[i]);
	  c_im(c[i]) = c_im(a[i]);
     }
}

/* C = A + B */
void aadd(C *c, C *a, C *b, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  c_re(c[i]) = c_re(a[i]) + c_re(b[i]);
	  c_im(c[i]) = c_im(a[i]) + c_im(b[i]);
     }
}

/* C = A - B */
void asub(C *c, C *a, C *b, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  c_re(c[i]) = c_re(a[i]) - c_re(b[i]);
	  c_im(c[i]) = c_im(a[i]) - c_im(b[i]);
     }
}

/* B = rotate left A (complex) */
void arol(C *b, C *a, int n, int nb, int na)
{
     int i, ib, ia;

     for (ib = 0; ib < nb; ++ib) {
	  for (i = 0; i < n - 1; ++i)
	       for (ia = 0; ia < na; ++ia) {
		    C *pb = b + (ib * n + i) * na + ia;
		    C *pa = a + (ib * n + i + 1) * na + ia;
		    c_re(*pb) = c_re(*pa);
		    c_im(*pb) = c_im(*pa);
	       }

	  for (ia = 0; ia < na; ++ia) {
	       C *pb = b + (ib * n + n - 1) * na + ia;
	       C *pa = a + ib * n * na + ia;
	       c_re(*pb) = c_re(*pa);
	       c_im(*pb) = c_im(*pa);
	  }
     }
}

void aphase_shift(C *b, C *a, int n, int nb, int na, double sign)
{
     int j, jb, ja;
     trigreal twopin;
     twopin = K2PI / n;

     for (jb = 0; jb < nb; ++jb)
	  for (j = 0; j < n; ++j) {
	       trigreal s = sign * SIN(j * twopin);
	       trigreal c = COS(j * twopin);

	       for (ja = 0; ja < na; ++ja) {
		    int k = (jb * n + j) * na + ja;
		    c_re(b[k]) = c_re(a[k]) * c - c_im(a[k]) * s;
		    c_im(b[k]) = c_re(a[k]) * s + c_im(a[k]) * c;
	       }
	  }
}

/* A = alpha * A  (complex, in place) */
void ascale(C *a, C alpha, int n)
{
     int i;

     for (i = 0; i < n; ++i) {
	  R xr = c_re(a[i]), xi = c_im(a[i]);
	  c_re(a[i]) = xr * c_re(alpha) - xi * c_im(alpha);
	  c_im(a[i]) = xr * c_im(alpha) + xi * c_re(alpha);
     }
}


double acmp(C *a, C *b, int n, const char *test, double tol)
{
     double d = aerror(a, b, n);
     if (d > tol) {
	  ovtpvt_err("Found relative error %e (%s)\n", d, test);

	  {
	       int i, N;
	       N = n > 300 && verbose <= 2 ? 300 : n;
	       for (i = 0; i < N; ++i) 
		    ovtpvt_err("%8d %16.12f %16.12f   %16.12f %16.12f\n", i, 
			       (double) c_re(a[i]), (double) c_im(a[i]),
			       (double) c_re(b[i]), (double) c_im(b[i]));
	  }

	  bench_exit(EXIT_FAILURE);
     }
     return d;
}


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

static double impulse0(dofft_closure *k,
		       int n, int vecn, 
		       C *inA, C *inB, C *inC,
		       C *outA, C *outB, C *outC,
		       C *tmp, int rounds, double tol)
{
     int N = n * vecn;
     double e = 0.0;
     int j;

     k->apply(k, inA, tmp);
     e = dmax(e, acmp(tmp, outA, N, "impulse 1", tol));

     for (j = 0; j < rounds; ++j) {
	  arand(inB, N);
	  asub(inC, inA, inB, N);
	  k->apply(k, inB, outB);
	  k->apply(k, inC, outC);
	  aadd(tmp, outB, outC, N);
	  e = dmax(e, acmp(tmp, outA, N, "impulse", tol));
     }
     return e;
}

double impulse(dofft_closure *k,
	       int n, int vecn, 
	       C *inA, C *inB, C *inC,
	       C *outA, C *outB, C *outC,
	       C *tmp, int rounds, double tol)
{
     int i, j;
     double e = 0.0;

     /* check impulsive input */
     for (i = 0; i < vecn; ++i) {
	  R x = (sqrt(n)*(i+1)) / (double)(vecn+1);
	  for (j = 0; j < n; ++j) {
	       c_re(inA[j + i * n]) = 0;
	       c_im(inA[j + i * n]) = 0;
	       c_re(outA[j + i * n]) = x;
	       c_im(outA[j + i * n]) = 0;
	  }
	  c_re(inA[i * n]) = x;
	  c_im(inA[i * n]) = 0;
     }

     e = dmax(e, impulse0(k, n, vecn, inA, inB, inC, outA, outB, outC,
			  tmp, rounds, tol));

     /* check constant input */
     for (i = 0; i < vecn; ++i) {
	  R x = (i+1) / ((double)(vecn+1) * sqrt(n));
	  for (j = 0; j < n; ++j) {
	       c_re(inA[j + i * n]) = x;
	       c_im(inA[j + i * n]) = 0;
	       c_re(outA[j + i * n]) = 0;
	       c_im(outA[j + i * n]) = 0;
	  }
	  c_re(outA[i * n]) = n * x;
	  c_im(outA[i * n]) = 0;
     }

     e = dmax(e, impulse0(k, n, vecn, inA, inB, inC, outA, outB, outC,
			  tmp, rounds, tol));
     return e;
}

double linear(dofft_closure *k, int realp,
	      int n, C *inA, C *inB, C *inC, C *outA,
	      C *outB, C *outC, C *tmp, int rounds, double tol)
{
     int j;
     double e = 0.0;

     for (j = 0; j < rounds; ++j) {
	  C alpha, beta;
	  c_re(alpha) = mydrand();
	  c_im(alpha) = realp ? 0.0 : mydrand();
	  c_re(beta) = mydrand();
	  c_im(beta) = realp ? 0.0 : mydrand();
	  arand(inA, n);
	  arand(inB, n);
	  k->apply(k, inA, outA);
	  k->apply(k, inB, outB);

	  ascale(outA, alpha, n);
	  ascale(outB, beta, n);
	  aadd(tmp, outA, outB, n);
	  ascale(inA, alpha, n);
	  ascale(inB, beta, n);
	  aadd(inC, inA, inB, n);
	  k->apply(k, inC, outC);

	  e = dmax(e, acmp(outC, tmp, n, "linear", tol));
     }
     return e;
}



double tf_shift(dofft_closure *k,
		int realp, const bench_tensor *sz,
		int n, int vecn, double sign,
		C *inA, C *inB, C *outA, C *outB, C *tmp,
		int rounds, double tol, int which_shift)
{
     int nb, na, dim, N = n * vecn;
     int i, j;
     double e = 0.0;

     /* test 3: check the time-shift property */
     /* the paper performs more tests, but this code should be fine too */

     nb = 1;
     na = n;

     /* check shifts across all SZ dimensions */
     for (dim = 0; dim < sz->rnk; ++dim) {
	  int ncur = sz->dims[dim].n;

	  na /= ncur;

	  for (j = 0; j < rounds; ++j) {
	       arand(inA, N);

	       if (which_shift == TIME_SHIFT) {
		    for (i = 0; i < vecn; ++i) {
			 if (realp) mkreal(inA + i * n, n);
			 arol(inB + i * n, inA + i * n, ncur, nb, na);
		    }
		    k->apply(k, inA, outA);
		    k->apply(k, inB, outB);
		    for (i = 0; i < vecn; ++i) 
			 aphase_shift(tmp + i * n, outB + i * n, ncur, 
				      nb, na, sign);
		    e = dmax(e, acmp(tmp, outA, N, "time shift", tol));
	       } else {
		    for (i = 0; i < vecn; ++i) {
			 if (realp) 
			      mkhermitian(inA + i * n, sz->rnk, sz->dims, 1);
			 aphase_shift(inB + i * n, inA + i * n, ncur,
				      nb, na, -sign);
		    }
		    k->apply(k, inA, outA);
		    k->apply(k, inB, outB);
		    for (i = 0; i < vecn; ++i) 
			 arol(tmp + i * n, outB + i * n, ncur, nb, na);
		    e = dmax(e, acmp(tmp, outA, N, "freq shift", tol));
	       }
	  }

	  nb *= ncur;
     }
     return e;
}


void preserves_input(dofft_closure *k, aconstrain constrain,
		     int n, C *inA, C *inB, C *outB, int rounds)
{
     int j;
     int recopy_input = k->recopy_input;

     k->recopy_input = 1;
     for (j = 0; j < rounds; ++j) {
	  arand(inA, n);
	  if (constrain)
	       constrain(inA, n);
	  
	  acopy(inB, inA, n);
	  k->apply(k, inB, outB);
	  acmp(inB, inA, n, "preserves_input", 0.0);
     }
     k->recopy_input = recopy_input;
}


/* Make a copy of the size tensor, with the same dimensions, but with
   the strides corresponding to a "packed" row-major array with the
   given stride. */
bench_tensor *verify_pack(const bench_tensor *sz, int s)
{
     bench_tensor *x = tensor_copy(sz);
     if (BENCH_FINITE_RNK(x->rnk) && x->rnk > 0) {
	  int i;
	  x->dims[x->rnk - 1].is = s;
	  x->dims[x->rnk - 1].os = s;
	  for (i = x->rnk - 1; i > 0; --i) {
	       x->dims[i - 1].is = x->dims[i].is * x->dims[i].n;
	       x->dims[i - 1].os = x->dims[i].os * x->dims[i].n;
	  }
     }
     return x;
}

static int all_zero(C *a, int n)
{
     int i;
     for (i = 0; i < n; ++i)
	  if (c_re(a[i]) != 0.0 || c_im(a[i]) != 0.0)
	       return 0;
     return 1;
}

static int one_accuracy_test(dofft_closure *k, aconstrain constrain,
			     int sign, int n, C *a, C *b, 
			     double t[6])
{
     double err[6];

     if (constrain)
	  constrain(a, n);
     
     if (all_zero(a, n))
	  return 0;
     
     k->apply(k, a, b);
     fftaccuracy(n, a, b, sign, err);
     
     t[0] += err[0];
     t[1] += err[1] * err[1];
     t[2] = dmax(t[2], err[2]);
     t[3] += err[3];
     t[4] += err[4] * err[4];
     t[5] = dmax(t[5], err[5]);

     return 1;
}

void accuracy_test(dofft_closure *k, aconstrain constrain,
		   int sign, int n, C *a, C *b, int rounds, int impulse_rounds,
		   double t[6])
{
     int r, i;
     int ntests = 0;
     bench_complex czero = {0, 0};

     for (i = 0; i < 6; ++i) t[i] = 0.0;

     for (r = 0; r < rounds; ++r) {
	  arand(a, n);
	  if (one_accuracy_test(k, constrain, sign, n, a, b, t))
	       ++ntests;
     }

     /* impulses at beginning of array */
     for (r = 0; r < impulse_rounds; ++r) {
	  if (r > n - r - 1)
	       continue;
	  
	  caset(a, n, czero);
	  c_re(a[r]) = c_im(a[r]) = 1.0;
	  
	  if (one_accuracy_test(k, constrain, sign, n, a, b, t))
	       ++ntests;
     }
     
     /* impulses at end of array */
     for (r = 0; r < impulse_rounds; ++r) {
	  if (r <= n - r - 1)
	       continue;
	  
	  caset(a, n, czero);
	  c_re(a[n - r - 1]) = c_im(a[n - r - 1]) = 1.0;
	  
	  if (one_accuracy_test(k, constrain, sign, n, a, b, t))
	       ++ntests;
     }
     
     /* randomly-located impulses */
     for (r = 0; r < impulse_rounds; ++r) {
	  caset(a, n, czero);
	  i = rand() % n;
	  c_re(a[i]) = c_im(a[i]) = 1.0;
	  
	  if (one_accuracy_test(k, constrain, sign, n, a, b, t))
	       ++ntests;
     }

     t[0] /= ntests;
     t[1] = sqrt(t[1] / ntests);
     t[3] /= ntests;
     t[4] = sqrt(t[4] / ntests);

     fftaccuracy_done();
}
