/* not worth copyrighting */
#include "libbench2/bench.h"

/* default routine, can be overridden by user */
void after_problem_rcopy_to(bench_problem *p, bench_real *ro)
{
     UNUSED(p);
     UNUSED(ro);
}
