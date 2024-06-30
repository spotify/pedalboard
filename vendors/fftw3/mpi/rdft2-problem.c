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

#include "mpi-rdft2.h"

static void destroy(problem *ego_)
{
     problem_mpi_rdft2 *ego = (problem_mpi_rdft2 *) ego_;
     XM(dtensor_destroy)(ego->sz);
     MPI_Comm_free(&ego->comm);
     X(ifree)(ego_);
}

static void hash(const problem *p_, md5 *m)
{
     const problem_mpi_rdft2 *p = (const problem_mpi_rdft2 *) p_;
     int i;
     X(md5puts)(m, "mpi-rdft2");
     X(md5int)(m, p->I == p->O);
     /* don't include alignment -- may differ between processes
	X(md5int)(m, X(ialignment_of)(p->I));
	X(md5int)(m, X(ialignment_of)(p->O));
	... note that applicability of MPI plans does not depend
	    on alignment (although optimality may, in principle). */
     XM(dtensor_md5)(m, p->sz);
     X(md5INT)(m, p->vn);
     X(md5int)(m, p->kind);
     X(md5int)(m, p->flags);
     MPI_Comm_size(p->comm, &i); X(md5int)(m, i);
     A(XM(md5_equal)(*m, p->comm));
}

static void print(const problem *ego_, printer *p)
{
     const problem_mpi_rdft2 *ego = (const problem_mpi_rdft2 *) ego_;
     int i;
     p->print(p, "(mpi-rdft2 %d %d %d ", 
	      ego->I == ego->O,
	      X(ialignment_of)(ego->I),
	      X(ialignment_of)(ego->O));
     XM(dtensor_print)(ego->sz, p);
     p->print(p, " %D %d %d", ego->vn, (int) ego->kind, ego->flags);
     MPI_Comm_size(ego->comm, &i); p->print(p, " %d)", i);
}

static void zero(const problem *ego_)
{
     const problem_mpi_rdft2 *ego = (const problem_mpi_rdft2 *) ego_;
     R *I = ego->I;
     dtensor *sz;
     INT i, N;
     int my_pe;

     sz = XM(dtensor_copy)(ego->sz);
     sz->dims[sz->rnk - 1].n = sz->dims[sz->rnk - 1].n / 2 + 1;
     MPI_Comm_rank(ego->comm, &my_pe);
     N = 2 * ego->vn * XM(total_block)(sz, IB, my_pe);
     XM(dtensor_destroy)(sz);
     for (i = 0; i < N; ++i) I[i] = K(0.0);
}

static const problem_adt padt =
{
     PROBLEM_MPI_RDFT2,
     hash,
     zero,
     print,
     destroy
};

problem *XM(mkproblem_rdft2)(const dtensor *sz, INT vn,
			   R *I, R *O,
			   MPI_Comm comm,
			   rdft_kind kind,
			   unsigned flags)
{
     problem_mpi_rdft2 *ego =
          (problem_mpi_rdft2 *)X(mkproblem)(sizeof(problem_mpi_rdft2), &padt);
     int n_pes;

     A(XM(dtensor_validp)(sz) && FINITE_RNK(sz->rnk) && sz->rnk > 1);
     MPI_Comm_size(comm, &n_pes);
     A(vn >= 0);
     A(kind == R2HC || kind == HC2R);

     /* enforce pointer equality if untainted pointers are equal */
     if (UNTAINT(I) == UNTAINT(O))
	  I = O = JOIN_TAINT(I, O);

     ego->sz = XM(dtensor_canonical)(sz, 0);
#ifdef FFTW_DEBUG
     ego->sz->dims[sz->rnk - 1].n = sz->dims[sz->rnk - 1].n / 2 + 1;
     A(n_pes >= XM(num_blocks_total)(ego->sz, IB)
       && n_pes >= XM(num_blocks_total)(ego->sz, OB));
     ego->sz->dims[sz->rnk - 1].n = sz->dims[sz->rnk - 1].n;
#endif

     ego->vn = vn;
     ego->I = I;
     ego->O = O;
     ego->kind = kind;

     /* We only support TRANSPOSED_OUT for r2c and TRANSPOSED_IN for
	c2r transforms. */

     ego->flags = flags;

     MPI_Comm_dup(comm, &ego->comm);

     return &(ego->super);
}

problem *XM(mkproblem_rdft2_d)(dtensor *sz, INT vn,
			     R *I, R *O,
			     MPI_Comm comm,
			     rdft_kind kind,
			     unsigned flags)
{
     problem *p = XM(mkproblem_rdft2)(sz, vn, I, O, comm, kind, flags);
     XM(dtensor_destroy)(sz);
     return p;
}
