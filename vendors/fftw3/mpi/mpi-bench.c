/**************************************************************************/
/* NOTE to users: this is the FFTW-MPI self-test and benchmark program.
   It is probably NOT a good place to learn FFTW usage, since it has a
   lot of added complexity in order to exercise and test the full API,
   etcetera.  We suggest reading the manual. */
/**************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fftw3-mpi.h"
#include "tests/fftw-bench.h"

#if defined(BENCHFFT_SINGLE)
#  define BENCH_MPI_TYPE MPI_FLOAT
#elif defined(BENCHFFT_LDOUBLE)
#  define BENCH_MPI_TYPE MPI_LONG_DOUBLE
#elif defined(BENCHFFT_QUAD)
#  error MPI quad-precision type is unknown
#else
#  define BENCH_MPI_TYPE MPI_DOUBLE
#endif

#if SIZEOF_PTRDIFF_T == SIZEOF_INT
#  define FFTW_MPI_PTRDIFF_T MPI_INT
#elif SIZEOF_PTRDIFF_T == SIZEOF_LONG
#  define FFTW_MPI_PTRDIFF_T MPI_LONG
#elif SIZEOF_PTRDIFF_T == SIZEOF_LONG_LONG
#  define FFTW_MPI_PTRDIFF_T MPI_LONG_LONG
#else
#  error MPI type for ptrdiff_t is unknown
#  define FFTW_MPI_PTRDIFF_T MPI_LONG
#endif

static const char *mkversion(void) { return FFTW(version); }
static const char *mkcc(void) { return FFTW(cc); }
static const char *mkcodelet_optim(void) { return FFTW(codelet_optim); }
static const char *mknproc(void) {
     static char buf[32];
     int ncpus;
     MPI_Comm_size(MPI_COMM_WORLD, &ncpus);
#ifdef HAVE_SNPRINTF
     snprintf(buf, 32, "%d", ncpus);
#else
     sprintf(buf, "%d", ncpus);
#endif
     return buf;
}

BEGIN_BENCH_DOC
BENCH_DOC("name", "fftw3_mpi")
BENCH_DOCF("version", mkversion)
BENCH_DOCF("cc", mkcc)
BENCH_DOCF("codelet-optim", mkcodelet_optim)
BENCH_DOCF("nproc", mknproc)
END_BENCH_DOC 

static int n_pes = 1, my_pe = 0;

/* global variables describing the shape of the data and its distribution */
static int rnk;
static ptrdiff_t vn, iNtot, oNtot;
static ptrdiff_t *local_ni=0, *local_starti=0;
static ptrdiff_t *local_no=0, *local_starto=0;
static ptrdiff_t *all_local_ni=0, *all_local_starti=0; /* n_pes x rnk arrays */
static ptrdiff_t *all_local_no=0, *all_local_starto=0; /* n_pes x rnk arrays */
static ptrdiff_t *istrides = 0, *ostrides = 0;
static ptrdiff_t *total_ni=0, *total_no=0;
static int *isend_cnt = 0, *isend_off = 0; /* for MPI_Scatterv */
static int *orecv_cnt = 0, *orecv_off = 0; /* for MPI_Gatherv */

static bench_real *local_in = 0, *local_out = 0;
static bench_real *all_local_in = 0, *all_local_out = 0;
static int all_local_in_alloc = 0, all_local_out_alloc = 0;
static FFTW(plan) plan_scramble_in = 0, plan_unscramble_out = 0;

static void alloc_rnk(int rnk_) {
     rnk = rnk_;
     bench_free(local_ni);
     if (rnk == 0)
	  local_ni = 0;
     else
	  local_ni = (ptrdiff_t *) bench_malloc(sizeof(ptrdiff_t) * rnk
						* (8 + n_pes * 4));

     local_starti = local_ni + rnk;
     local_no = local_ni + 2 * rnk;
     local_starto = local_ni + 3 * rnk;
     istrides = local_ni + 4 * rnk;
     ostrides = local_ni + 5 * rnk;
     total_ni = local_ni + 6 * rnk;
     total_no = local_ni + 7 * rnk;
     all_local_ni = local_ni + 8 * rnk;
     all_local_starti = local_ni + (8 + n_pes) * rnk;
     all_local_no = local_ni + (8 + 2 * n_pes) * rnk;
     all_local_starto = local_ni + (8 + 3 * n_pes) * rnk;
}

static void setup_gather_scatter(void)
{
     int i, j;
     ptrdiff_t off;

     MPI_Gather(local_ni, rnk, FFTW_MPI_PTRDIFF_T,
		all_local_ni, rnk, FFTW_MPI_PTRDIFF_T,
		0, MPI_COMM_WORLD);
     MPI_Bcast(all_local_ni, rnk*n_pes, FFTW_MPI_PTRDIFF_T, 0, MPI_COMM_WORLD);
     MPI_Gather(local_starti, rnk, FFTW_MPI_PTRDIFF_T,
		all_local_starti, rnk, FFTW_MPI_PTRDIFF_T,
		0, MPI_COMM_WORLD);
     MPI_Bcast(all_local_starti, rnk*n_pes, FFTW_MPI_PTRDIFF_T, 0, MPI_COMM_WORLD);

     MPI_Gather(local_no, rnk, FFTW_MPI_PTRDIFF_T,
		all_local_no, rnk, FFTW_MPI_PTRDIFF_T,
		0, MPI_COMM_WORLD);
     MPI_Bcast(all_local_no, rnk*n_pes, FFTW_MPI_PTRDIFF_T, 0, MPI_COMM_WORLD);
     MPI_Gather(local_starto, rnk, FFTW_MPI_PTRDIFF_T,
		all_local_starto, rnk, FFTW_MPI_PTRDIFF_T,
		0, MPI_COMM_WORLD);
     MPI_Bcast(all_local_starto, rnk*n_pes, FFTW_MPI_PTRDIFF_T, 0, MPI_COMM_WORLD);

     off = 0;
     for (i = 0; i < n_pes; ++i) {
	  ptrdiff_t N = vn;
	  for (j = 0; j < rnk; ++j)
	       N *= all_local_ni[i * rnk + j];
	  isend_cnt[i] = N;
	  isend_off[i] = off;
	  off += N;
     }
     iNtot = off;
     all_local_in_alloc = 1;

     istrides[rnk - 1] = vn;
     for (j = rnk - 2; j >= 0; --j)
	  istrides[j] = total_ni[j + 1] * istrides[j + 1];

     off = 0;
     for (i = 0; i < n_pes; ++i) {
	  ptrdiff_t N = vn;
	  for (j = 0; j < rnk; ++j)
	       N *= all_local_no[i * rnk + j];
	  orecv_cnt[i] = N;
	  orecv_off[i] = off;
	  off += N;
     }
     oNtot = off;
     all_local_out_alloc = 1;

     ostrides[rnk - 1] = vn;
     for (j = rnk - 2; j >= 0; --j)
	  ostrides[j] = total_no[j + 1] * ostrides[j + 1];
}

static void copy_block_out(const bench_real *in,
			   int rnk, ptrdiff_t *n, ptrdiff_t *start, 
			   ptrdiff_t is, ptrdiff_t *os, ptrdiff_t vn,
			   bench_real *out)
{
     ptrdiff_t i;
     if (rnk == 0) { 
	  for (i = 0; i < vn; ++i)
	       out[i] = in[i];
     }
     else if (rnk == 1) { /* this case is just an optimization */
	  ptrdiff_t j;
	  out += start[0] * os[0];
	  for (j = 0; j < n[0]; ++j) {
	       for (i = 0; i < vn; ++i)
		    out[i] = in[i];
	       in += is;
	       out += os[0];
	  }
     }
     else {
	  /* we should do n[0] for locality, but this way is simpler to code */
	  for (i = 0; i < n[rnk - 1]; ++i) 
	       copy_block_out(in + i * is,
			      rnk - 1, n, start, is * n[rnk - 1], os, vn,
			      out + (start[rnk - 1] + i) * os[rnk - 1]);
     }
}

static void copy_block_in(bench_real *in,
			  int rnk, ptrdiff_t *n, ptrdiff_t *start, 
			  ptrdiff_t is, ptrdiff_t *os, ptrdiff_t vn,
			  const bench_real *out)
{
     ptrdiff_t i;
     if (rnk == 0) { 
	  for (i = 0; i < vn; ++i)
	       in[i] = out[i];
     }
     else if (rnk == 1) { /* this case is just an optimization */
	  ptrdiff_t j;
	  out += start[0] * os[0];
	  for (j = 0; j < n[0]; ++j) {
	       for (i = 0; i < vn; ++i)
		    in[i] = out[i];
	       in += is;
	       out += os[0];
	  }
     }
     else {
	  /* we should do n[0] for locality, but this way is simpler to code */
	  for (i = 0; i < n[rnk - 1]; ++i) 
	       copy_block_in(in + i * is,
			     rnk - 1, n, start, is * n[rnk - 1], os, vn,
			     out + (start[rnk - 1] + i) * os[rnk - 1]);
     }
}

static void do_scatter_in(bench_real *in)
{
     bench_real *ali;
     int i;
     if (all_local_in_alloc) {
          bench_free(all_local_in);
	  all_local_in = (bench_real*) bench_malloc(iNtot*sizeof(bench_real));
	  all_local_in_alloc = 0;
     }
     ali = all_local_in;
     for (i = 0; i < n_pes; ++i) {
	  copy_block_in(ali,
			rnk, all_local_ni + i * rnk, 
			all_local_starti + i * rnk,
			vn, istrides, vn,
			in);
	  ali += isend_cnt[i];
     }
     MPI_Scatterv(all_local_in, isend_cnt, isend_off, BENCH_MPI_TYPE,
		  local_in, isend_cnt[my_pe], BENCH_MPI_TYPE,
		  0, MPI_COMM_WORLD);
}

static void do_gather_out(bench_real *out)
{
     bench_real *alo;
     int i;

     if (all_local_out_alloc) {
          bench_free(all_local_out);
	  all_local_out = (bench_real*) bench_malloc(oNtot*sizeof(bench_real));
	  all_local_out_alloc = 0;
     }
     MPI_Gatherv(local_out, orecv_cnt[my_pe], BENCH_MPI_TYPE,
		 all_local_out, orecv_cnt, orecv_off, BENCH_MPI_TYPE,
		 0, MPI_COMM_WORLD);
     MPI_Bcast(all_local_out, oNtot, BENCH_MPI_TYPE, 0, MPI_COMM_WORLD);
     alo = all_local_out;
     for (i = 0; i < n_pes; ++i) {
	  copy_block_out(alo,
			 rnk, all_local_no + i * rnk, 
			 all_local_starto + i * rnk,
			 vn, ostrides, vn,
			 out);
	  alo += orecv_cnt[i];
     }
}

static void alloc_local(ptrdiff_t nreal, int inplace)
{
     bench_free(local_in);
     if (local_out != local_in) bench_free(local_out);
     local_in = local_out = 0;
     if (nreal > 0) {
	  ptrdiff_t i;
	  local_in = (bench_real*) bench_malloc(nreal * sizeof(bench_real));
	  if (inplace)
	       local_out = local_in;
	  else
	       local_out = (bench_real*) bench_malloc(nreal * sizeof(bench_real));
	  for (i = 0; i < nreal; ++i) local_in[i] = local_out[i] = 0.0;
     }
}

void after_problem_rcopy_from(bench_problem *p, bench_real *ri)
{
     UNUSED(p);
     do_scatter_in(ri);
     if (plan_scramble_in) FFTW(execute)(plan_scramble_in);
}

void after_problem_rcopy_to(bench_problem *p, bench_real *ro)
{
     UNUSED(p);
     if (plan_unscramble_out) FFTW(execute)(plan_unscramble_out);
     do_gather_out(ro);
}

void after_problem_ccopy_from(bench_problem *p, bench_real *ri, bench_real *ii)
{
     UNUSED(ii);
     after_problem_rcopy_from(p, ri);
}

void after_problem_ccopy_to(bench_problem *p, bench_real *ro, bench_real *io)
{
     UNUSED(io);
     after_problem_rcopy_to(p, ro);
}

void after_problem_hccopy_from(bench_problem *p, bench_real *ri, bench_real *ii)
{
     UNUSED(ii);
     after_problem_rcopy_from(p, ri);
}

void after_problem_hccopy_to(bench_problem *p, bench_real *ro, bench_real *io)
{
     UNUSED(io);
     after_problem_rcopy_to(p, ro);
}

static FFTW(plan) mkplan_transpose_local(ptrdiff_t nx, ptrdiff_t ny, 
					 ptrdiff_t vn, 
					 bench_real *in, bench_real *out)
{
     FFTW(iodim64) hdims[3];
     FFTW(r2r_kind) k[3];
     FFTW(plan) pln;

     hdims[0].n = nx;
     hdims[0].is = ny * vn;
     hdims[0].os = vn;
     hdims[1].n = ny;
     hdims[1].is = vn;
     hdims[1].os = nx * vn;
     hdims[2].n = vn;
     hdims[2].is = 1;
     hdims[2].os = 1;
     k[0] = k[1] = k[2] = FFTW_R2HC;
     pln = FFTW(plan_guru64_r2r)(0, 0, 3, hdims, in, out, k, FFTW_ESTIMATE);
     BENCH_ASSERT(pln != 0);
     return pln;
}

static int tensor_rowmajor_transposedp(bench_tensor *t)
{
     bench_iodim *d;
     int i;

     BENCH_ASSERT(BENCH_FINITE_RNK(t->rnk));
     if (t->rnk < 2)
	  return 0;

     d = t->dims;
     if (d[0].is != d[1].is * d[1].n
	 || d[0].os != d[1].is
	 || d[1].os != d[0].os * d[0].n)
	  return 0;
     if (t->rnk > 2 && d[1].is != d[2].is * d[2].n)
	  return 0;
     for (i = 2; i + 1 < t->rnk; ++i) {
          d = t->dims + i;
          if (d[0].is != d[1].is * d[1].n
	      || d[0].os != d[1].os * d[1].n)
               return 0;
     }

     if (t->rnk > 2 && t->dims[t->rnk-1].is != t->dims[t->rnk-1].os)
	  return 0;
     return 1;
}

static int tensor_contiguousp(bench_tensor *t, int s)
{
     return (t->dims[t->rnk-1].is == s
	     && ((tensor_rowmajorp(t) && 
		  t->dims[t->rnk-1].is == t->dims[t->rnk-1].os)
		 || tensor_rowmajor_transposedp(t)));
}

static FFTW(plan) mkplan_complex(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln = 0;
     int i; 
     ptrdiff_t ntot;

     vn = p->vecsz->rnk == 1 ? p->vecsz->dims[0].n : 1;

     if (p->sz->rnk < 1
	 || p->split
	 || !tensor_contiguousp(p->sz, vn)
	 || tensor_rowmajor_transposedp(p->sz)
	 || p->vecsz->rnk > 1
	 || (p->vecsz->rnk == 1 && (p->vecsz->dims[0].is != 1
				    || p->vecsz->dims[0].os != 1)))
	  return 0;

     alloc_rnk(p->sz->rnk);
     for (i = 0; i < rnk; ++i) {
	  total_ni[i] = total_no[i] = p->sz->dims[i].n;
	  local_ni[i] = local_no[i] = total_ni[i];
	  local_starti[i] = local_starto[i] = 0;
     }
     if (rnk > 1) {
	  ptrdiff_t n, start, nT, startT;
	  ntot = FFTW(mpi_local_size_many_transposed)
	       (p->sz->rnk, total_ni, vn,
		FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
		MPI_COMM_WORLD,
		&n, &start, &nT, &startT);
	  if  (flags & FFTW_MPI_TRANSPOSED_IN) {
	       local_ni[1] = nT;
	       local_starti[1] = startT;
	  }
	  else {
	       local_ni[0] = n;
	       local_starti[0] = start;
	  }
	  if  (flags & FFTW_MPI_TRANSPOSED_OUT) {
	       local_no[1] = nT;
	       local_starto[1] = startT;
	  }
	  else {
	       local_no[0] = n;
	       local_starto[0] = start;
	  }
     }
     else if (rnk == 1) {
	  ntot = FFTW(mpi_local_size_many_1d)
	       (total_ni[0], vn, MPI_COMM_WORLD, p->sign, flags,
		local_ni, local_starti, local_no, local_starto);
     }
     alloc_local(ntot * 2, p->in == p->out);

     pln = FFTW(mpi_plan_many_dft)(p->sz->rnk, total_ni, vn, 
				   FFTW_MPI_DEFAULT_BLOCK,
				   FFTW_MPI_DEFAULT_BLOCK,
				   (FFTW(complex) *) local_in, 
				   (FFTW(complex) *) local_out,
				   MPI_COMM_WORLD, p->sign, flags);

     vn *= 2;

     if (rnk > 1) {
	  ptrdiff_t nrest = 1;
	  for (i = 2; i < rnk; ++i) nrest *= p->sz->dims[i].n;
	  if (flags & FFTW_MPI_TRANSPOSED_IN)
	       plan_scramble_in = mkplan_transpose_local(
		    p->sz->dims[0].n, local_ni[1], vn * nrest,
		    local_in, local_in);
	  if (flags & FFTW_MPI_TRANSPOSED_OUT)
	       plan_unscramble_out = mkplan_transpose_local(
		    local_no[1], p->sz->dims[0].n, vn * nrest,
		    local_out, local_out);
     }
     
     return pln;
}

static int tensor_real_contiguousp(bench_tensor *t, int sign, int s)
{
     return (t->dims[t->rnk-1].is == s
	     && ((tensor_real_rowmajorp(t, sign, 1) && 
		  t->dims[t->rnk-1].is == t->dims[t->rnk-1].os)));
}

static FFTW(plan) mkplan_real(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln = 0;
     int i; 
     ptrdiff_t ntot;

     vn = p->vecsz->rnk == 1 ? p->vecsz->dims[0].n : 1;

     if (p->sz->rnk < 2
	 || p->split
	 || !tensor_real_contiguousp(p->sz, p->sign, vn)
	 || tensor_rowmajor_transposedp(p->sz)
	 || p->vecsz->rnk > 1
	 || (p->vecsz->rnk == 1 && (p->vecsz->dims[0].is != 1
				    || p->vecsz->dims[0].os != 1)))
	  return 0;

     alloc_rnk(p->sz->rnk);
     for (i = 0; i < rnk; ++i) {
	  total_ni[i] = total_no[i] = p->sz->dims[i].n;
	  local_ni[i] = local_no[i] = total_ni[i];
	  local_starti[i] = local_starto[i] = 0;
     }
     local_ni[rnk-1] = local_no[rnk-1] = total_ni[rnk-1] = total_no[rnk-1] 
	  = p->sz->dims[rnk-1].n / 2 + 1;
     {
	  ptrdiff_t n, start, nT, startT;
	  ntot = FFTW(mpi_local_size_many_transposed)
	       (p->sz->rnk, total_ni, vn,
		FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
		MPI_COMM_WORLD,
		&n, &start, &nT, &startT);
	  if  (flags & FFTW_MPI_TRANSPOSED_IN) {
	       local_ni[1] = nT;
	       local_starti[1] = startT;
	  }
	  else {
	       local_ni[0] = n;
	       local_starti[0] = start;
	  }
	  if  (flags & FFTW_MPI_TRANSPOSED_OUT) {
	       local_no[1] = nT;
	       local_starto[1] = startT;
	  }
	  else {
	       local_no[0] = n;
	       local_starto[0] = start;
	  }
     }
     alloc_local(ntot * 2, p->in == p->out);

     total_ni[rnk - 1] = p->sz->dims[rnk - 1].n;
     if (p->sign < 0)
	  pln = FFTW(mpi_plan_many_dft_r2c)(p->sz->rnk, total_ni, vn, 
					    FFTW_MPI_DEFAULT_BLOCK,
					    FFTW_MPI_DEFAULT_BLOCK,
					    local_in, 
					    (FFTW(complex) *) local_out,
					    MPI_COMM_WORLD, flags);
     else
	  pln = FFTW(mpi_plan_many_dft_c2r)(p->sz->rnk, total_ni, vn, 
					    FFTW_MPI_DEFAULT_BLOCK,
					    FFTW_MPI_DEFAULT_BLOCK,
					    (FFTW(complex) *) local_in, 
					    local_out,
					    MPI_COMM_WORLD, flags);

     total_ni[rnk - 1] = p->sz->dims[rnk - 1].n / 2 + 1;
     vn *= 2;

     {
	  ptrdiff_t nrest = 1;
	  for (i = 2; i < rnk; ++i) nrest *= total_ni[i];
	  if (flags & FFTW_MPI_TRANSPOSED_IN)
	       plan_scramble_in = mkplan_transpose_local(
		    total_ni[0], local_ni[1], vn * nrest,
		    local_in, local_in);
	  if (flags & FFTW_MPI_TRANSPOSED_OUT)
	       plan_unscramble_out = mkplan_transpose_local(
		    local_no[1], total_ni[0], vn * nrest,
		    local_out, local_out);
     }
     
     return pln;
}

static FFTW(plan) mkplan_transpose(bench_problem *p, unsigned flags)
{
     ptrdiff_t ntot, nx, ny;
     int ix=0, iy=1, i;
     const bench_iodim *d = p->vecsz->dims;
     FFTW(plan) pln;

     if (p->vecsz->rnk == 3) {
	  for (i = 0; i < 3; ++i)
	       if (d[i].is == 1 && d[i].os == 1) {
		    vn = d[i].n;
		    ix = (i + 1) % 3;
		    iy = (i + 2) % 3;
		    break;
	       }
	  if (i == 3) return 0;
     }
     else {
	  vn = 1;
	  ix = 0;
	  iy = 1;
     }

     if (d[ix].is == d[iy].n * vn && d[ix].os == vn
	 && d[iy].os == d[ix].n * vn && d[iy].is == vn) {
	  nx = d[ix].n;
	  ny = d[iy].n;
     }
     else if (d[iy].is == d[ix].n * vn && d[iy].os == vn
	      && d[ix].os == d[iy].n * vn && d[ix].is == vn) {
	  nx = d[iy].n;
	  ny = d[ix].n;
     }
     else
	  return 0;

     alloc_rnk(2);
     ntot = vn * FFTW(mpi_local_size_2d_transposed)(nx, ny, MPI_COMM_WORLD,
						    &local_ni[0], 
						    &local_starti[0],
						    &local_no[0], 
						    &local_starto[0]);
     local_ni[1] = ny;
     local_starti[1] = 0;
     local_no[1] = nx;
     local_starto[1] = 0;
     total_ni[0] = nx; total_ni[1] = ny;
     total_no[1] = nx; total_no[0] = ny;
     alloc_local(ntot, p->in == p->out);

     pln = FFTW(mpi_plan_many_transpose)(nx, ny, vn,
					 FFTW_MPI_DEFAULT_BLOCK,
					 FFTW_MPI_DEFAULT_BLOCK,
					 local_in, local_out,
					 MPI_COMM_WORLD, flags);
     
     if (flags & FFTW_MPI_TRANSPOSED_IN)
	  plan_scramble_in = mkplan_transpose_local(local_ni[0], ny, vn,
						    local_in, local_in);
     if (flags & FFTW_MPI_TRANSPOSED_OUT)
	  plan_unscramble_out = mkplan_transpose_local
	       (nx, local_no[0], vn, local_out, local_out);
     
#if 0
     if (pln && vn == 1) {
	  int i, j;
	  bench_real *ri = (bench_real *) p->in;
	  bench_real *ro = (bench_real *) p->out;
	  if (!ri || !ro) return pln;
	  setup_gather_scatter();
	  for (i = 0; i < nx * ny; ++i)
	       ri[i] = i;
	  after_problem_rcopy_from(p, ri);
	  FFTW(execute)(pln);
	  after_problem_rcopy_to(p, ro);
	  if (my_pe == 0) {
	       for (i = 0; i < nx; ++i) {
		    for (j = 0; j < ny; ++j)
			 printf("  %3g", ro[j * nx + i]);
		    printf("\n");
	       }
	  }
     }
#endif

     return pln;
}

static FFTW(plan) mkplan_r2r(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln = 0;
     int i; 
     ptrdiff_t ntot;
     FFTW(r2r_kind) *k;

     if ((p->sz->rnk == 0 || (p->sz->rnk == 1 && p->sz->dims[0].n == 1))
	 && p->vecsz->rnk >= 2 && p->vecsz->rnk <= 3)
	  return mkplan_transpose(p, flags);

     vn = p->vecsz->rnk == 1 ? p->vecsz->dims[0].n : 1;

     if (p->sz->rnk < 1
	 || p->split
	 || !tensor_contiguousp(p->sz, vn)
	 || tensor_rowmajor_transposedp(p->sz)
	 || p->vecsz->rnk > 1
	 || (p->vecsz->rnk == 1 && (p->vecsz->dims[0].is != 1
				    || p->vecsz->dims[0].os != 1)))
	  return 0;

     alloc_rnk(p->sz->rnk);
     for (i = 0; i < rnk; ++i) {
	  total_ni[i] = total_no[i] = p->sz->dims[i].n;
	  local_ni[i] = local_no[i] = total_ni[i];
	  local_starti[i] = local_starto[i] = 0;
     }
     if (rnk > 1) {
	  ptrdiff_t n, start, nT, startT;
	  ntot = FFTW(mpi_local_size_many_transposed)
	       (p->sz->rnk, total_ni, vn,
		FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK,
		MPI_COMM_WORLD,
		&n, &start, &nT, &startT);
	  if  (flags & FFTW_MPI_TRANSPOSED_IN) {
	       local_ni[1] = nT;
	       local_starti[1] = startT;
	  }
	  else {
	       local_ni[0] = n;
	       local_starti[0] = start;
	  }
	  if  (flags & FFTW_MPI_TRANSPOSED_OUT) {
	       local_no[1] = nT;
	       local_starto[1] = startT;
	  }
	  else {
	       local_no[0] = n;
	       local_starto[0] = start;
	  }
     }
     else if (rnk == 1) {
	  ntot = FFTW(mpi_local_size_many_1d)
	       (total_ni[0], vn, MPI_COMM_WORLD, p->sign, flags,
		local_ni, local_starti, local_no, local_starto);
     }
     alloc_local(ntot, p->in == p->out);

     k = (FFTW(r2r_kind) *) bench_malloc(sizeof(FFTW(r2r_kind)) * p->sz->rnk);
     for (i = 0; i < p->sz->rnk; ++i)
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

     pln = FFTW(mpi_plan_many_r2r)(p->sz->rnk, total_ni, vn, 
				   FFTW_MPI_DEFAULT_BLOCK,
				   FFTW_MPI_DEFAULT_BLOCK,
				   local_in, local_out,
				   MPI_COMM_WORLD, k, flags);
     bench_free(k);

     if (rnk > 1) {
	  ptrdiff_t nrest = 1;
	  for (i = 2; i < rnk; ++i) nrest *= p->sz->dims[i].n;
	  if (flags & FFTW_MPI_TRANSPOSED_IN)
	       plan_scramble_in = mkplan_transpose_local(
		    p->sz->dims[0].n, local_ni[1], vn * nrest,
		    local_in, local_in);
	  if (flags & FFTW_MPI_TRANSPOSED_OUT)
	       plan_unscramble_out = mkplan_transpose_local(
		    local_no[1], p->sz->dims[0].n, vn * nrest,
		    local_out, local_out);
     }
     
     return pln;
}

FFTW(plan) mkplan(bench_problem *p, unsigned flags)
{
     FFTW(plan) pln = 0;
     FFTW(destroy_plan)(plan_scramble_in); plan_scramble_in = 0;
     FFTW(destroy_plan)(plan_unscramble_out); plan_unscramble_out = 0;
     if (p->scrambled_in) {
	  if (p->sz->rnk == 1 && p->sz->dims[0].n != 1) 
	       flags |= FFTW_MPI_SCRAMBLED_IN;
	  else
	       flags |= FFTW_MPI_TRANSPOSED_IN;
     }
     if (p->scrambled_out) {
	  if (p->sz->rnk == 1 && p->sz->dims[0].n != 1) 
	       flags |= FFTW_MPI_SCRAMBLED_OUT;
	  else
	       flags |= FFTW_MPI_TRANSPOSED_OUT;
     }
     switch (p->kind) {
         case PROBLEM_COMPLEX: 
	      pln =mkplan_complex(p, flags);
	      break;
         case PROBLEM_REAL: 
	      pln = mkplan_real(p, flags);
	      break;
         case PROBLEM_R2R:
	      pln = mkplan_r2r(p, flags);
	      break;
         default: BENCH_ASSERT(0);
     }
     if (pln) setup_gather_scatter();
     return pln;
}

void main_init(int *argc, char ***argv)
{
#ifdef HAVE_SMP
# if MPI_VERSION >= 2 /* for MPI_Init_thread */
     int provided;
     MPI_Init_thread(argc, argv, MPI_THREAD_FUNNELED, &provided);
     threads_ok = provided >= MPI_THREAD_FUNNELED;
# else
     MPI_Init(argc, argv);
     threads_ok = 0;
# endif
#else
     MPI_Init(argc, argv);
#endif
     MPI_Comm_rank(MPI_COMM_WORLD, &my_pe);
     MPI_Comm_size(MPI_COMM_WORLD, &n_pes);
     if (my_pe != 0) verbose = -999;
     no_speed_allocation = 1; /* so we can benchmark transforms > memory */
     always_pad_real = 1; /* out-of-place real transforms are padded */
     isend_cnt = (int *) bench_malloc(sizeof(int) * n_pes);
     isend_off = (int *) bench_malloc(sizeof(int) * n_pes);
     orecv_cnt = (int *) bench_malloc(sizeof(int) * n_pes);
     orecv_off = (int *) bench_malloc(sizeof(int) * n_pes);

     /* init_threads must be called before any other FFTW function,
	including mpi_init, because it has to register the threads hooks
	before the planner is initalized */
#ifdef HAVE_SMP
     if (threads_ok) { BENCH_ASSERT(FFTW(init_threads)()); }
#endif
     FFTW(mpi_init)();
}

void initial_cleanup(void)
{
     alloc_rnk(0);
     alloc_local(0, 0);
     bench_free(all_local_in); all_local_in = 0;
     bench_free(all_local_out); all_local_out = 0;
     bench_free(isend_off); isend_off = 0;
     bench_free(isend_cnt); isend_cnt = 0;
     bench_free(orecv_off); orecv_off = 0;
     bench_free(orecv_cnt); orecv_cnt = 0;
     FFTW(destroy_plan)(plan_scramble_in); plan_scramble_in = 0;
     FFTW(destroy_plan)(plan_unscramble_out); plan_unscramble_out = 0;
}

void final_cleanup(void)
{
     MPI_Finalize();
}

void bench_exit(int status)
{
     MPI_Abort(MPI_COMM_WORLD, status);
}

double bench_cost_postprocess(double cost)
{
     double cost_max;
     MPI_Allreduce(&cost, &cost_max, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
     return cost_max;
}


int import_wisdom(FILE *f)
{
     int success = 1, sall;
     if (my_pe == 0) success = FFTW(import_wisdom_from_file)(f);
     FFTW(mpi_broadcast_wisdom)(MPI_COMM_WORLD);
     MPI_Allreduce(&success, &sall, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
     return sall;
}

void export_wisdom(FILE *f)
{
     FFTW(mpi_gather_wisdom)(MPI_COMM_WORLD);
     if (my_pe == 0) FFTW(export_wisdom_to_file)(f);
}
