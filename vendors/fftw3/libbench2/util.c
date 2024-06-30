/*
 * Copyright (c) 2000 Matteo Frigo
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#if defined(HAVE_MALLOC_H)
#  include <malloc.h>
#endif

#if defined(HAVE_DECL_MEMALIGN) && !HAVE_DECL_MEMALIGN
extern void *memalign(size_t, size_t);
#endif

#if defined(HAVE_DECL_POSIX_MEMALIGN) && !HAVE_DECL_POSIX_MEMALIGN
extern int posix_memalign(void **, size_t, size_t);
#endif

void bench_assertion_failed(const char *s, int line, const char *file)
{
     ovtpvt_err("bench: %s:%d: assertion failed: %s\n", file, line, s);
     bench_exit(EXIT_FAILURE);
}

#ifdef HAVE_DRAND48
#  if defined(HAVE_DECL_DRAND48) && !HAVE_DECL_DRAND48
extern double drand48(void);
#  endif
double bench_drand(void)
{
     return drand48() - 0.5;
}
#  if defined(HAVE_DECL_SRAND48) && !HAVE_DECL_SRAND48
extern void srand48(long);
#  endif
void bench_srand(int seed)
{
     srand48(seed);
}
#else
double bench_drand(void)
{
     double d = rand();
     return (d / (double) RAND_MAX) - 0.5;
}
void bench_srand(int seed)
{
     srand(seed);
}
#endif

/**********************************************************
 *   DEBUGGING CODE
 **********************************************************/
#ifdef BENCH_DEBUG
static int bench_malloc_cnt = 0;

/*
 * debugging malloc/free.  Initialize every malloced and freed area to
 * random values, just to make sure we are not using uninitialized
 * pointers.  Also check for writes past the ends of allocated blocks,
 * and a couple of other things.
 *
 * This code is a quick and dirty hack -- use at your own risk.
 */

static int bench_malloc_total = 0, bench_malloc_max = 0, bench_malloc_cnt_max = 0;

#define MAGIC ((size_t)0xABadCafe)
#define PAD_FACTOR 2
#define TWO_SIZE_T (2 * sizeof(size_t))

#define VERBOSE_ALLOCATION 0

#if VERBOSE_ALLOCATION
#define WHEN_VERBOSE(a) a
#else
#define WHEN_VERBOSE(a)
#endif

void *bench_malloc(size_t n)
{
     char *p;
     size_t i;

     bench_malloc_total += n;

     if (bench_malloc_total > bench_malloc_max)
	  bench_malloc_max = bench_malloc_total;

     p = (char *) malloc(PAD_FACTOR * n + TWO_SIZE_T);
     BENCH_ASSERT(p);

     /* store the size in a known position */
     ((size_t *) p)[0] = n;
     ((size_t *) p)[1] = MAGIC;
     for (i = 0; i < PAD_FACTOR * n; i++)
	  p[i + TWO_SIZE_T] = (char) (i ^ 0xDEADBEEF);

     ++bench_malloc_cnt;

     if (bench_malloc_cnt > bench_malloc_cnt_max)
	  bench_malloc_cnt_max = bench_malloc_cnt;

     /* skip the size we stored previously */
     return (void *) (p + TWO_SIZE_T);
}

void bench_free(void *p)
{
     char *q;

     BENCH_ASSERT(p);

     q = ((char *) p) - TWO_SIZE_T;
     BENCH_ASSERT(q);

     {
	  size_t n = ((size_t *) q)[0];
	  size_t magic = ((size_t *) q)[1];
	  size_t i;

	  ((size_t *) q)[0] = 0; /* set to zero to detect duplicate free's */

	  BENCH_ASSERT(magic == MAGIC);
	  ((size_t *) q)[1] = ~MAGIC;

	  bench_malloc_total -= n;
	  BENCH_ASSERT(bench_malloc_total >= 0);

	  /* check for writing past end of array: */
	  for (i = n; i < PAD_FACTOR * n; ++i)
	       if (q[i + TWO_SIZE_T] != (char) (i ^ 0xDEADBEEF)) {
		    BENCH_ASSERT(0 /* array bounds overwritten */);
	       }
	  for (i = 0; i < PAD_FACTOR * n; ++i)
	       q[i + TWO_SIZE_T] = (char) (i ^ 0xBEEFDEAD);

	  --bench_malloc_cnt;

	  BENCH_ASSERT(bench_malloc_cnt >= 0);

	  BENCH_ASSERT(
	       (bench_malloc_cnt == 0 && bench_malloc_total == 0) ||
	       (bench_malloc_cnt > 0 && bench_malloc_total > 0));

	  free(q);
     }
}

#else
/**********************************************************
 *   NON DEBUGGING CODE
 **********************************************************/
/* production version, no hacks */

#define MIN_ALIGNMENT 128    /* must be power of two */

#define real_free free /* memalign and malloc use ordinary free */

void *bench_malloc(size_t n)
{
     void *p;
     if (n == 0) n = 1;

#if defined(WITH_OUR_MALLOC)
     /* Our own aligned malloc/free.  Assumes sizeof(void*) is
	a power of two <= 8 and that malloc is at least
	sizeof(void*)-aligned.  Assumes size_t = uintptr_t.  */
     {
	  void *p0;
	  if ((p0 = malloc(n + MIN_ALIGNMENT))) {
	       p = (void *) (((size_t) p0 + MIN_ALIGNMENT) & (~((size_t) (MIN_ALIGNMENT - 1))));
	       *((void **) p - 1) = p0;
	  }
	  else
	       p = (void *) 0;
     }
#elif defined(HAVE_MEMALIGN)
     p = memalign(MIN_ALIGNMENT, n);
#elif defined(HAVE_POSIX_MEMALIGN)
     /* note: posix_memalign is broken in glibc 2.2.5: it constrains
	the size, not the alignment, to be (power of two) * sizeof(void*).
        The bug seems to have been fixed as of glibc 2.3.1. */
     if (posix_memalign(&p, MIN_ALIGNMENT, n))
	  p = (void*) 0;
#elif defined(__ICC) || defined(__INTEL_COMPILER) || defined(HAVE__MM_MALLOC)
     /* Intel's C compiler defines _mm_malloc and _mm_free intrinsics */
     p = (void *) _mm_malloc(n, MIN_ALIGNMENT);
#    undef real_free
#    define real_free _mm_free
#else
     p = malloc(n);
#endif

     BENCH_ASSERT(p);
     return p;
}

void bench_free(void *p)
{
#ifdef WITH_OUR_MALLOC
     if (p) free(*((void **) p - 1));
#else
     real_free(p);
#endif
}

#endif

void bench_free0(void *p)
{
     if (p) bench_free(p);
}
