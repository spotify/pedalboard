/* not worth copyrighting */
#include "libbench2/bench.h"

/* default routine, can be overridden by user */
void after_problem_hccopy_from(bench_problem *p, bench_real *ri, bench_real *ii)
{
     UNUSED(p);
     UNUSED(ri);
     UNUSED(ii);
}
