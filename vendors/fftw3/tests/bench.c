/**************************************************************************/
/* NOTE to users: this is the FFTW self-test and benchmark program.
   It is probably NOT a good place to learn FFTW usage, since it has a
   lot of added complexity in order to exercise and test the full API,
   etcetera.  We suggest reading the manual. 

   (Some of the self-test code is split off into fftw-bench.c and
   hook.c.) */
/**************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tests/fftw-bench.h"

static const char *mkversion(void) { return FFTW(version); }
static const char *mkcc(void) { return FFTW(cc); }
static const char *mkcodelet_optim(void) { return FFTW(codelet_optim); }

BEGIN_BENCH_DOC
BENCH_DOC("name", "fftw3")
BENCH_DOCF("version", mkversion)
BENCH_DOCF("cc", mkcc)
BENCH_DOCF("codelet-optim", mkcodelet_optim)
END_BENCH_DOC 

static FFTW(iodim) *bench_tensor_to_fftw_iodim(bench_tensor *t)
{
     FFTW(iodim) *d;
     int i;

     BENCH_ASSERT(t->rnk >= 0);
     if (t->rnk == 0) return 0;
     
     d = (FFTW(iodim) *)bench_malloc(sizeof(FFTW(iodim)) * t->rnk);
     for (i = 0; i < t->rnk; ++i) {
	  d[i].n = t->dims[i].n;
	  d[i].is = t->dims[i].is;
	  d[i].os = t->dims[i].os;
     }

     return d;
}

static void extract_reim_split(int sign, int size, bench_real *p,
			       bench_real **r, bench_real **i)
{
     if (sign == FFTW_FORWARD) {
          *r = p + 0;
          *i = p + size;
     } else {
          *r = p + size;
          *i = p + 0;
     }
}

static int sizeof_problem(bench_problem *p)
{
     return tensor_sz(p->sz) * tensor_sz(p->vecsz);
}

/* ouch */
static int expressible_as_api_many(bench_tensor *t)
{
     int i;

     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));

     i = t->rnk - 1;
     while (--i >= 0) {
	  bench_iodim *d = t->dims + i;
	  if (d[0].is % d[1].is) return 0;
	  if (d[0].os % d[1].os) return 0;
     }
     return 1;
}

static int *mkn(bench_tensor *t)
{
     int *n = (int *) bench_malloc(sizeof(int *) * t->rnk);
     int i;
     for (i = 0; i < t->rnk; ++i) 
	  n[i] = t->dims[i].n;
     return n;
}

static void mknembed_many(bench_tensor *t, int **inembedp, int **onembedp)
{
     int i;
     bench_iodim *d;
     int *inembed = (int *) bench_malloc(sizeof(int *) * t->rnk);
     int *onembed = (int *) bench_malloc(sizeof(int *) * t->rnk);

     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));
     *inembedp = inembed; *onembedp = onembed;

     i = t->rnk - 1;
     while (--i >= 0) {
	  d = t->dims + i;
	  inembed[i+1] = d[0].is / d[1].is;
	  onembed[i+1] = d[0].os / d[1].os;
     }
}

/* try to use the most appropriate API function.  Big mess. */

static int imax(int a, int b) { return (a > b ? a : b); }

static int halfish_sizeof_problem(bench_problem *p)
{
     int n2 = sizeof_problem(p);
     if (BENCH_FINITE_RNK(p->sz->rnk) && p->sz->rnk > 0)
          n2 = (n2 / imax(p->sz->dims[p->sz->rnk - 1].n, 1)) *
               (p->sz->dims[p->sz->rnk - 1].n / 2 + 1);
     return n2;
}

static FFTW(plan) mkplan_real_split(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln;
     bench_tensor *sz = p->sz, *vecsz = p->vecsz;
     FFTW(iodim) *dims, *howmany_dims;
     bench_real *ri, *ii, *ro, *io;
     int n2 = halfish_sizeof_problem(p);

     extract_reim_split(FFTW_FORWARD, n2, (bench_real *) p->in, &ri, &ii);
     extract_reim_split(FFTW_FORWARD, n2, (bench_real *) p->out, &ro, &io);

     dims = bench_tensor_to_fftw_iodim(sz);
     howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
     if (p->sign < 0) {
	  if (verbose > 2) printf("using plan_guru_split_dft_r2c\n");
	  pln = FFTW(plan_guru_split_dft_r2c)(sz->rnk, dims,
					vecsz->rnk, howmany_dims,
					ri, ro, io, flags);
     }
     else {
	  if (verbose > 2) printf("using plan_guru_split_dft_c2r\n");
	  pln = FFTW(plan_guru_split_dft_c2r)(sz->rnk, dims,
					vecsz->rnk, howmany_dims,
					ri, ii, ro, flags);
     }
     bench_free(dims);
     bench_free(howmany_dims);
     return pln;
}

static FFTW(plan) mkplan_real_interleaved(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln;
     bench_tensor *sz = p->sz, *vecsz = p->vecsz;

     if (vecsz->rnk == 0 && tensor_unitstridep(sz) 
	 && tensor_real_rowmajorp(sz, p->sign, p->in_place)) 
	  goto api_simple;
     
     if (vecsz->rnk == 1 && expressible_as_api_many(sz))
	  goto api_many;

     goto api_guru;

 api_simple:
     switch (sz->rnk) {
	 case 1:
	      if (p->sign < 0) {
		   if (verbose > 2) printf("using plan_dft_r2c_1d\n");
		   return FFTW(plan_dft_r2c_1d)(sz->dims[0].n, 
						(bench_real *) p->in, 
						(bench_complex *) p->out,
						flags);
	      }
	      else {
		   if (verbose > 2) printf("using plan_dft_c2r_1d\n");
		   return FFTW(plan_dft_c2r_1d)(sz->dims[0].n, 
						(bench_complex *) p->in, 
						(bench_real *) p->out,
						flags);
	      }
	      break;
	 case 2:
	      if (p->sign < 0) {
		   if (verbose > 2) printf("using plan_dft_r2c_2d\n");
		   return FFTW(plan_dft_r2c_2d)(sz->dims[0].n, sz->dims[1].n,
						(bench_real *) p->in, 
						(bench_complex *) p->out,
						flags);
	      }
	      else {
		   if (verbose > 2) printf("using plan_dft_c2r_2d\n");
		   return FFTW(plan_dft_c2r_2d)(sz->dims[0].n, sz->dims[1].n,
						(bench_complex *) p->in, 
						(bench_real *) p->out,
						flags);
	      }
	      break;
	 case 3:
	      if (p->sign < 0) {
		   if (verbose > 2) printf("using plan_dft_r2c_3d\n");
		   return FFTW(plan_dft_r2c_3d)(
			sz->dims[0].n, sz->dims[1].n, sz->dims[2].n,
			(bench_real *) p->in, (bench_complex *) p->out,
			flags);
	      }
	      else {
		   if (verbose > 2) printf("using plan_dft_c2r_3d\n");
		   return FFTW(plan_dft_c2r_3d)(
			sz->dims[0].n, sz->dims[1].n, sz->dims[2].n,
			(bench_complex *) p->in, (bench_real *) p->out,
			flags);
	      }
	      break;
	 default: {
	      int *n = mkn(sz);
	      if (p->sign < 0) {
		   if (verbose > 2) printf("using plan_dft_r2c\n");
		   pln = FFTW(plan_dft_r2c)(sz->rnk, n,
					    (bench_real *) p->in, 
					    (bench_complex *) p->out,
					    flags);
	      }
	      else {
		   if (verbose > 2) printf("using plan_dft_c2r\n");
		   pln = FFTW(plan_dft_c2r)(sz->rnk, n,
					    (bench_complex *) p->in, 
					    (bench_real *) p->out,
					    flags);
	      }
	      bench_free(n);
	      return pln;
	 }
     }

 api_many:
     {
	  int *n, *inembed, *onembed;
	  BENCH_ASSERT(vecsz->rnk == 1);
	  n = mkn(sz);
	  mknembed_many(sz, &inembed, &onembed);
	  if (p->sign < 0) {
	       if (verbose > 2) printf("using plan_many_dft_r2c\n");
	       pln = FFTW(plan_many_dft_r2c)(
		    sz->rnk, n, vecsz->dims[0].n, 
		    (bench_real *) p->in, inembed,
		    sz->dims[sz->rnk - 1].is, vecsz->dims[0].is,
		    (bench_complex *) p->out, onembed,
		    sz->dims[sz->rnk - 1].os, vecsz->dims[0].os,
		    flags);
	  }
	  else {
	       if (verbose > 2) printf("using plan_many_dft_c2r\n");
	       pln = FFTW(plan_many_dft_c2r)(
		    sz->rnk, n, vecsz->dims[0].n, 
		    (bench_complex *) p->in, inembed,
		    sz->dims[sz->rnk - 1].is, vecsz->dims[0].is,
		    (bench_real *) p->out, onembed,
		    sz->dims[sz->rnk - 1].os, vecsz->dims[0].os,
		    flags);
	  }
	  bench_free(n); bench_free(inembed); bench_free(onembed);
	  return pln;
     }

 api_guru:
     {
	  FFTW(iodim) *dims, *howmany_dims;

	  if (p->sign < 0) {
	       dims = bench_tensor_to_fftw_iodim(sz);
	       howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
	       if (verbose > 2) printf("using plan_guru_dft_r2c\n");
	       pln = FFTW(plan_guru_dft_r2c)(sz->rnk, dims,
					     vecsz->rnk, howmany_dims,
					     (bench_real *) p->in,
					     (bench_complex *) p->out,
					     flags);
	  }
	  else {
	       dims = bench_tensor_to_fftw_iodim(sz);
	       howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
	       if (verbose > 2) printf("using plan_guru_dft_c2r\n");
	       pln = FFTW(plan_guru_dft_c2r)(sz->rnk, dims,
					     vecsz->rnk, howmany_dims,
					     (bench_complex *) p->in,
					     (bench_real *) p->out,
					     flags);
	  }
	  bench_free(dims);
	  bench_free(howmany_dims);
	  return pln;
     }
}

static FFTW(plan) mkplan_real(bench_problem *p, unsigned flags)
{
     if (p->split)
	  return mkplan_real_split(p, flags);
     else
	  return mkplan_real_interleaved(p, flags);
}

static FFTW(plan) mkplan_complex_split(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln;
     bench_tensor *sz = p->sz, *vecsz = p->vecsz;
     FFTW(iodim) *dims, *howmany_dims;
     bench_real *ri, *ii, *ro, *io;

     extract_reim_split(p->sign, p->iphyssz, (bench_real *) p->in, &ri, &ii);
     extract_reim_split(p->sign, p->ophyssz, (bench_real *) p->out, &ro, &io);

     dims = bench_tensor_to_fftw_iodim(sz);
     howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
     if (verbose > 2) printf("using plan_guru_split_dft\n");
     pln = FFTW(plan_guru_split_dft)(sz->rnk, dims,
			       vecsz->rnk, howmany_dims,
			       ri, ii, ro, io, flags);
     bench_free(dims);
     bench_free(howmany_dims);
     return pln;
}

static FFTW(plan) mkplan_complex_interleaved(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln;
     bench_tensor *sz = p->sz, *vecsz = p->vecsz;

     if (vecsz->rnk == 0 && tensor_unitstridep(sz) && tensor_rowmajorp(sz)) 
	  goto api_simple;
     
     if (vecsz->rnk == 1 && expressible_as_api_many(sz))
	  goto api_many;

     goto api_guru;

 api_simple:
     switch (sz->rnk) {
	 case 1:
	      if (verbose > 2) printf("using plan_dft_1d\n");
	      return FFTW(plan_dft_1d)(sz->dims[0].n, 
				       (bench_complex *) p->in,
				       (bench_complex *) p->out, 
				       p->sign, flags);
	      break;
	 case 2:
	      if (verbose > 2) printf("using plan_dft_2d\n");
	      return FFTW(plan_dft_2d)(sz->dims[0].n, sz->dims[1].n,
				       (bench_complex *) p->in,
				       (bench_complex *) p->out, 
				       p->sign, flags);
	      break;
	 case 3:
	      if (verbose > 2) printf("using plan_dft_3d\n");
	      return FFTW(plan_dft_3d)(
		   sz->dims[0].n, sz->dims[1].n, sz->dims[2].n,
		   (bench_complex *) p->in, (bench_complex *) p->out, 
		   p->sign, flags);
	      break;
	 default: {
	      int *n = mkn(sz);
	      if (verbose > 2) printf("using plan_dft\n");
	      pln = FFTW(plan_dft)(sz->rnk, n, 
				   (bench_complex *) p->in, 
				   (bench_complex *) p->out, p->sign, flags);
	      bench_free(n);
	      return pln;
	 }
     }

 api_many:
     {
	  int *n, *inembed, *onembed;
	  BENCH_ASSERT(vecsz->rnk == 1);
	  n = mkn(sz);
	  mknembed_many(sz, &inembed, &onembed);
	  if (verbose > 2) printf("using plan_many_dft\n");
	  pln = FFTW(plan_many_dft)(
	       sz->rnk, n, vecsz->dims[0].n, 
	       (bench_complex *) p->in, 
	       inembed, sz->dims[sz->rnk - 1].is, vecsz->dims[0].is,
	       (bench_complex *) p->out,
	       onembed, sz->dims[sz->rnk - 1].os, vecsz->dims[0].os,
	       p->sign, flags);
	  bench_free(n); bench_free(inembed); bench_free(onembed);
	  return pln;
     }

 api_guru:
     {
	  FFTW(iodim) *dims, *howmany_dims;

	  dims = bench_tensor_to_fftw_iodim(sz);
	  howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
	  if (verbose > 2) printf("using plan_guru_dft\n");
	  pln = FFTW(plan_guru_dft)(sz->rnk, dims,
				    vecsz->rnk, howmany_dims,
				    (bench_complex *) p->in,
				    (bench_complex *) p->out,
				    p->sign, flags);
	  bench_free(dims);
	  bench_free(howmany_dims);
	  return pln;
     }
}

static FFTW(plan) mkplan_complex(bench_problem *p, unsigned flags)
{
     if (p->split)
	  return mkplan_complex_split(p, flags);
     else
	  return mkplan_complex_interleaved(p, flags);
}

static FFTW(plan) mkplan_r2r(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln;
     bench_tensor *sz = p->sz, *vecsz = p->vecsz;
     FFTW(r2r_kind) *k;

     k = (FFTW(r2r_kind) *) bench_malloc(sizeof(FFTW(r2r_kind)) * sz->rnk);
     {
	  int i;
	  for (i = 0; i < sz->rnk; ++i)
	       switch (p->k[i]) {
		   case R2R_R2HC: k[i] = FFTW_R2HC; break;
		   case R2R_HC2R: k[i] = FFTW_HC2R; break;
		   case R2R_DHT: k[i] = FFTW_DHT; break;
		   case R2R_REDFT00: k[i] = FFTW_REDFT00; break;
		   case R2R_REDFT01: k[i] = FFTW_REDFT01; break;
		   case R2R_REDFT10: k[i] = FFTW_REDFT10; break;
		   case R2R_REDFT11: k[i] = FFTW_REDFT11; break;
		   case R2R_RODFT00: k[i] = FFTW_RODFT00; break;
		   case R2R_RODFT01: k[i] = FFTW_RODFT01; break;
		   case R2R_RODFT10: k[i] = FFTW_RODFT10; break;
		   case R2R_RODFT11: k[i] = FFTW_RODFT11; break;
		   default: BENCH_ASSERT(0);
	       }
     }

     if (vecsz->rnk == 0 && tensor_unitstridep(sz) && tensor_rowmajorp(sz)) 
	  goto api_simple;
     
     if (vecsz->rnk == 1 && expressible_as_api_many(sz))
	  goto api_many;

     goto api_guru;

 api_simple:
     switch (sz->rnk) {
	 case 1:
	      if (verbose > 2) printf("using plan_r2r_1d\n");
	      pln = FFTW(plan_r2r_1d)(sz->dims[0].n, 
				      (bench_real *) p->in,
				      (bench_real *) p->out, 
				      k[0], flags);
	      goto done;
	 case 2:
	      if (verbose > 2) printf("using plan_r2r_2d\n");
	      pln = FFTW(plan_r2r_2d)(sz->dims[0].n, sz->dims[1].n,
				      (bench_real *) p->in,
				      (bench_real *) p->out, 
				      k[0], k[1], flags);
	      goto done;
	 case 3:
	      if (verbose > 2) printf("using plan_r2r_3d\n");
	      pln = FFTW(plan_r2r_3d)(
		   sz->dims[0].n, sz->dims[1].n, sz->dims[2].n,
		   (bench_real *) p->in, (bench_real *) p->out, 
		   k[0], k[1], k[2], flags);
	      goto done;
	 default: {
	      int *n = mkn(sz);
	      if (verbose > 2) printf("using plan_r2r\n");
	      pln = FFTW(plan_r2r)(sz->rnk, n,
				   (bench_real *) p->in, (bench_real *) p->out,
				   k, flags);
	      bench_free(n);
	      goto done;
	 }
     }

 api_many:
     {
	  int *n, *inembed, *onembed;
	  BENCH_ASSERT(vecsz->rnk == 1);
	  n = mkn(sz);
	  mknembed_many(sz, &inembed, &onembed);
	  if (verbose > 2) printf("using plan_many_r2r\n");
	  pln = FFTW(plan_many_r2r)(
	       sz->rnk, n, vecsz->dims[0].n, 
	       (bench_real *) p->in,
	       inembed, sz->dims[sz->rnk - 1].is, vecsz->dims[0].is,
	       (bench_real *) p->out,
	       onembed, sz->dims[sz->rnk - 1].os, vecsz->dims[0].os,
	       k, flags);
	  bench_free(n); bench_free(inembed); bench_free(onembed);
	  goto done;
     }

 api_guru:
     {
	  FFTW(iodim) *dims, *howmany_dims;

	  dims = bench_tensor_to_fftw_iodim(sz);
	  howmany_dims = bench_tensor_to_fftw_iodim(vecsz);
	  if (verbose > 2) printf("using plan_guru_r2r\n");
	  pln = FFTW(plan_guru_r2r)(sz->rnk, dims,
				    vecsz->rnk, howmany_dims,
				    (bench_real *) p->in, 
				    (bench_real *) p->out, k, flags);
	  bench_free(dims);
	  bench_free(howmany_dims);
	  goto done;
     }
     
 done:
     bench_free(k);
     return pln;
}

FFTW(plan) mkplan(bench_problem *p, unsigned flags)
{
     switch (p->kind) {
	 case PROBLEM_COMPLEX:	  return mkplan_complex(p, flags);
	 case PROBLEM_REAL:	  return mkplan_real(p, flags);
	 case PROBLEM_R2R:        return mkplan_r2r(p, flags);
	 default: BENCH_ASSERT(0); return 0;
     }
}

void main_init(int *argc, char ***argv)
{
     UNUSED(argc);
     UNUSED(argv);
}

void initial_cleanup(void)
{
}

void final_cleanup(void)
{
}

int import_wisdom(FILE *f)
{
     return FFTW(import_wisdom_from_file)(f);
}

void export_wisdom(FILE *f)
{
     FFTW(export_wisdom_to_file)(f);
}
