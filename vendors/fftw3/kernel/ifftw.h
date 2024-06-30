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


/* FFTW internal header file */
#ifndef __IFFTW_H__
#define __IFFTW_H__

#include "config.h"

#include <stdlib.h>		/* size_t */
#include <stdarg.h>		/* va_list */
#include <stddef.h>             /* ptrdiff_t */
#include <limits.h>             /* INT_MAX */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_STDINT_H
# include <stdint.h>             /* uintptr_t, maybe */
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>           /* uintptr_t, maybe */
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Windows annoyances -- since tests/hook.c uses some internal
   FFTW functions, we need to given them the dllexport attribute
   under Windows when compiling as a DLL (see api/fftw3.h). */
#if defined(FFTW_EXTERN)
#  define IFFTW_EXTERN FFTW_EXTERN
#elif (defined(FFTW_DLL) || defined(DLL_EXPORT)) \
 && (defined(_WIN32) || defined(__WIN32__))
#  define IFFTW_EXTERN extern __declspec(dllexport)
#else
#  define IFFTW_EXTERN extern
#endif

/* determine precision and name-mangling scheme */
#define CONCAT(prefix, name) prefix ## name
#if defined(FFTW_SINGLE)
  typedef float R;
# define X(name) CONCAT(fftwf_, name)
#elif defined(FFTW_LDOUBLE)
  typedef long double R;
# define X(name) CONCAT(fftwl_, name)
# define TRIGREAL_IS_LONG_DOUBLE
#elif defined(FFTW_QUAD)
  typedef __float128 R;
# define X(name) CONCAT(fftwq_, name)
# define TRIGREAL_IS_QUAD
#else
  typedef double R;
# define X(name) CONCAT(fftw_, name)
#endif

/*
  integral type large enough to contain a stride (what ``int'' should
  have been in the first place.
*/
typedef ptrdiff_t INT;

/* dummy use of unused parameters to silence compiler warnings */
#define UNUSED(x) (void)x

#define NELEM(array) ((sizeof(array) / sizeof((array)[0])))

#define FFT_SIGN (-1)  /* sign convention for forward transforms */
extern void X(extract_reim)(int sign, R *c, R **r, R **i);

#define REGISTER_SOLVER(p, s) X(solver_register)(p, s)

#define STRINGIZEx(x) #x
#define STRINGIZE(x) STRINGIZEx(x)
#define CIMPLIES(ante, post) (!(ante) || (post))

/* define HAVE_SIMD if any simd extensions are supported */
#if defined(HAVE_SSE) || defined(HAVE_SSE2) || \
      defined(HAVE_AVX) || defined(HAVE_AVX_128_FMA) || \
      defined(HAVE_AVX2) || defined(HAVE_AVX512) || \
      defined(HAVE_KCVI) || \
      defined(HAVE_ALTIVEC) || defined(HAVE_VSX) || \
      defined(HAVE_MIPS_PS) || \
      defined(HAVE_GENERIC_SIMD128) || defined(HAVE_GENERIC_SIMD256)
#define HAVE_SIMD 1
#else
#define HAVE_SIMD 0
#endif

extern int X(have_simd_sse2)(void);
extern int X(have_simd_avx)(void);
extern int X(have_simd_avx_128_fma)(void);
extern int X(have_simd_avx2)(void);
extern int X(have_simd_avx2_128)(void);
extern int X(have_simd_avx512)(void);
extern int X(have_simd_altivec)(void);
extern int X(have_simd_vsx)(void);
extern int X(have_simd_neon)(void);

/* forward declarations */
typedef struct problem_s problem;
typedef struct plan_s plan;
typedef struct solver_s solver;
typedef struct planner_s planner;
typedef struct printer_s printer;
typedef struct scanner_s scanner;

/*-----------------------------------------------------------------------*/
/* alloca: */
#if HAVE_SIMD
#  if defined(HAVE_KCVI) || defined(HAVE_AVX512)
#    define MIN_ALIGNMENT 64
#  elif defined(HAVE_AVX) || defined(HAVE_AVX2) || defined(HAVE_GENERIC_SIMD256)
#    define MIN_ALIGNMENT 32  /* best alignment for AVX, conservative for
			       * everything else */
#  else
     /* Note that we cannot use 32-byte alignment for all SIMD.  For
	example, MacOS X malloc is 16-byte aligned, but there was no
	posix_memalign in MacOS X until version 10.6. */
#    define MIN_ALIGNMENT 16
#  endif
#endif

#if defined(HAVE_ALLOCA) && defined(FFTW_ENABLE_ALLOCA)
   /* use alloca if available */

#ifndef alloca
#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  if HAVE_ALLOCA_H
#   include <alloca.h>
#  else
#   ifdef _AIX
 #pragma alloca
#   else
#    ifndef alloca /* predefined by HP cc +Olibcalls */
void *alloca(size_t);
#    endif
#   endif
#  endif
# endif
#endif
#endif

#  ifdef MIN_ALIGNMENT
#    define STACK_MALLOC(T, p, n)				\
     {								\
         p = (T)alloca((n) + MIN_ALIGNMENT);			\
         p = (T)(((uintptr_t)p + (MIN_ALIGNMENT - 1)) &	\
               (~(uintptr_t)(MIN_ALIGNMENT - 1)));		\
     }
#    define STACK_FREE(n) 
#  else /* HAVE_ALLOCA && !defined(MIN_ALIGNMENT) */
#    define STACK_MALLOC(T, p, n) p = (T)alloca(n) 
#    define STACK_FREE(n) 
#  endif

#else /* ! HAVE_ALLOCA */
   /* use malloc instead of alloca */
#  define STACK_MALLOC(T, p, n) p = (T)MALLOC(n, OTHER)
#  define STACK_FREE(n) X(ifree)(n)
#endif /* ! HAVE_ALLOCA */

/* allocation of buffers.  If these grow too large use malloc(), else
   use STACK_MALLOC (hopefully reducing to alloca()). */

/* 64KiB ought to be enough for anybody */
#define MAX_STACK_ALLOC ((size_t)64 * 1024)

#define BUF_ALLOC(T, p, n)			\
{						\
     if (n < MAX_STACK_ALLOC) {			\
	  STACK_MALLOC(T, p, n);		\
     } else {					\
	  p = (T)MALLOC(n, BUFFERS);		\
     }						\
}

#define BUF_FREE(p, n)				\
{						\
     if (n < MAX_STACK_ALLOC) {			\
	  STACK_FREE(p);			\
     } else {					\
	  X(ifree)(p);				\
     }						\
}

/*-----------------------------------------------------------------------*/
/* define uintptr_t if it is not already defined */

#ifndef HAVE_UINTPTR_T
#  if SIZEOF_VOID_P == 0
#    error sizeof void* is unknown!
#  elif SIZEOF_UNSIGNED_INT == SIZEOF_VOID_P
     typedef unsigned int uintptr_t;
#  elif SIZEOF_UNSIGNED_LONG == SIZEOF_VOID_P
     typedef unsigned long uintptr_t;
#  elif SIZEOF_UNSIGNED_LONG_LONG == SIZEOF_VOID_P
     typedef unsigned long long uintptr_t;
#  else
#    error no unsigned integer type matches void* sizeof!
#  endif
#endif

/*-----------------------------------------------------------------------*/
/* We can do an optimization for copying pairs of (aligned) floats
   when in single precision if 2*float = double. */

#define FFTW_2R_IS_DOUBLE (defined(FFTW_SINGLE) \
                           && SIZEOF_FLOAT != 0 \
                           && SIZEOF_DOUBLE == 2*SIZEOF_FLOAT)

#define DOUBLE_ALIGNED(p) ((((uintptr_t)(p)) % sizeof(double)) == 0)

/*-----------------------------------------------------------------------*/
/* assert.c: */
IFFTW_EXTERN void X(assertion_failed)(const char *s, 
				      int line, const char *file);

/* always check */
#define CK(ex)						 \
      (void)((ex) || (X(assertion_failed)(#ex, __LINE__, __FILE__), 0))

#ifdef FFTW_DEBUG
/* check only if debug enabled */
#define A(ex)						 \
      (void)((ex) || (X(assertion_failed)(#ex, __LINE__, __FILE__), 0))
#else
#define A(ex) /* nothing */
#endif

extern void X(debug)(const char *format, ...);
#define D X(debug)

/*-----------------------------------------------------------------------*/
/* kalloc.c: */
extern void *X(kernel_malloc)(size_t n);
extern void X(kernel_free)(void *p);

/*-----------------------------------------------------------------------*/
/* alloc.c: */

/* objects allocated by malloc, for statistical purposes */
enum malloc_tag {
     EVERYTHING,
     PLANS,
     SOLVERS,
     PROBLEMS,
     BUFFERS,
     HASHT,
     TENSORS,
     PLANNERS,
     SLVDESCS,
     TWIDDLES,
     STRIDES,
     OTHER,
     MALLOC_WHAT_LAST		/* must be last */
};

IFFTW_EXTERN void X(ifree)(void *ptr);
extern void X(ifree0)(void *ptr);

IFFTW_EXTERN void *X(malloc_plain)(size_t sz);
#define MALLOC(n, what)  X(malloc_plain)(n)

/*-----------------------------------------------------------------------*/
/* low-resolution clock */

#ifdef FAKE_CRUDE_TIME
 typedef int crude_time;
#else
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif

# ifdef HAVE_BSDGETTIMEOFDAY
# ifndef HAVE_GETTIMEOFDAY
# define gettimeofday BSDgettimeofday
# define HAVE_GETTIMEOFDAY 1
# endif
# endif

# if defined(HAVE_GETTIMEOFDAY)
   typedef struct timeval crude_time;
# else
   typedef clock_t crude_time;
# endif
#endif /* else FAKE_CRUDE_TIME */

crude_time X(get_crude_time)(void);
double X(elapsed_since)(const planner *plnr, const problem *p,
			crude_time t0); /* time in seconds since t0 */

/*-----------------------------------------------------------------------*/
/* ops.c: */
/*
 * ops counter.  The total number of additions is add + fma
 * and the total number of multiplications is mul + fma.
 * Total flops = add + mul + 2 * fma
 */
typedef struct {
     double add;
     double mul;
     double fma;
     double other;
} opcnt;

void X(ops_zero)(opcnt *dst);
void X(ops_other)(INT o, opcnt *dst);
void X(ops_cpy)(const opcnt *src, opcnt *dst);

void X(ops_add)(const opcnt *a, const opcnt *b, opcnt *dst);
void X(ops_add2)(const opcnt *a, opcnt *dst);

/* dst = m * a + b */
void X(ops_madd)(INT m, const opcnt *a, const opcnt *b, opcnt *dst);

/* dst += m * a */
void X(ops_madd2)(INT m, const opcnt *a, opcnt *dst);


/*-----------------------------------------------------------------------*/
/* minmax.c: */
INT X(imax)(INT a, INT b);
INT X(imin)(INT a, INT b);

/*-----------------------------------------------------------------------*/
/* iabs.c: */
INT X(iabs)(INT a);

/* inline version */
#define IABS(x) (((x) < 0) ? (0 - (x)) : (x))

/*-----------------------------------------------------------------------*/
/* md5.c */

#if SIZEOF_UNSIGNED_INT >= 4
typedef unsigned int md5uint;
#else
typedef unsigned long md5uint; /* at least 32 bits as per C standard */
#endif

typedef md5uint md5sig[4];

typedef struct {
     md5sig s; /* state and signature */

     /* fields not meant to be used outside md5.c: */
     unsigned char c[64]; /* stuff not yet processed */
     unsigned l;  /* total length.  Should be 64 bits long, but this is
		     good enough for us */
} md5;

void X(md5begin)(md5 *p);
void X(md5putb)(md5 *p, const void *d_, size_t len);
void X(md5puts)(md5 *p, const char *s);
void X(md5putc)(md5 *p, unsigned char c);
void X(md5int)(md5 *p, int i);
void X(md5INT)(md5 *p, INT i);
void X(md5unsigned)(md5 *p, unsigned i);
void X(md5end)(md5 *p);

/*-----------------------------------------------------------------------*/
/* tensor.c: */
#define STRUCT_HACK_KR
#undef STRUCT_HACK_C99

typedef struct {
     INT n;
     INT is;			/* input stride */
     INT os;			/* output stride */
} iodim;

typedef struct {
     int rnk;
#if defined(STRUCT_HACK_KR)
     iodim dims[1];
#elif defined(STRUCT_HACK_C99)
     iodim dims[];
#else
     iodim *dims;
#endif
} tensor;

/*
  Definition of rank -infinity.
  This definition has the property that if you want rank 0 or 1,
  you can simply test for rank <= 1.  This is a common case.
 
  A tensor of rank -infinity has size 0.
*/
#define RNK_MINFTY  INT_MAX
#define FINITE_RNK(rnk) ((rnk) != RNK_MINFTY)

typedef enum { INPLACE_IS, INPLACE_OS } inplace_kind;

tensor *X(mktensor)(int rnk);
tensor *X(mktensor_0d)(void);
tensor *X(mktensor_1d)(INT n, INT is, INT os);
tensor *X(mktensor_2d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1);
tensor *X(mktensor_3d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1,
		       INT n2, INT is2, INT os2);
tensor *X(mktensor_4d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1,
		       INT n2, INT is2, INT os2,
		       INT n3, INT is3, INT os3);
tensor *X(mktensor_5d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1,
		       INT n2, INT is2, INT os2,
		       INT n3, INT is3, INT os3,
		       INT n4, INT is4, INT os4);
INT X(tensor_sz)(const tensor *sz);
void X(tensor_md5)(md5 *p, const tensor *t);
INT X(tensor_max_index)(const tensor *sz);
INT X(tensor_min_istride)(const tensor *sz);
INT X(tensor_min_ostride)(const tensor *sz);
INT X(tensor_min_stride)(const tensor *sz);
int X(tensor_inplace_strides)(const tensor *sz);
int X(tensor_inplace_strides2)(const tensor *a, const tensor *b);
int X(tensor_strides_decrease)(const tensor *sz, const tensor *vecsz,
                               inplace_kind k);
tensor *X(tensor_copy)(const tensor *sz);
int X(tensor_kosherp)(const tensor *x);

tensor *X(tensor_copy_inplace)(const tensor *sz, inplace_kind k);
tensor *X(tensor_copy_except)(const tensor *sz, int except_dim);
tensor *X(tensor_copy_sub)(const tensor *sz, int start_dim, int rnk);
tensor *X(tensor_compress)(const tensor *sz);
tensor *X(tensor_compress_contiguous)(const tensor *sz);
tensor *X(tensor_append)(const tensor *a, const tensor *b);
void X(tensor_split)(const tensor *sz, tensor **a, int a_rnk, tensor **b);
int X(tensor_tornk1)(const tensor *t, INT *n, INT *is, INT *os);
void X(tensor_destroy)(tensor *sz);
void X(tensor_destroy2)(tensor *a, tensor *b);
void X(tensor_destroy4)(tensor *a, tensor *b, tensor *c, tensor *d);
void X(tensor_print)(const tensor *sz, printer *p);
int X(dimcmp)(const iodim *a, const iodim *b);
int X(tensor_equal)(const tensor *a, const tensor *b);
int X(tensor_inplace_locations)(const tensor *sz, const tensor *vecsz);

/*-----------------------------------------------------------------------*/
/* problem.c: */
enum { 
     /* a problem that cannot be solved */
     PROBLEM_UNSOLVABLE,

     PROBLEM_DFT, 
     PROBLEM_RDFT,
     PROBLEM_RDFT2,

     /* for mpi/ subdirectory */
     PROBLEM_MPI_DFT,
     PROBLEM_MPI_RDFT,
     PROBLEM_MPI_RDFT2,
     PROBLEM_MPI_TRANSPOSE,

     PROBLEM_LAST 
};

typedef struct {
     int problem_kind;
     void (*hash) (const problem *ego, md5 *p);
     void (*zero) (const problem *ego);
     void (*print) (const problem *ego, printer *p);
     void (*destroy) (problem *ego);
} problem_adt;

struct problem_s {
     const problem_adt *adt;
};

problem *X(mkproblem)(size_t sz, const problem_adt *adt);
void X(problem_destroy)(problem *ego);
problem *X(mkproblem_unsolvable)(void);

/*-----------------------------------------------------------------------*/
/* print.c */
struct printer_s {
     void (*print)(printer *p, const char *format, ...);
     void (*vprint)(printer *p, const char *format, va_list ap);
     void (*putchr)(printer *p, char c);
     void (*cleanup)(printer *p);
     int indent;
     int indent_incr;
};

printer *X(mkprinter)(size_t size, 
		      void (*putchr)(printer *p, char c),
		      void (*cleanup)(printer *p));
IFFTW_EXTERN void X(printer_destroy)(printer *p);

/*-----------------------------------------------------------------------*/
/* scan.c */
struct scanner_s {
     int (*scan)(scanner *sc, const char *format, ...);
     int (*vscan)(scanner *sc, const char *format, va_list ap);
     int (*getchr)(scanner *sc);
     int ungotc;
};

scanner *X(mkscanner)(size_t size, int (*getchr)(scanner *sc));
void X(scanner_destroy)(scanner *sc);

/*-----------------------------------------------------------------------*/
/* plan.c: */

enum wakefulness {
     SLEEPY,
     AWAKE_ZERO,
     AWAKE_SQRTN_TABLE,
     AWAKE_SINCOS
};

typedef struct {
     void (*solve)(const plan *ego, const problem *p);
     void (*awake)(plan *ego, enum wakefulness wakefulness);
     void (*print)(const plan *ego, printer *p);
     void (*destroy)(plan *ego);
} plan_adt;

struct plan_s {
     const plan_adt *adt;
     opcnt ops;
     double pcost;
     enum wakefulness wakefulness; /* used for debugging only */
     int could_prune_now_p;
};

plan *X(mkplan)(size_t size, const plan_adt *adt);
void X(plan_destroy_internal)(plan *ego);
IFFTW_EXTERN void X(plan_awake)(plan *ego, enum wakefulness wakefulness);
void X(plan_null_destroy)(plan *ego);

/*-----------------------------------------------------------------------*/
/* solver.c: */
typedef struct {
     int problem_kind;
     plan *(*mkplan)(const solver *ego, const problem *p, planner *plnr);
     void (*destroy)(solver *ego);
} solver_adt;

struct solver_s {
     const solver_adt *adt;
     int refcnt;
};

solver *X(mksolver)(size_t size, const solver_adt *adt);
void X(solver_use)(solver *ego);
void X(solver_destroy)(solver *ego);
void X(solver_register)(planner *plnr, solver *s);

/* shorthand */
#define MKSOLVER(type, adt) (type *)X(mksolver)(sizeof(type), adt)

/*-----------------------------------------------------------------------*/
/* planner.c */

typedef struct slvdesc_s {
     solver *slv;
     const char *reg_nam;
     unsigned nam_hash;
     int reg_id;
     int next_for_same_problem_kind;
} slvdesc;

typedef struct solution_s solution; /* opaque */

/* interpretation of L and U: 

   - if it returns a plan, the planner guarantees that all applicable
     plans at least as impatient as U have been tried, and that each
     plan in the solution is at least as impatient as L.
   
   - if it returns 0, the planner guarantees to have tried all solvers
     at least as impatient as L, and that none of them was applicable.

   The structure is packed to fit into 64 bits.
*/

typedef struct {
     unsigned l:20;
     unsigned hash_info:3;
#    define BITS_FOR_TIMELIMIT 9
     unsigned timelimit_impatience:BITS_FOR_TIMELIMIT;
     unsigned u:20;
     
     /* abstraction break: we store the solver here to pad the
	structure to 64 bits.  Otherwise, the struct is padded to 64
	bits anyway, and another word is allocated for slvndx. */
#    define BITS_FOR_SLVNDX 12
     unsigned slvndx:BITS_FOR_SLVNDX;
} flags_t;

/* impatience flags  */
enum {
     BELIEVE_PCOST = 0x0001,
     ESTIMATE = 0x0002,
     NO_DFT_R2HC = 0x0004,
     NO_SLOW = 0x0008,
     NO_VRECURSE = 0x0010,
     NO_INDIRECT_OP = 0x0020,
     NO_LARGE_GENERIC = 0x0040,
     NO_RANK_SPLITS = 0x0080,
     NO_VRANK_SPLITS = 0x0100,
     NO_NONTHREADED = 0x0200,
     NO_BUFFERING = 0x0400,
     NO_FIXED_RADIX_LARGE_N = 0x0800,
     NO_DESTROY_INPUT = 0x1000,
     NO_SIMD = 0x2000,
     CONSERVE_MEMORY = 0x4000,
     NO_DHT_R2HC = 0x8000,
     NO_UGLY = 0x10000,
     ALLOW_PRUNING = 0x20000
};

/* hashtable information */
enum {
     BLESSING = 0x1u,   /* save this entry */
     H_VALID = 0x2u,    /* valid hastable entry */
     H_LIVE = 0x4u      /* entry is nonempty, implies H_VALID */
};

#define PLNR_L(plnr) ((plnr)->flags.l)
#define PLNR_U(plnr) ((plnr)->flags.u)
#define PLNR_TIMELIMIT_IMPATIENCE(plnr) ((plnr)->flags.timelimit_impatience)

#define ESTIMATEP(plnr) (PLNR_U(plnr) & ESTIMATE)
#define BELIEVE_PCOSTP(plnr) (PLNR_U(plnr) & BELIEVE_PCOST)
#define ALLOW_PRUNINGP(plnr) (PLNR_U(plnr) & ALLOW_PRUNING)

#define NO_INDIRECT_OP_P(plnr) (PLNR_L(plnr) & NO_INDIRECT_OP)
#define NO_LARGE_GENERICP(plnr) (PLNR_L(plnr) & NO_LARGE_GENERIC)
#define NO_RANK_SPLITSP(plnr) (PLNR_L(plnr) & NO_RANK_SPLITS)
#define NO_VRANK_SPLITSP(plnr) (PLNR_L(plnr) & NO_VRANK_SPLITS)
#define NO_VRECURSEP(plnr) (PLNR_L(plnr) & NO_VRECURSE)
#define NO_DFT_R2HCP(plnr) (PLNR_L(plnr) & NO_DFT_R2HC)
#define NO_SLOWP(plnr) (PLNR_L(plnr) & NO_SLOW)
#define NO_UGLYP(plnr) (PLNR_L(plnr) & NO_UGLY)
#define NO_FIXED_RADIX_LARGE_NP(plnr) \
  (PLNR_L(plnr) & NO_FIXED_RADIX_LARGE_N)
#define NO_NONTHREADEDP(plnr) \
  ((PLNR_L(plnr) & NO_NONTHREADED) && (plnr)->nthr > 1)

#define NO_DESTROY_INPUTP(plnr) (PLNR_L(plnr) & NO_DESTROY_INPUT)
#define NO_SIMDP(plnr) (PLNR_L(plnr) & NO_SIMD)
#define CONSERVE_MEMORYP(plnr) (PLNR_L(plnr) & CONSERVE_MEMORY)
#define NO_DHT_R2HCP(plnr) (PLNR_L(plnr) & NO_DHT_R2HC)
#define NO_BUFFERINGP(plnr) (PLNR_L(plnr) & NO_BUFFERING)

typedef enum { FORGET_ACCURSED, FORGET_EVERYTHING } amnesia;

typedef enum { 
     /* WISDOM_NORMAL: planner may or may not use wisdom */
     WISDOM_NORMAL, 

     /* WISDOM_ONLY: planner must use wisdom and must avoid searching */
     WISDOM_ONLY, 

     /* WISDOM_IS_BOGUS: planner must return 0 as quickly as possible */
     WISDOM_IS_BOGUS,

     /* WISDOM_IGNORE_INFEASIBLE: planner ignores infeasible wisdom */
     WISDOM_IGNORE_INFEASIBLE,

     /* WISDOM_IGNORE_ALL: planner ignores all */
     WISDOM_IGNORE_ALL
} wisdom_state_t;

typedef struct {
     void (*register_solver)(planner *ego, solver *s);
     plan *(*mkplan)(planner *ego, const problem *p);
     void (*forget)(planner *ego, amnesia a);
     void (*exprt)(planner *ego, printer *p); /* ``export'' is a reserved
						 word in C++. */
     int (*imprt)(planner *ego, scanner *sc);
} planner_adt;

/* hash table of solutions */
typedef struct {
     solution *solutions;
     unsigned hashsiz, nelem;

     /* statistics */
     int lookup, succ_lookup, lookup_iter;
     int insert, insert_iter, insert_unknown;
     int nrehash;
} hashtab;

typedef enum { COST_SUM, COST_MAX } cost_kind;

struct planner_s {
     const planner_adt *adt;
     void (*hook)(struct planner_s *plnr, plan *pln, 
		  const problem *p, int optimalp);
     double (*cost_hook)(const problem *p, double t, cost_kind k);
     int (*wisdom_ok_hook)(const problem *p, flags_t flags);
     void (*nowisdom_hook)(const problem *p);
     wisdom_state_t (*bogosity_hook)(wisdom_state_t state, const problem *p);

     /* solver descriptors */
     slvdesc *slvdescs;
     unsigned nslvdesc, slvdescsiz;
     const char *cur_reg_nam;
     int cur_reg_id;
     int slvdescs_for_problem_kind[PROBLEM_LAST];

     wisdom_state_t wisdom_state;

     hashtab htab_blessed;
     hashtab htab_unblessed;

     int nthr;
     flags_t flags;

     crude_time start_time;
     double timelimit; /* elapsed_since(start_time) at which to bail out */
     int timed_out; /* whether most recent search timed out */
     int need_timeout_check;

     /* various statistics */
     int nplan;    /* number of plans evaluated */
     double pcost, epcost; /* total pcost of measured/estimated plans */
     int nprob;    /* number of problems evaluated */
};

planner *X(mkplanner)(void);
void X(planner_destroy)(planner *ego);

/*
  Iterate over all solvers.   Read:
 
  @article{ baker93iterators,
  author = "Henry G. Baker, Jr.",
  title = "Iterators: Signs of Weakness in Object-Oriented Languages",
  journal = "{ACM} {OOPS} Messenger",
  volume = "4",
  number = "3",
  pages = "18--25"
  }
*/
#define FORALL_SOLVERS(ego, s, p, what)			\
{							\
     unsigned _cnt;					\
     for (_cnt = 0; _cnt < ego->nslvdesc; ++_cnt) {	\
	  slvdesc *p = ego->slvdescs + _cnt;		\
	  solver *s = p->slv;				\
	  what;						\
     }							\
}

#define FORALL_SOLVERS_OF_KIND(kind, ego, s, p, what)		\
{								\
     int _cnt = ego->slvdescs_for_problem_kind[kind]; 		\
     while (_cnt >= 0) {					\
	  slvdesc *p = ego->slvdescs + _cnt;			\
	  solver *s = p->slv;					\
	  what;							\
	  _cnt = p->next_for_same_problem_kind;			\
     }								\
}


/* make plan, destroy problem */
plan *X(mkplan_d)(planner *ego, problem *p);
plan *X(mkplan_f_d)(planner *ego, problem *p, 
		    unsigned l_set, unsigned u_set, unsigned u_reset);

/*-----------------------------------------------------------------------*/
/* stride.c: */

/* If PRECOMPUTE_ARRAY_INDICES is defined, precompute all strides. */
#if (defined(__i386__) || defined(__x86_64__) || _M_IX86 >= 500) && !defined(FFTW_LDOUBLE)
#define PRECOMPUTE_ARRAY_INDICES
#endif

extern const INT X(an_INT_guaranteed_to_be_zero);

#ifdef PRECOMPUTE_ARRAY_INDICES
typedef INT *stride;
#define WS(stride, i)  (stride[i])
extern stride X(mkstride)(INT n, INT s);
void X(stride_destroy)(stride p);
/* hackery to prevent the compiler from copying the strides array
   onto the stack */
#define MAKE_VOLATILE_STRIDE(nptr, x) (x) = (x) + X(an_INT_guaranteed_to_be_zero)
#else

typedef INT stride;
#define WS(stride, i)  (stride * i)
#define fftwf_mkstride(n, stride) stride
#define fftw_mkstride(n, stride) stride
#define fftwl_mkstride(n, stride) stride
#define fftwf_stride_destroy(p) ((void) p)
#define fftw_stride_destroy(p) ((void) p)
#define fftwl_stride_destroy(p) ((void) p)

/* hackery to prevent the compiler from ``optimizing'' induction
   variables in codelet loops.  The problem is that for each K and for
   each expression of the form P[I + STRIDE * K] in a loop, most
   compilers will try to lift an induction variable PK := &P[I + STRIDE * K].
   For large values of K this behavior overflows the
   register set, which is likely worse than doing the index computation
   in the first place.

   If we guess that there are more than
   ESTIMATED_AVAILABLE_INDEX_REGISTERS such pointers, we deliberately confuse
   the compiler by setting STRIDE ^= ZERO, where ZERO is a value guaranteed to
   be 0, but the compiler does not know this. 

   16 registers ought to be enough for anybody, or so the amd64 and ARM ISA's
   seem to imply.
*/
#define ESTIMATED_AVAILABLE_INDEX_REGISTERS 16
#define MAKE_VOLATILE_STRIDE(nptr, x)                   \
     (nptr <= ESTIMATED_AVAILABLE_INDEX_REGISTERS ?     \
        0 :                                             \
      ((x) = (x) ^ X(an_INT_guaranteed_to_be_zero)))
#endif /* PRECOMPUTE_ARRAY_INDICES */

/*-----------------------------------------------------------------------*/
/* solvtab.c */

struct solvtab_s { void (*reg)(planner *); const char *reg_nam; };
typedef struct solvtab_s solvtab[];
void X(solvtab_exec)(const solvtab tbl, planner *p);
#define SOLVTAB(s) { s, STRINGIZE(s) }
#define SOLVTAB_END { 0, 0 }

/*-----------------------------------------------------------------------*/
/* pickdim.c */
int X(pickdim)(int which_dim, const int *buddies, size_t nbuddies,
	       const tensor *sz, int oop, int *dp);

/*-----------------------------------------------------------------------*/
/* twiddle.c */
/* little language to express twiddle factors computation */
enum { TW_COS = 0, TW_SIN = 1, TW_CEXP = 2, TW_NEXT = 3, 
       TW_FULL = 4, TW_HALF = 5 };

typedef struct {
     unsigned char op;
     signed char v;
     short i;
} tw_instr;

typedef struct twid_s {
     R *W;                     /* array of twiddle factors */
     INT n, r, m;                /* transform order, radix, # twiddle rows */
     int refcnt;
     const tw_instr *instr;
     struct twid_s *cdr;
     enum wakefulness wakefulness;
} twid;

INT X(twiddle_length)(INT r, const tw_instr *p);
void X(twiddle_awake)(enum wakefulness wakefulness,
		      twid **pp, const tw_instr *instr, INT n, INT r, INT m);

/*-----------------------------------------------------------------------*/
/* trig.c */
#if defined(TRIGREAL_IS_LONG_DOUBLE)
   typedef long double trigreal;
#elif defined(TRIGREAL_IS_QUAD)
   typedef __float128 trigreal;
#else
   typedef double trigreal;
#endif

typedef struct triggen_s triggen;

struct triggen_s {
     void (*cexp)(triggen *t, INT m, R *result);
     void (*cexpl)(triggen *t, INT m, trigreal *result);
     void (*rotate)(triggen *p, INT m, R xr, R xi, R *res);

     INT twshft;
     INT twradix;
     INT twmsk;
     trigreal *W0, *W1;
     INT n;
};

triggen *X(mktriggen)(enum wakefulness wakefulness, INT n);
void X(triggen_destroy)(triggen *p);

/*-----------------------------------------------------------------------*/
/* primes.c: */

#define MULMOD(x, y, p) \
   (((x) <= 92681 - (y)) ? ((x) * (y)) % (p) : X(safe_mulmod)(x, y, p))

INT X(safe_mulmod)(INT x, INT y, INT p);
INT X(power_mod)(INT n, INT m, INT p);
INT X(find_generator)(INT p);
INT X(first_divisor)(INT n);
int X(is_prime)(INT n);
INT X(next_prime)(INT n);
int X(factors_into)(INT n, const INT *primes);
int X(factors_into_small_primes)(INT n);
INT X(choose_radix)(INT r, INT n);
INT X(isqrt)(INT n);
INT X(modulo)(INT a, INT n);

#define GENERIC_MIN_BAD 173 /* min prime for which generic becomes bad */

/* thresholds below which certain solvers are considered SLOW.  These are guesses
   believed to be conservative */
#define GENERIC_MAX_SLOW     16
#define RADER_MAX_SLOW       32
#define BLUESTEIN_MAX_SLOW   24

/*-----------------------------------------------------------------------*/
/* rader.c: */
typedef struct rader_tls rader_tl;

void X(rader_tl_insert)(INT k1, INT k2, INT k3, R *W, rader_tl **tl);
R *X(rader_tl_find)(INT k1, INT k2, INT k3, rader_tl *t);
void X(rader_tl_delete)(R *W, rader_tl **tl);

/*-----------------------------------------------------------------------*/
/* copy/transposition routines */

/* lower bound to the cache size, for tiled routines */
#define CACHESIZE 8192

INT X(compute_tilesz)(INT vl, int how_many_tiles_in_cache);

void X(tile2d)(INT n0l, INT n0u, INT n1l, INT n1u, INT tilesz,
	       void (*f)(INT n0l, INT n0u, INT n1l, INT n1u, void *args),
	       void *args);
void X(cpy1d)(R *I, R *O, INT n0, INT is0, INT os0, INT vl);
void X(zero1d_pair)(R *O0, R *O1, INT n0, INT os0);
void X(cpy2d)(R *I, R *O,
	      INT n0, INT is0, INT os0,
	      INT n1, INT is1, INT os1,
	      INT vl);
void X(cpy2d_ci)(R *I, R *O,
		 INT n0, INT is0, INT os0,
		 INT n1, INT is1, INT os1,
		 INT vl);
void X(cpy2d_co)(R *I, R *O,
		 INT n0, INT is0, INT os0,
		 INT n1, INT is1, INT os1,
		 INT vl);
void X(cpy2d_tiled)(R *I, R *O,
		    INT n0, INT is0, INT os0,
		    INT n1, INT is1, INT os1, 
		    INT vl);
void X(cpy2d_tiledbuf)(R *I, R *O,
		       INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1, 
		       INT vl);
void X(cpy2d_pair)(R *I0, R *I1, R *O0, R *O1,
		   INT n0, INT is0, INT os0,
		   INT n1, INT is1, INT os1);
void X(cpy2d_pair_ci)(R *I0, R *I1, R *O0, R *O1,
		      INT n0, INT is0, INT os0,
		      INT n1, INT is1, INT os1);
void X(cpy2d_pair_co)(R *I0, R *I1, R *O0, R *O1,
		      INT n0, INT is0, INT os0,
		      INT n1, INT is1, INT os1);

void X(transpose)(R *I, INT n, INT s0, INT s1, INT vl);
void X(transpose_tiled)(R *I, INT n, INT s0, INT s1, INT vl);
void X(transpose_tiledbuf)(R *I, INT n, INT s0, INT s1, INT vl);

typedef void (*transpose_func)(R *I, INT n, INT s0, INT s1, INT vl);
typedef void (*cpy2d_func)(R *I, R *O,
			   INT n0, INT is0, INT os0,
			   INT n1, INT is1, INT os1,
			   INT vl);

/*-----------------------------------------------------------------------*/
/* misc stuff */
void X(null_awake)(plan *ego, enum wakefulness wakefulness);
double X(iestimate_cost)(const planner *, const plan *, const problem *);

#ifdef FFTW_RANDOM_ESTIMATOR
extern unsigned X(random_estimate_seed);
#endif

double X(measure_execution_time)(const planner *plnr, 
				 plan *pln, const problem *p);
IFFTW_EXTERN int X(ialignment_of)(R *p);
unsigned X(hash)(const char *s);
INT X(nbuf)(INT n, INT vl, INT maxnbuf);
int X(nbuf_redundant)(INT n, INT vl, size_t which, 
		      const INT *maxnbuf, size_t nmaxnbuf);
INT X(bufdist)(INT n, INT vl);
int X(toobig)(INT n);
int X(ct_uglyp)(INT min_n, INT v, INT n, INT r);

#if HAVE_SIMD
R *X(taint)(R *p, INT s);
R *X(join_taint)(R *p1, R *p2);
#define TAINT(p, s) X(taint)(p, s)
#define UNTAINT(p) ((R *) (((uintptr_t) (p)) & ~(uintptr_t)3))
#define TAINTOF(p) (((uintptr_t)(p)) & 3)
#define JOIN_TAINT(p1, p2) X(join_taint)(p1, p2)
#else
#define TAINT(p, s) (p)
#define UNTAINT(p) (p)
#define TAINTOF(p) 0
#define JOIN_TAINT(p1, p2) p1
#endif

#define ASSERT_ALIGNED_DOUBLE  /*unused, legacy*/

/*-----------------------------------------------------------------------*/
/* macros used in codelets to reduce source code size */

typedef R E;  /* internal precision of codelets. */

#if defined(FFTW_LDOUBLE)
#  define K(x) ((E) x##L)
#elif defined(FFTW_QUAD)
#  define K(x) ((E) x##Q)
#else
#  define K(x) ((E) x)
#endif
#define DK(name, value) const E name = K(value)

/* FMA macros */

#if defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__) || defined(_POWER))
/* The obvious expression a * b + c does not work.  If both x = a * b
   + c and y = a * b - c appear in the source, gcc computes t = a * b,
   x = t + c, y = t - c, thus destroying the fma.

   This peculiar coding seems to do the right thing on all of
   gcc-2.95, gcc-3.1, gcc-3.2, and gcc-3.3.  It does the right thing
   on gcc-3.4 -fno-web (because the ``web'' pass splits the variable
   `x' for the single-assignment form).

   However, gcc-4.0 is a formidable adversary which succeeds in
   pessimizing two fma's into one multiplication and two additions.
   It does it very early in the game---before the optimization passes
   even start.  The only real workaround seems to use fake inline asm
   such as

     asm ("# confuse gcc %0" : "=f"(a) : "0"(a));
     return a * b + c;
     
   in each of the FMA, FMS, FNMA, and FNMS functions.  However, this
   does not solve the problem either, because two equal asm statements
   count as a common subexpression!  One must use *different* fake asm
   statements:

   in FMA:
     asm ("# confuse gcc for fma %0" : "=f"(a) : "0"(a));

   in FMS:
     asm ("# confuse gcc for fms %0" : "=f"(a) : "0"(a));

   etc.

   After these changes, gcc recalcitrantly generates the fma that was
   in the source to begin with.  However, the extra asm() cruft
   confuses other passes of gcc, notably the instruction scheduler.
   (Of course, one could also generate the fma directly via inline
   asm, but this confuses the scheduler even more.)

   Steven and I have submitted more than one bug report to the gcc
   mailing list over the past few years, to no effect.  Thus, I give
   up.  gcc-4.0 can go to hell.  I'll wait at least until gcc-4.3 is
   out before touching this crap again.
*/
static __inline__ E FMA(E a, E b, E c)
{
     E x = a * b;
     x = x + c;
     return x;
}

static __inline__ E FMS(E a, E b, E c)
{
     E x = a * b;
     x = x - c;
     return x;
}

static __inline__ E FNMA(E a, E b, E c)
{
     E x = a * b;
     x = - (x + c);
     return x;
}

static __inline__ E FNMS(E a, E b, E c)
{
     E x = a * b;
     x = - (x - c);
     return x;
}
#else
#define FMA(a, b, c) (((a) * (b)) + (c))
#define FMS(a, b, c) (((a) * (b)) - (c))
#define FNMA(a, b, c) (- (((a) * (b)) + (c)))
#define FNMS(a, b, c) ((c) - ((a) * (b)))
#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif /* __cplusplus */

#endif /* __IFFTW_H__ */
