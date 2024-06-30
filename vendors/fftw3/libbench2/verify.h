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

#include "libbench2/bench.h"

typedef bench_real R;
typedef bench_complex C;

typedef struct dofft_closure_s {
     void (*apply)(struct dofft_closure_s *k,
		   bench_complex *in, bench_complex *out);
     int recopy_input;
} dofft_closure;

double dmax(double x, double y);

typedef void (*aconstrain)(C *a, int n);

void arand(C *a, int n);
void mkreal(C *A, int n);
void mkhermitian(C *A, int rank, const bench_iodim *dim, int stride);
void mkhermitian1(C *a, int n);
void aadd(C *c, C *a, C *b, int n);
void asub(C *c, C *a, C *b, int n);
void arol(C *b, C *a, int n, int nb, int na);
void aphase_shift(C *b, C *a, int n, int nb, int na, double sign);
void ascale(C *a, C alpha, int n);
double acmp(C *a, C *b, int n, const char *test, double tol);
double mydrand(void);
double impulse(dofft_closure *k,
	       int n, int vecn, 
	       C *inA, C *inB, C *inC,
	       C *outA, C *outB, C *outC,
	       C *tmp, int rounds, double tol);
double linear(dofft_closure *k, int realp,
	      int n, C *inA, C *inB, C *inC, C *outA,
	      C *outB, C *outC, C *tmp, int rounds, double tol);
void preserves_input(dofft_closure *k, aconstrain constrain,
                     int n, C *inA, C *inB, C *outB, int rounds);

enum { TIME_SHIFT, FREQ_SHIFT };
double tf_shift(dofft_closure *k, int realp, const bench_tensor *sz,
		int n, int vecn, double sign,
		C *inA, C *inB, C *outA, C *outB, C *tmp,
		int rounds, double tol, int which_shift);

typedef struct dotens2_closure_s {
     void (*apply)(struct dotens2_closure_s *k, 
		   int indx0, int ondx0, int indx1, int ondx1);
} dotens2_closure;

void bench_dotens2(const bench_tensor *sz0, 
		   const bench_tensor *sz1, dotens2_closure *k);

void accuracy_test(dofft_closure *k, aconstrain constrain,
		   int sign, int n, C *a, C *b, int rounds, int impulse_rounds,
		   double t[6]);

void accuracy_dft(bench_problem *p, int rounds, int impulse_rounds,
		  double t[6]);
void accuracy_rdft2(bench_problem *p, int rounds, int impulse_rounds,
		    double t[6]);
void accuracy_r2r(bench_problem *p, int rounds, int impulse_rounds,
		  double t[6]);

#if defined(BENCHFFT_LDOUBLE) && HAVE_COSL
   typedef long double trigreal;
#  define COS cosl
#  define SIN sinl
#  define TAN tanl
#  define KTRIG(x) (x##L)
#elif defined(BENCHFFT_QUAD) && HAVE_LIBQUADMATH
   typedef __float128 trigreal;
#  define COS cosq
#  define SIN sinq
#  define TAN tanq
#  define KTRIG(x) (x##Q)
extern trigreal cosq(trigreal);
extern trigreal sinq(trigreal);
extern trigreal tanq(trigreal);
#else
   typedef double trigreal;
#  define COS cos
#  define SIN sin
#  define TAN tan
#  define KTRIG(x) (x)
#endif
#define K2PI KTRIG(6.2831853071795864769252867665590057683943388)
