/* declarations of common subroutines, etc. for use with FFTW
   self-test/benchmark program (see bench.c). */

#include "libbench2/bench-user.h"
#include "api/fftw3.h"

#define CONCAT(prefix, name) prefix ## name
#if defined(BENCHFFT_SINGLE)
#define FFTW(x) CONCAT(fftwf_, x)
#elif defined(BENCHFFT_LDOUBLE)
#define FFTW(x) CONCAT(fftwl_, x)
#elif defined(BENCHFFT_QUAD)
#define FFTW(x) CONCAT(fftwq_, x)
#else
#define FFTW(x) CONCAT(fftw_, x)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

extern FFTW(plan) mkplan(bench_problem *p, unsigned flags);
extern void initial_cleanup(void);
extern void final_cleanup(void);
extern int import_wisdom(FILE *f);
extern void export_wisdom(FILE *f);

#if defined(HAVE_THREADS) || defined(HAVE_OPENMP)
#  define HAVE_SMP
   extern int threads_ok;
#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif /* __cplusplus */

