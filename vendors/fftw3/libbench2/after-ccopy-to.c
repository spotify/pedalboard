/* not worth copyrighting */
#include "libbench2/bench.h"

/* default routine, can be overridden by user */
void after_problem_ccopy_to(bench_problem *p, bench_real *ro, bench_real *io)
{
     UNUSED(p);
     UNUSED(ro);
     UNUSED(io);
}
