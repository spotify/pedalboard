/* not worth copyrighting */

#include "libbench2/bench.h"

void caset(bench_complex *A, int n, bench_complex x)
{
     int i;
     for (i = 0; i < n; ++i) {
	  c_re(A[i]) = c_re(x);
	  c_im(A[i]) = c_im(x);
     }
}
