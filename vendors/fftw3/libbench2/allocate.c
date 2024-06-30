/* not worth copyrighting */


#include "libbench2/bench.h"

static void bounds(bench_problem *p, int *ilb, int *iub, int *olb, int *oub)
{
     bench_tensor *t = tensor_append(p->sz, p->vecsz);
     tensor_ibounds(t, ilb, iub);
     tensor_obounds(t, olb, oub);
     tensor_destroy(t);
}

/*
 * Allocate I/O arrays for a problem.
 *
 * This is the default routine that can be overridden by the user in
 * complicated cases.
 */
void problem_alloc(bench_problem *p)
{
     int ilb, iub, olb, oub;
     int isz, osz;

     bounds(p, &ilb, &iub, &olb, &oub);
     isz = iub - ilb;
     osz = oub - olb;

     if (p->kind == PROBLEM_COMPLEX) {
	  bench_complex *in, *out;

	  p->iphyssz = isz;
	  p->inphys = in = (bench_complex *) bench_malloc(isz * sizeof(bench_complex));
	  p->in = in - ilb;
	  
	  if (p->in_place) {
	       p->out = p->in;
	       p->outphys = p->inphys;
	       p->ophyssz = p->iphyssz;
	  } else {
	       p->ophyssz = osz;
	       p->outphys = out = (bench_complex *) bench_malloc(osz * sizeof(bench_complex));
	       p->out = out - olb;
	  }
     } else if (p->kind == PROBLEM_R2R) {
	  bench_real *in, *out;

	  p->iphyssz = isz;
	  p->inphys = in = (bench_real *) bench_malloc(isz * sizeof(bench_real));
	  p->in = in - ilb;
	  
	  if (p->in_place) {
	       p->out = p->in;
	       p->outphys = p->inphys;
	       p->ophyssz = p->iphyssz;
	  } else {
	       p->ophyssz = osz;
	       p->outphys = out = (bench_real *) bench_malloc(osz * sizeof(bench_real));
	       p->out = out - olb;
	  }
     } else if (p->kind == PROBLEM_REAL && p->sign < 0) { /* R2HC */
	  bench_real *in;
	  bench_complex *out;

	  isz = isz > osz*2 ? isz : osz*2;
	  p->iphyssz = isz;
	  p->inphys = in = (bench_real *) bench_malloc(p->iphyssz * sizeof(bench_real));
	  p->in = in - ilb;
	  
	  if (p->in_place) {
	       p->out = p->in;
	       p->outphys = p->inphys;
	       p->ophyssz = p->iphyssz / 2;
	  } else {
	       p->ophyssz = osz;
	       p->outphys = out = (bench_complex *) bench_malloc(osz * sizeof(bench_complex));
	       p->out = out - olb;
	  }
     } else if (p->kind == PROBLEM_REAL && p->sign > 0) { /* HC2R */
	  bench_real *out;
	  bench_complex *in;

	  osz = osz > isz*2 ? osz : isz*2;
	  p->ophyssz = osz;
	  p->outphys = out = (bench_real *) bench_malloc(p->ophyssz * sizeof(bench_real));
	  p->out = out - olb;
	  
	  if (p->in_place) {
	       p->in = p->out;
	       p->inphys = p->outphys;
	       p->iphyssz = p->ophyssz / 2;
	  } else {
	       p->iphyssz = isz;
	       p->inphys = in = (bench_complex *) bench_malloc(isz * sizeof(bench_complex));
	       p->in = in - ilb;
	  }
     } else {
	  BENCH_ASSERT(0); /* TODO */
     }
}

void problem_free(bench_problem *p)
{
     if (p->outphys && p->outphys != p->inphys)
	  bench_free(p->outphys);
     if (p->inphys)
	  bench_free(p->inphys);
     tensor_destroy(p->sz);
     tensor_destroy(p->vecsz);
}
