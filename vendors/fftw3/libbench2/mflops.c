/* not worth copyrighting */

#include "libbench2/bench.h"
#include <math.h>

double mflops(const bench_problem *p, double t)
{
     size_t size = tensor_sz(p->sz);
     size_t vsize = tensor_sz(p->vecsz);

     if (size <= 1) /* a copy: just return reals copied / time */
	  switch (p->kind) {
	      case PROBLEM_COMPLEX:
		   return (2.0 * size * vsize / (t * 1.0e6));
	      case PROBLEM_REAL:
	      case PROBLEM_R2R:
		   return (1.0 * size * vsize / (t * 1.0e6));
	  }

     switch (p->kind) {
	 case PROBLEM_COMPLEX:
	      return (5.0 * size * vsize * log((double)size) / 
		      (log(2.0) * t * 1.0e6));
	 case PROBLEM_REAL:
	 case PROBLEM_R2R:
	      return (2.5 * vsize * size * log((double) size) / 
		      (log(2.0) * t * 1.0e6));
     }
     BENCH_ASSERT(0 /* can't happen */);
     return 0.0;
}

