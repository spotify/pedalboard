/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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

#ifndef __BENCH_USER_H__
#define __BENCH_USER_H__

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

/* benchmark program definitions for user code */
#include "config.h"
#include <limits.h>

#if HAVE_STDDEF_H
#include <stddef.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if defined(BENCHFFT_SINGLE)
typedef float bench_real;
#elif defined(BENCHFFT_LDOUBLE)
typedef long double bench_real;
#elif defined(BENCHFFT_QUAD)
typedef __float128 bench_real;
#else
typedef double bench_real;
#endif

typedef bench_real bench_complex[2];

#define c_re(c)  ((c)[0])
#define c_im(c)  ((c)[1])

#undef DOUBLE_PRECISION
#define DOUBLE_PRECISION (sizeof(bench_real) == sizeof(double))
#undef SINGLE_PRECISION
#define SINGLE_PRECISION (!DOUBLE_PRECISION && sizeof(bench_real) == sizeof(float))
#undef LDOUBLE_PRECISION
#define LDOUBLE_PRECISION (!DOUBLE_PRECISION && sizeof(bench_real) == sizeof(long double))

#undef QUAD_PRECISION
#ifdef BENCHFFT_QUAD
#define QUAD_PRECISION (!LDOUBLE_PRECISION && sizeof(bench_real) == sizeof(__float128))
#else
#define QUAD_PRECISION 0
#endif

typedef enum { PROBLEM_COMPLEX, PROBLEM_REAL, PROBLEM_R2R } problem_kind_t;

typedef enum {
     R2R_R2HC, R2R_HC2R, R2R_DHT,
     R2R_REDFT00, R2R_REDFT01, R2R_REDFT10, R2R_REDFT11,
     R2R_RODFT00, R2R_RODFT01, R2R_RODFT10, R2R_RODFT11
} r2r_kind_t;

typedef struct {
     int n;
     int is;			/* input stride */
     int os;			/* output stride */
} bench_iodim;

typedef struct {
     int rnk;
     bench_iodim *dims;
} bench_tensor;

bench_tensor *mktensor(int rnk);
void tensor_destroy(bench_tensor *sz);
size_t tensor_sz(const bench_tensor *sz);
bench_tensor *tensor_compress(const bench_tensor *sz);
int tensor_unitstridep(bench_tensor *t);
int tensor_rowmajorp(bench_tensor *t);
int tensor_real_rowmajorp(bench_tensor *t, int sign, int in_place);
bench_tensor *tensor_append(const bench_tensor *a, const bench_tensor *b);
bench_tensor *tensor_copy(const bench_tensor *sz);
bench_tensor *tensor_copy_sub(const bench_tensor *sz, int start_dim, int rnk);
bench_tensor *tensor_copy_swapio(const bench_tensor *sz);
void tensor_ibounds(bench_tensor *t, int *lbp, int *ubp);
void tensor_obounds(bench_tensor *t, int *lbp, int *ubp);

/*
  Definition of rank -infinity.
  This definition has the property that if you want rank 0 or 1,
  you can simply test for rank <= 1.  This is a common case.
 
  A tensor of rank -infinity has size 0.
*/
#define BENCH_RNK_MINFTY  INT_MAX
#define BENCH_FINITE_RNK(rnk) ((rnk) != BENCH_RNK_MINFTY)

typedef struct {
     problem_kind_t kind;
     r2r_kind_t *k;
     bench_tensor *sz;
     bench_tensor *vecsz;
     int sign;
     int in_place;
     int destroy_input;
     int split;
     void *in, *out;
     void *inphys, *outphys;
     int iphyssz, ophyssz;
     char *pstring;
     void *userinfo; /* user can store whatever */
     int scrambled_in, scrambled_out; /* hack for MPI */

     /* internal hack so that we can use verifier in FFTW test program */
     void *ini, *outi; /* if nonzero, point to imag. parts for dft */

     /* another internal hack to avoid passing around too many parameters */
     double setup_time;
} bench_problem;

extern int verbose;

extern int no_speed_allocation;

extern int always_pad_real;

#define LIBBENCH_TIMER 0
#define USER_TIMER 1
#define BENCH_NTIMERS 2
extern void timer_start(int which_timer);
extern double timer_stop(int which_timer);

extern int can_do(bench_problem *p);
extern void setup(bench_problem *p);
extern void doit(int iter, bench_problem *p);
extern void done(bench_problem *p);
extern void main_init(int *argc, char ***argv);
extern void cleanup(void);
extern void verify(const char *param, int rounds, double tol);
extern void useropt(const char *arg);

extern void verify_problem(bench_problem *p, int rounds, double tol);

extern void problem_alloc(bench_problem *p);
extern void problem_free(bench_problem *p);
extern void problem_zero(bench_problem *p);
extern void problem_destroy(bench_problem *p);

extern int power_of_two(int n);
extern int log_2(int n);


#define CASSIGN(out, in) (c_re(out) = c_re(in), c_im(out) = c_im(in))

bench_tensor *verify_pack(const bench_tensor *sz, int s);

typedef struct {
     double l;
     double i;
     double s;
} errors;

void verify_dft(bench_problem *p, int rounds, double tol, errors *e);
void verify_rdft2(bench_problem *p, int rounds, double tol, errors *e);
void verify_r2r(bench_problem *p, int rounds, double tol, errors *e);

/**************************************************************/
/* routines to override */

extern void after_problem_ccopy_from(bench_problem *p, bench_real *ri, bench_real *ii);
extern void after_problem_ccopy_to(bench_problem *p, bench_real *ro, bench_real *io);
extern void after_problem_hccopy_from(bench_problem *p, bench_real *ri, bench_real *ii);
extern void after_problem_hccopy_to(bench_problem *p, bench_real *ro, bench_real *io);
extern void after_problem_rcopy_from(bench_problem *p, bench_real *ri);
extern void after_problem_rcopy_to(bench_problem *p, bench_real *ro);
extern void bench_exit(int status);
extern double bench_cost_postprocess(double cost);

/**************************************************************
 * malloc
 **************************************************************/
extern void *bench_malloc(size_t size);
extern void bench_free(void *ptr);
extern void bench_free0(void *ptr);

/**************************************************************
 * alloca
 **************************************************************/
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

/**************************************************************
 * assert
 **************************************************************/
extern void bench_assertion_failed(const char *s, int line, const char *file);
#define BENCH_ASSERT(ex)						 \
      (void)((ex) || (bench_assertion_failed(#ex, __LINE__, __FILE__), 0))

#define UNUSED(x) (void)x

/***************************************
 * Documentation strings
 ***************************************/
struct bench_doc {
     const char *key;
     const char *val;
     const char *(*f)(void);
};

extern struct bench_doc bench_doc[];

#ifdef CC
#define CC_DOC BENCH_DOC("cc", CC)
#elif defined(BENCH_CC)
#define CC_DOC BENCH_DOC("cc", BENCH_CC)
#else
#define CC_DOC /* none */
#endif

#ifdef CXX
#define CXX_DOC BENCH_DOC("cxx", CXX)
#elif defined(BENCH_CXX)
#define CXX_DOC BENCH_DOC("cxx", BENCH_CXX)
#else
#define CXX_DOC /* none */
#endif

#ifdef F77
#define F77_DOC BENCH_DOC("f77", F77)
#elif defined(BENCH_F77)
#define F77_DOC BENCH_DOC("f77", BENCH_F77)
#else
#define F77_DOC /* none */
#endif

#ifdef F90
#define F90_DOC BENCH_DOC("f90", F90)
#elif defined(BENCH_F90)
#define F90_DOC BENCH_DOC("f90", BENCH_F90)
#else
#define F90_DOC /* none */
#endif

#define BEGIN_BENCH_DOC						\
struct bench_doc bench_doc[] = {				\
    CC_DOC							\
    CXX_DOC							\
    F77_DOC							\
    F90_DOC

#define BENCH_DOC(key, val) { key, val, 0 },
#define BENCH_DOCF(key, f) { key, 0, f },

#define END_BENCH_DOC				\
     {0, 0, 0}};

#ifdef __cplusplus
}                               /* extern "C" */
#endif                          /* __cplusplus */
    
#endif /* __BENCH_USER_H__ */
