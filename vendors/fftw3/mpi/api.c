/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
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

#include "api/api.h"
#include "fftw3-mpi.h"
#include "ifftw-mpi.h"
#include "mpi-transpose.h"
#include "mpi-dft.h"
#include "mpi-rdft.h"
#include "mpi-rdft2.h"

/* Convert API flags to internal MPI flags. */
#define MPI_FLAGS(f) ((f) >> 27)

/*************************************************************************/

static int mpi_inited = 0;

static MPI_Comm problem_comm(const problem *p) {
     switch (p->adt->problem_kind) {
	 case PROBLEM_MPI_DFT:
	      return ((const problem_mpi_dft *) p)->comm;
	 case PROBLEM_MPI_RDFT:
	      return ((const problem_mpi_rdft *) p)->comm;
	 case PROBLEM_MPI_RDFT2:
	      return ((const problem_mpi_rdft2 *) p)->comm;
	 case PROBLEM_MPI_TRANSPOSE:
	      return ((const problem_mpi_transpose *) p)->comm;
	 default:
	      return MPI_COMM_NULL;
     }
}

/* used to synchronize cost measurements (timing or estimation)
   across all processes for an MPI problem, which is critical to
   ensure that all processes decide to use the same MPI plans
   (whereas serial plans need not be syncronized). */
static double cost_hook(const problem *p, double t, cost_kind k)
{
     MPI_Comm comm = problem_comm(p);
     double tsum;
     if (comm == MPI_COMM_NULL) return t;
     MPI_Allreduce(&t, &tsum, 1, MPI_DOUBLE, 
		   k == COST_SUM ? MPI_SUM : MPI_MAX, comm);
     return tsum;
}

/* Used to reject wisdom that is not in sync across all processes
   for an MPI problem, which is critical to ensure that all processes
   decide to use the same MPI plans.  (Even though costs are synchronized,
   above, out-of-sync wisdom may result from plans being produced
   by communicators that do not span all processes, either from a
   user-specified communicator or e.g. from transpose-recurse. */
static int wisdom_ok_hook(const problem *p, flags_t flags)
{
     MPI_Comm comm = problem_comm(p);
     int eq_me, eq_all;
     /* unpack flags bitfield, since MPI communications may involve
	byte-order changes and MPI cannot do this for bit fields */
#if SIZEOF_UNSIGNED_INT >= 4 /* must be big enough to hold 20-bit fields */
     unsigned int f[5];
#else
     unsigned long f[5]; /* at least 32 bits as per C standard */
#endif

     if (comm == MPI_COMM_NULL) return 1; /* non-MPI wisdom is always ok */

     if (XM(any_true)(0, comm)) return 0; /* some process had nowisdom_hook */

     /* otherwise, check that the flags and solver index are identical
	on all processes in this problem's communicator.

	TO DO: possibly we can relax strict equality, but it is
	critical to ensure that any flags which affect what plan is
	created (and whether the solver is applicable) are the same,
	e.g. DESTROY_INPUT, NO_UGLY, etcetera.  (If the MPI algorithm
	differs between processes, deadlocks/crashes generally result.) */
     f[0] = flags.l;
     f[1] = flags.hash_info;
     f[2] = flags.timelimit_impatience;
     f[3] = flags.u;
     f[4] = flags.slvndx;
     MPI_Bcast(f, 5, 
	       SIZEOF_UNSIGNED_INT >= 4 ? MPI_UNSIGNED : MPI_UNSIGNED_LONG,
	       0, comm);
     eq_me = f[0] == flags.l && f[1] == flags.hash_info
	  && f[2] == flags.timelimit_impatience
	  && f[3] == flags.u && f[4] == flags.slvndx;
     MPI_Allreduce(&eq_me, &eq_all, 1, MPI_INT, MPI_LAND, comm);
     return eq_all;
}

/* This hook is called when wisdom is not found.  The any_true here
   matches up with the any_true in wisdom_ok_hook, in order to handle
   the case where some processes had wisdom (and called wisdom_ok_hook)
   and some processes didn't have wisdom (and called nowisdom_hook). */
static void nowisdom_hook(const problem *p)
{
     MPI_Comm comm = problem_comm(p);
     if (comm == MPI_COMM_NULL) return; /* nothing to do for non-MPI p */
     XM(any_true)(1, comm); /* signal nowisdom to any wisdom_ok_hook */
}

/* needed to synchronize planner bogosity flag, in case non-MPI problems
   on a subset of processes encountered bogus wisdom */
static wisdom_state_t bogosity_hook(wisdom_state_t state, const problem *p)
{
     MPI_Comm comm = problem_comm(p);
     if (comm != MPI_COMM_NULL /* an MPI problem */
	 && XM(any_true)(state == WISDOM_IS_BOGUS, comm)) /* bogus somewhere */
	  return WISDOM_IS_BOGUS;
     return state;
}

void XM(init)(void)
{
     if (!mpi_inited) {
	  planner *plnr = X(the_planner)();
	  plnr->cost_hook = cost_hook;
	  plnr->wisdom_ok_hook = wisdom_ok_hook;
	  plnr->nowisdom_hook = nowisdom_hook;
	  plnr->bogosity_hook = bogosity_hook;
          XM(conf_standard)(plnr);
	  mpi_inited = 1;	  
     }
}

void XM(cleanup)(void)
{
     X(cleanup)();
     mpi_inited = 0;
}

/*************************************************************************/

static dtensor *mkdtensor_api(int rnk, const XM(ddim) *dims0)
{
     dtensor *x = XM(mkdtensor)(rnk);
     int i;
     for (i = 0; i < rnk; ++i) {
	  x->dims[i].n = dims0[i].n;
	  x->dims[i].b[IB] = dims0[i].ib;
	  x->dims[i].b[OB] = dims0[i].ob;
     }
     return x;
}

static dtensor *default_sz(int rnk, const XM(ddim) *dims0, int n_pes,
			   int rdft2)
{
     dtensor *sz = XM(mkdtensor)(rnk);
     dtensor *sz0 = mkdtensor_api(rnk, dims0);
     block_kind k;
     int i;

     for (i = 0; i < rnk; ++i)
	  sz->dims[i].n = dims0[i].n;

     if (rdft2) sz->dims[rnk-1].n = dims0[rnk-1].n / 2 + 1;

     for (i = 0; i < rnk; ++i) {
	  sz->dims[i].b[IB] = dims0[i].ib ? dims0[i].ib : sz->dims[i].n;
	  sz->dims[i].b[OB] = dims0[i].ob ? dims0[i].ob : sz->dims[i].n;
     }

     /* If we haven't used all of the processes yet, and some of the
	block sizes weren't specified (i.e. 0), then set the
	unspecified blocks so as to use as many processes as
	possible with as few distributed dimensions as possible. */
     FORALL_BLOCK_KIND(k) {
	  INT nb = XM(num_blocks_total)(sz, k);
	  INT np = n_pes / nb;
	  for (i = 0; i < rnk && np > 1; ++i)
	       if (!sz0->dims[i].b[k]) {
		    sz->dims[i].b[k] = XM(default_block)(sz->dims[i].n, np);
		    nb *= XM(num_blocks)(sz->dims[i].n, sz->dims[i].b[k]);
		    np = n_pes / nb;
	       }
     }

     if (rdft2) sz->dims[rnk-1].n = dims0[rnk-1].n;

     /* punt for 1d prime */
     if (rnk == 1 && X(is_prime)(sz->dims[0].n))
	  sz->dims[0].b[IB] = sz->dims[0].b[OB] = sz->dims[0].n;

     XM(dtensor_destroy)(sz0);
     sz0 = XM(dtensor_canonical)(sz, 0);
     XM(dtensor_destroy)(sz);
     return sz0;
}

/* allocate simple local (serial) dims array corresponding to n[rnk] */
static XM(ddim) *simple_dims(int rnk, const ptrdiff_t *n)
{
     XM(ddim) *dims = (XM(ddim) *) MALLOC(sizeof(XM(ddim)) * rnk,
						TENSORS);
     int i;
     for (i = 0; i < rnk; ++i)
	  dims[i].n = dims[i].ib = dims[i].ob = n[i];
     return dims;
}

/*************************************************************************/

static void local_size(int my_pe, const dtensor *sz, block_kind k,
		       ptrdiff_t *local_n, ptrdiff_t *local_start)
{
     int i;
     if (my_pe >= XM(num_blocks_total)(sz, k))
	  for (i = 0; i < sz->rnk; ++i)
	       local_n[i] = local_start[i] = 0;
     else {
	  XM(block_coords)(sz, k, my_pe, local_start);
	  for (i = 0; i < sz->rnk; ++i) {
	       local_n[i] = XM(block)(sz->dims[i].n, sz->dims[i].b[k],
				      local_start[i]);
	       local_start[i] *= sz->dims[i].b[k];
	  }
     }
}

static INT prod(int rnk, const ptrdiff_t *local_n) 
{
     int i;
     INT N = 1;
     for (i = 0; i < rnk; ++i) N *= local_n[i];
     return N;
}

ptrdiff_t XM(local_size_guru)(int rnk, const XM(ddim) *dims0,
			      ptrdiff_t howmany, MPI_Comm comm,
			      ptrdiff_t *local_n_in,
			      ptrdiff_t *local_start_in,
			      ptrdiff_t *local_n_out, 
			      ptrdiff_t *local_start_out,
			      int sign, unsigned flags)
{
     INT N;
     int my_pe, n_pes, i;
     dtensor *sz;

     if (rnk == 0)
	  return howmany;

     MPI_Comm_rank(comm, &my_pe);
     MPI_Comm_size(comm, &n_pes);
     sz = default_sz(rnk, dims0, n_pes, 0);

     /* Now, we must figure out how much local space the user should
	allocate (or at least an upper bound).  This depends strongly
	on the exact algorithms we employ...ugh!  FIXME: get this info
	from the solvers somehow? */
     N = 1; /* never return zero allocation size */
     if (rnk > 1 && XM(is_block1d)(sz, IB) && XM(is_block1d)(sz, OB)) {
	  INT Nafter;
	  ddim odims[2];

	  /* dft-rank-geq2-transposed */
	  odims[0] = sz->dims[0]; odims[1] = sz->dims[1]; /* save */
	  /* we may need extra space for transposed intermediate data */
	  for (i = 0; i < 2; ++i)
	       if (XM(num_blocks)(sz->dims[i].n, sz->dims[i].b[IB]) == 1 &&
		   XM(num_blocks)(sz->dims[i].n, sz->dims[i].b[OB]) == 1) {
		    sz->dims[i].b[IB]
			 = XM(default_block)(sz->dims[i].n, n_pes);
		    sz->dims[1-i].b[IB] = sz->dims[1-i].n;
		    local_size(my_pe, sz, IB, local_n_in, local_start_in);
		    N = X(imax)(N, prod(rnk, local_n_in));
		    sz->dims[i] = odims[i];
		    sz->dims[1-i] = odims[1-i];
		    break;
	       }

	  /* dft-rank-geq2 */
	  Nafter = howmany;
	  for (i = 1; i < sz->rnk; ++i) Nafter *= sz->dims[i].n;
	  N = X(imax)(N, (sz->dims[0].n
			  * XM(block)(Nafter, XM(default_block)(Nafter, n_pes),
				      my_pe) + howmany - 1) / howmany);

	  /* dft-rank-geq2 with dimensions swapped */
	  Nafter = howmany * sz->dims[0].n;
          for (i = 2; i < sz->rnk; ++i) Nafter *= sz->dims[i].n;
          N = X(imax)(N, (sz->dims[1].n
                          * XM(block)(Nafter, XM(default_block)(Nafter, n_pes),
                                      my_pe) + howmany - 1) / howmany);
     }
     else if (rnk == 1) {
	  if (howmany >= n_pes && !MPI_FLAGS(flags)) { /* dft-rank1-bigvec */
	       ptrdiff_t n[2], start[2];
	       dtensor *sz2 = XM(mkdtensor)(2);
	       sz2->dims[0] = sz->dims[0];
	       sz2->dims[0].b[IB] = sz->dims[0].n;
	       sz2->dims[1].n = sz2->dims[1].b[OB] = howmany;
	       sz2->dims[1].b[IB] = XM(default_block)(howmany, n_pes);
	       local_size(my_pe, sz2, IB, n, start);
	       XM(dtensor_destroy)(sz2);
	       N = X(imax)(N, (prod(2, n) + howmany - 1) / howmany);
	  }
	  else { /* dft-rank1 */
	       INT r, m, rblock[2], mblock[2];

	       /* Since the 1d transforms are so different, we require
		  the user to call local_size_1d for this case.  Ugh. */
	       CK(sign == FFTW_FORWARD || sign == FFTW_BACKWARD);

	       if ((r = XM(choose_radix)(sz->dims[0], n_pes, flags, sign,
					 rblock, mblock))) {
		    m = sz->dims[0].n / r;
		    if (flags & FFTW_MPI_SCRAMBLED_IN)
			 sz->dims[0].b[IB] = rblock[IB] * m;
		    else { /* !SCRAMBLED_IN */
			 sz->dims[0].b[IB] = r * mblock[IB];
			 N = X(imax)(N, rblock[IB] * m);
		    }
		    if (flags & FFTW_MPI_SCRAMBLED_OUT)
			 sz->dims[0].b[OB] = r * mblock[OB];
		    else { /* !SCRAMBLED_OUT */
			 N = X(imax)(N, r * mblock[OB]);
			 sz->dims[0].b[OB] = rblock[OB] * m;
		    }
	       }
	  }
     }

     local_size(my_pe, sz, IB, local_n_in, local_start_in);
     local_size(my_pe, sz, OB, local_n_out, local_start_out);

     /* at least, make sure we have enough space to store input & output */
     N = X(imax)(N, X(imax)(prod(rnk, local_n_in), prod(rnk, local_n_out)));

     XM(dtensor_destroy)(sz);
     return N * howmany;
}

ptrdiff_t XM(local_size_many_transposed)(int rnk, const ptrdiff_t *n,
					 ptrdiff_t howmany,
					 ptrdiff_t xblock, ptrdiff_t yblock,
					 MPI_Comm comm,
					 ptrdiff_t *local_nx,
					 ptrdiff_t *local_x_start,
					 ptrdiff_t *local_ny,
					 ptrdiff_t *local_y_start)
{
     ptrdiff_t N;
     XM(ddim) *dims; 
     ptrdiff_t *local;

     if (rnk == 0) {
	  *local_nx = *local_ny = 1;
	  *local_x_start = *local_y_start = 0;
	  return howmany;
     }

     dims = simple_dims(rnk, n);
     local = (ptrdiff_t *) MALLOC(sizeof(ptrdiff_t) * rnk * 4, TENSORS);

     /* default 1d block distribution, with transposed output
        if yblock < n[1] */
     dims[0].ib = xblock;
     if (rnk > 1) {
	  if (yblock < n[1])
	       dims[1].ob = yblock;
	  else
	       dims[0].ob = xblock;
     }
     else
	  dims[0].ob = xblock; /* FIXME: 1d not really supported here 
				         since we don't have flags/sign */
     
     N = XM(local_size_guru)(rnk, dims, howmany, comm, 
			     local, local + rnk,
			     local + 2*rnk, local + 3*rnk,
			     0, 0);
     *local_nx = local[0];
     *local_x_start = local[rnk];
     if (rnk > 1) {
	  *local_ny = local[2*rnk + 1];
	  *local_y_start = local[3*rnk + 1];
     }
     else {
	  *local_ny = *local_nx;
	  *local_y_start = *local_x_start;
     }
     X(ifree)(local);
     X(ifree)(dims);
     return N;
}

ptrdiff_t XM(local_size_many)(int rnk, const ptrdiff_t *n,
			      ptrdiff_t howmany, 
			      ptrdiff_t xblock,
			      MPI_Comm comm,
			      ptrdiff_t *local_nx,
			      ptrdiff_t *local_x_start)
{
     ptrdiff_t local_ny, local_y_start;
     return XM(local_size_many_transposed)(rnk, n, howmany,
					   xblock, rnk > 1 
					   ? n[1] : FFTW_MPI_DEFAULT_BLOCK,
					   comm,
					   local_nx, local_x_start,
					   &local_ny, &local_y_start);
}


ptrdiff_t XM(local_size_transposed)(int rnk, const ptrdiff_t *n,
				    MPI_Comm comm,
				    ptrdiff_t *local_nx,
				    ptrdiff_t *local_x_start,
				    ptrdiff_t *local_ny,
				    ptrdiff_t *local_y_start)
{
     return XM(local_size_many_transposed)(rnk, n, 1,
					   FFTW_MPI_DEFAULT_BLOCK,
					   FFTW_MPI_DEFAULT_BLOCK,
					   comm,
					   local_nx, local_x_start,
					   local_ny, local_y_start);
}

ptrdiff_t XM(local_size)(int rnk, const ptrdiff_t *n,
			 MPI_Comm comm,
			 ptrdiff_t *local_nx,
			 ptrdiff_t *local_x_start)
{
     return XM(local_size_many)(rnk, n, 1, FFTW_MPI_DEFAULT_BLOCK, comm,
				local_nx, local_x_start);
}

ptrdiff_t XM(local_size_many_1d)(ptrdiff_t nx, ptrdiff_t howmany, 
				 MPI_Comm comm, int sign, unsigned flags,
				 ptrdiff_t *local_nx, ptrdiff_t *local_x_start,
				 ptrdiff_t *local_ny, ptrdiff_t *local_y_start)
{
     XM(ddim) d;
     d.n = nx;
     d.ib = d.ob = FFTW_MPI_DEFAULT_BLOCK;
     return XM(local_size_guru)(1, &d, howmany, comm,
				local_nx, local_x_start,
				local_ny, local_y_start, sign, flags);
}

ptrdiff_t XM(local_size_1d)(ptrdiff_t nx,
			    MPI_Comm comm, int sign, unsigned flags,
			    ptrdiff_t *local_nx, ptrdiff_t *local_x_start,
			    ptrdiff_t *local_ny, ptrdiff_t *local_y_start)
{
     return XM(local_size_many_1d)(nx, 1, comm, sign, flags,
				   local_nx, local_x_start,
				   local_ny, local_y_start);
}

ptrdiff_t XM(local_size_2d_transposed)(ptrdiff_t nx, ptrdiff_t ny,
				       MPI_Comm comm,
				       ptrdiff_t *local_nx,
				       ptrdiff_t *local_x_start,
				       ptrdiff_t *local_ny, 
				       ptrdiff_t *local_y_start)
{
     ptrdiff_t n[2];
     n[0] = nx; n[1] = ny;
     return XM(local_size_transposed)(2, n, comm,
				      local_nx, local_x_start,
				      local_ny, local_y_start);
}

ptrdiff_t XM(local_size_2d)(ptrdiff_t nx, ptrdiff_t ny, MPI_Comm comm,
			       ptrdiff_t *local_nx, ptrdiff_t *local_x_start)
{
     ptrdiff_t n[2];
     n[0] = nx; n[1] = ny;
     return XM(local_size)(2, n, comm, local_nx, local_x_start);
}

ptrdiff_t XM(local_size_3d_transposed)(ptrdiff_t nx, ptrdiff_t ny,
				       ptrdiff_t nz,
				       MPI_Comm comm,
				       ptrdiff_t *local_nx,
				       ptrdiff_t *local_x_start,
				       ptrdiff_t *local_ny, 
				       ptrdiff_t *local_y_start)
{
     ptrdiff_t n[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     return XM(local_size_transposed)(3, n, comm,
				      local_nx, local_x_start,
				      local_ny, local_y_start);
}

ptrdiff_t XM(local_size_3d)(ptrdiff_t nx, ptrdiff_t ny, ptrdiff_t nz,
			    MPI_Comm comm,
			    ptrdiff_t *local_nx, ptrdiff_t *local_x_start)
{
     ptrdiff_t n[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     return XM(local_size)(3, n, comm, local_nx, local_x_start);
}

/*************************************************************************/
/* Transpose API */

X(plan) XM(plan_many_transpose)(ptrdiff_t nx, ptrdiff_t ny, 
				ptrdiff_t howmany,
				ptrdiff_t xblock, ptrdiff_t yblock,
				R *in, R *out, 
				MPI_Comm comm, unsigned flags)
{
     int n_pes;
     XM(init)();

     if (howmany < 0 || xblock < 0 || yblock < 0 ||
	 nx <= 0 || ny <= 0) return 0;

     MPI_Comm_size(comm, &n_pes);
     if (!xblock) xblock = XM(default_block)(nx, n_pes);
     if (!yblock) yblock = XM(default_block)(ny, n_pes);
     if (n_pes < XM(num_blocks)(nx, xblock)
	 || n_pes < XM(num_blocks)(ny, yblock))
	  return 0;

     return 
	  X(mkapiplan)(FFTW_FORWARD, flags,
		       XM(mkproblem_transpose)(nx, ny, howmany,
					       in, out, xblock, yblock,
					       comm, MPI_FLAGS(flags)));
}

X(plan) XM(plan_transpose)(ptrdiff_t nx, ptrdiff_t ny, R *in, R *out, 
			   MPI_Comm comm, unsigned flags)
			      
{
     return XM(plan_many_transpose)(nx, ny, 1,
				    FFTW_MPI_DEFAULT_BLOCK,
				    FFTW_MPI_DEFAULT_BLOCK,
				    in, out, comm, flags);
}

/*************************************************************************/
/* Complex DFT API */

X(plan) XM(plan_guru_dft)(int rnk, const XM(ddim) *dims0,
			  ptrdiff_t howmany,
			  C *in, C *out,
			  MPI_Comm comm, int sign, unsigned flags)
{
     int n_pes, i;
     dtensor *sz;
     
     XM(init)();

     if (howmany < 0 || rnk < 1) return 0;
     for (i = 0; i < rnk; ++i)
	  if (dims0[i].n < 1 || dims0[i].ib < 0 || dims0[i].ob < 0)
	       return 0;

     MPI_Comm_size(comm, &n_pes);
     sz = default_sz(rnk, dims0, n_pes, 0);

     if (XM(num_blocks_total)(sz, IB) > n_pes
	 || XM(num_blocks_total)(sz, OB) > n_pes) {
	  XM(dtensor_destroy)(sz);
	  return 0;
     }

     return
          X(mkapiplan)(sign, flags,
                       XM(mkproblem_dft_d)(sz, howmany,
					   (R *) in, (R *) out,
					   comm, sign, 
					   MPI_FLAGS(flags)));
}

X(plan) XM(plan_many_dft)(int rnk, const ptrdiff_t *n,
			  ptrdiff_t howmany,
			  ptrdiff_t iblock, ptrdiff_t oblock,
			  C *in, C *out,
			  MPI_Comm comm, int sign, unsigned flags)
{
     XM(ddim) *dims = simple_dims(rnk, n);
     X(plan) pln;

     if (rnk == 1) {
	  dims[0].ib = iblock;
	  dims[0].ob = oblock;
     }
     else if (rnk > 1) {
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_IN)].ib = iblock;
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_OUT)].ob = oblock;
     }

     pln = XM(plan_guru_dft)(rnk,dims,howmany, in,out, comm, sign, flags);
     X(ifree)(dims);
     return pln;
}

X(plan) XM(plan_dft)(int rnk, const ptrdiff_t *n, C *in, C *out,
		     MPI_Comm comm, int sign, unsigned flags)
{
     return XM(plan_many_dft)(rnk, n, 1,
			      FFTW_MPI_DEFAULT_BLOCK,
			      FFTW_MPI_DEFAULT_BLOCK,
			      in, out, comm, sign, flags);
}

X(plan) XM(plan_dft_1d)(ptrdiff_t nx, C *in, C *out,
			MPI_Comm comm, int sign, unsigned flags)
{
     return XM(plan_dft)(1, &nx, in, out, comm, sign, flags);
}

X(plan) XM(plan_dft_2d)(ptrdiff_t nx, ptrdiff_t ny, C *in, C *out,
			MPI_Comm comm, int sign, unsigned flags)
{
     ptrdiff_t n[2];
     n[0] = nx; n[1] = ny;
     return XM(plan_dft)(2, n, in, out, comm, sign, flags);
}

X(plan) XM(plan_dft_3d)(ptrdiff_t nx, ptrdiff_t ny, ptrdiff_t nz,
			C *in, C *out,
			MPI_Comm comm, int sign, unsigned flags)
{
     ptrdiff_t n[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     return XM(plan_dft)(3, n, in, out, comm, sign, flags);
}

/*************************************************************************/
/* R2R API */

X(plan) XM(plan_guru_r2r)(int rnk, const XM(ddim) *dims0,
			  ptrdiff_t howmany,
			  R *in, R *out,
			  MPI_Comm comm, const X(r2r_kind) *kind,
			  unsigned flags)
{
     int n_pes, i;
     dtensor *sz;
     rdft_kind *k;
     X(plan) pln;
     
     XM(init)();

     if (howmany < 0 || rnk < 1) return 0;
     for (i = 0; i < rnk; ++i)
	  if (dims0[i].n < 1 || dims0[i].ib < 0 || dims0[i].ob < 0)
	       return 0;

     k = X(map_r2r_kind)(rnk, kind);

     MPI_Comm_size(comm, &n_pes);
     sz = default_sz(rnk, dims0, n_pes, 0);

     if (XM(num_blocks_total)(sz, IB) > n_pes
	 || XM(num_blocks_total)(sz, OB) > n_pes) {
	  XM(dtensor_destroy)(sz);
	  return 0;
     }

     pln = X(mkapiplan)(0, flags,
			XM(mkproblem_rdft_d)(sz, howmany,
					     in, out,
					     comm, k, MPI_FLAGS(flags)));
     X(ifree0)(k);
     return pln;
}

X(plan) XM(plan_many_r2r)(int rnk, const ptrdiff_t *n,
			  ptrdiff_t howmany,
			  ptrdiff_t iblock, ptrdiff_t oblock,
			  R *in, R *out,
			  MPI_Comm comm, const X(r2r_kind) *kind,
			  unsigned flags)
{
     XM(ddim) *dims = simple_dims(rnk, n);
     X(plan) pln;

     if (rnk == 1) {
	  dims[0].ib = iblock;
	  dims[0].ob = oblock;
     }
     else if (rnk > 1) {
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_IN)].ib = iblock;
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_OUT)].ob = oblock;
     }

     pln = XM(plan_guru_r2r)(rnk,dims,howmany, in,out, comm, kind, flags);
     X(ifree)(dims);
     return pln;
}

X(plan) XM(plan_r2r)(int rnk, const ptrdiff_t *n, R *in, R *out,
		     MPI_Comm comm, 
		     const X(r2r_kind) *kind,
		     unsigned flags)
{
     return XM(plan_many_r2r)(rnk, n, 1,
			      FFTW_MPI_DEFAULT_BLOCK,
			      FFTW_MPI_DEFAULT_BLOCK,
			      in, out, comm, kind, flags);
}

X(plan) XM(plan_r2r_2d)(ptrdiff_t nx, ptrdiff_t ny, R *in, R *out,
			MPI_Comm comm,
			X(r2r_kind) kindx, X(r2r_kind) kindy,
			unsigned flags)
{
     ptrdiff_t n[2];
     X(r2r_kind) kind[2];
     n[0] = nx; n[1] = ny;
     kind[0] = kindx; kind[1] = kindy;
     return XM(plan_r2r)(2, n, in, out, comm, kind, flags);
}

X(plan) XM(plan_r2r_3d)(ptrdiff_t nx, ptrdiff_t ny, ptrdiff_t nz,
			R *in, R *out,
			MPI_Comm comm, 
			X(r2r_kind) kindx, X(r2r_kind) kindy,
			X(r2r_kind) kindz,
			unsigned flags)
{
     ptrdiff_t n[3];
     X(r2r_kind) kind[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     kind[0] = kindx; kind[1] = kindy; kind[2] = kindz;
     return XM(plan_r2r)(3, n, in, out, comm, kind, flags);
}

/*************************************************************************/
/* R2C/C2R API */

static X(plan) plan_guru_rdft2(int rnk, const XM(ddim) *dims0,
			       ptrdiff_t howmany,
			       R *r, C *c,
			       MPI_Comm comm, rdft_kind kind, unsigned flags)
{
     int n_pes, i;
     dtensor *sz;
     R *cr = (R *) c;
     
     XM(init)();

     if (howmany < 0 || rnk < 2) return 0;
     for (i = 0; i < rnk; ++i)
	  if (dims0[i].n < 1 || dims0[i].ib < 0 || dims0[i].ob < 0)
	       return 0;

     MPI_Comm_size(comm, &n_pes);
     sz = default_sz(rnk, dims0, n_pes, 1);

     sz->dims[rnk-1].n = dims0[rnk-1].n / 2 + 1;
     if (XM(num_blocks_total)(sz, IB) > n_pes
	 || XM(num_blocks_total)(sz, OB) > n_pes) {
	  XM(dtensor_destroy)(sz);
	  return 0;
     }
     sz->dims[rnk-1].n = dims0[rnk-1].n;

     if (kind == R2HC)
	  return X(mkapiplan)(0, flags,
			      XM(mkproblem_rdft2_d)(sz, howmany,
						    r, cr, comm, R2HC, 
						    MPI_FLAGS(flags)));
     else
	  return X(mkapiplan)(0, flags,
			      XM(mkproblem_rdft2_d)(sz, howmany,
						    cr, r, comm, HC2R, 
						    MPI_FLAGS(flags)));
}

X(plan) XM(plan_many_dft_r2c)(int rnk, const ptrdiff_t *n,
			  ptrdiff_t howmany,
			  ptrdiff_t iblock, ptrdiff_t oblock,
			  R *in, C *out,
			  MPI_Comm comm, unsigned flags)
{
     XM(ddim) *dims = simple_dims(rnk, n);
     X(plan) pln;

     if (rnk == 1) {
	  dims[0].ib = iblock;
	  dims[0].ob = oblock;
     }
     else if (rnk > 1) {
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_IN)].ib = iblock;
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_OUT)].ob = oblock;
     }

     pln = plan_guru_rdft2(rnk,dims,howmany, in,out, comm, R2HC, flags);
     X(ifree)(dims);
     return pln;
}

X(plan) XM(plan_many_dft_c2r)(int rnk, const ptrdiff_t *n,
			  ptrdiff_t howmany,
			  ptrdiff_t iblock, ptrdiff_t oblock,
			  C *in, R *out,
			  MPI_Comm comm, unsigned flags)
{
     XM(ddim) *dims = simple_dims(rnk, n);
     X(plan) pln;

     if (rnk == 1) {
	  dims[0].ib = iblock;
	  dims[0].ob = oblock;
     }
     else if (rnk > 1) {
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_IN)].ib = iblock;
	  dims[0 != (flags & FFTW_MPI_TRANSPOSED_OUT)].ob = oblock;
     }

     pln = plan_guru_rdft2(rnk,dims,howmany, out,in, comm, HC2R, flags);
     X(ifree)(dims);
     return pln;
}

X(plan) XM(plan_dft_r2c)(int rnk, const ptrdiff_t *n, R *in, C *out,
		     MPI_Comm comm, unsigned flags)
{
     return XM(plan_many_dft_r2c)(rnk, n, 1,
			      FFTW_MPI_DEFAULT_BLOCK,
			      FFTW_MPI_DEFAULT_BLOCK,
			      in, out, comm, flags);
}

X(plan) XM(plan_dft_r2c_2d)(ptrdiff_t nx, ptrdiff_t ny, R *in, C *out,
			MPI_Comm comm, unsigned flags)
{
     ptrdiff_t n[2];
     n[0] = nx; n[1] = ny;
     return XM(plan_dft_r2c)(2, n, in, out, comm, flags);
}

X(plan) XM(plan_dft_r2c_3d)(ptrdiff_t nx, ptrdiff_t ny, ptrdiff_t nz,
			R *in, C *out, MPI_Comm comm, unsigned flags)
{
     ptrdiff_t n[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     return XM(plan_dft_r2c)(3, n, in, out, comm, flags);
}

X(plan) XM(plan_dft_c2r)(int rnk, const ptrdiff_t *n, C *in, R *out,
		     MPI_Comm comm, unsigned flags)
{
     return XM(plan_many_dft_c2r)(rnk, n, 1,
			      FFTW_MPI_DEFAULT_BLOCK,
			      FFTW_MPI_DEFAULT_BLOCK,
			      in, out, comm, flags);
}

X(plan) XM(plan_dft_c2r_2d)(ptrdiff_t nx, ptrdiff_t ny, C *in, R *out,
			MPI_Comm comm, unsigned flags)
{
     ptrdiff_t n[2];
     n[0] = nx; n[1] = ny;
     return XM(plan_dft_c2r)(2, n, in, out, comm, flags);
}

X(plan) XM(plan_dft_c2r_3d)(ptrdiff_t nx, ptrdiff_t ny, ptrdiff_t nz,
			C *in, R *out, MPI_Comm comm, unsigned flags)
{
     ptrdiff_t n[3];
     n[0] = nx; n[1] = ny; n[2] = nz;
     return XM(plan_dft_c2r)(3, n, in, out, comm, flags);
}

/*************************************************************************/
/* New-array execute functions */

void XM(execute_dft)(const X(plan) p, C *in, C *out) {
     /* internally, MPI plans are just rdft plans */
     X(execute_r2r)(p, (R*) in, (R*) out);
}

void XM(execute_dft_r2c)(const X(plan) p, R *in, C *out) {
     /* internally, MPI plans are just rdft plans */
     X(execute_r2r)(p, in, (R*) out);
}

void XM(execute_dft_c2r)(const X(plan) p, C *in, R *out) {
     /* internally, MPI plans are just rdft plans */
     X(execute_r2r)(p, (R*) in, out);
}

void XM(execute_r2r)(const X(plan) p, R *in, R *out) {
     /* internally, MPI plans are just rdft plans */
     X(execute_r2r)(p, in, out);
}
