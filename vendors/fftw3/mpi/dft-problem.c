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

#include "mpi-dft.h"

static void destroy(problem *ego_)
{
     problem_mpi_dft *ego = (problem_mpi_dft *) ego_;
     XM(dtensor_destroy)(ego->sz);
     MPI_Comm_free(&ego->comm);
     X(ifree)(ego_);
}

static void hash(const problem *p_, md5 *m)
{
     const problem_mpi_dft *p = (const problem_mpi_dft *) p_;
     int i;
     X(md5puts)(m, "mpi-dft");
     X(md5int)(m, p->I == p->O);
     /* don't include alignment -- may differ between processes
	X(md5int)(m, X(ialignment_of)(p->I));
	X(md5int)(m, X(ialignment_of)(p->O));
	... note that applicability of MPI plans does not depend
	    on alignment (although optimality may, in principle). */
     XM(dtensor_md5)(m, p->sz);
     X(md5INT)(m, p->vn);
     X(md5int)(m, p->sign);
     X(md5int)(m, p->flags);
     MPI_Comm_size(p->comm, &i); X(md5int)(m, i);
     A(XM(md5_equal)(*m, p->comm));
}

static void print(const problem *ego_, printer *p)
{
     const problem_mpi_dft *ego = (const problem_mpi_dft *) ego_;
     int i;
     p->print(p, "(mpi-dft %d %d %d ", 
	      ego->I == ego->O,
	      X(ialignment_of)(ego->I),
	      X(ialignment_of)(ego->O));
     XM(dtensor_print)(ego->sz, p);
     p->print(p, " %D %d %d", ego->vn, ego->sign, ego->flags);
     MPI_Comm_size(ego->comm, &i); p->print(p, " %d)", i);
}

static void zero(const problem *ego_)
{
     const problem_mpi_dft *ego = (const problem_mpi_dft *) ego_;
     R *I = ego->I;
     INT i, N;
     int my_pe;

     MPI_Comm_rank(ego->comm, &my_pe);
     N = 2 * ego->vn * XM(total_block)(ego->sz, IB, my_pe);
     for (i = 0; i < N; ++i) I[i] = K(0.0);
}

static const problem_adt padt =
{
     PROBLEM_MPI_DFT,
     hash,
     zero,
     print,
     destroy
};

problem *XM(mkproblem_dft)(const dtensor *sz, INT vn,
			   R *I, R *O,
			   MPI_Comm comm,
			   int sign,
			   unsigned flags)
{
     problem_mpi_dft *ego =
          (problem_mpi_dft *)X(mkproblem)(sizeof(problem_mpi_dft), &padt);
     int n_pes;

     A(XM(dtensor_validp)(sz) && FINITE_RNK(sz->rnk));
     MPI_Comm_size(comm, &n_pes);
     A(n_pes >= XM(num_blocks_total)(sz, IB)
       && n_pes >= XM(num_blocks_total)(sz, OB));
     A(vn >= 0);
     A(sign == -1 || sign == 1);

     /* enforce pointer equality if untainted pointers are equal */
     if (UNTAINT(I) == UNTAINT(O))
	  I = O = JOIN_TAINT(I, O);

     ego->sz = XM(dtensor_canonical)(sz, 1);
     ego->vn = vn;
     ego->I = I;
     ego->O = O;
     ego->sign = sign;

     /* canonicalize: replace TRANSPOSED_IN with TRANSPOSED_OUT by
        swapping the first two dimensions (for rnk > 1) */
     if ((flags & TRANSPOSED_IN) && ego->sz->rnk > 1) {
	  ddim dim0 = ego->sz->dims[0];
	  ego->sz->dims[0] = ego->sz->dims[1];
	  ego->sz->dims[1] = dim0;
	  flags &= ~TRANSPOSED_IN;
	  flags ^= TRANSPOSED_OUT;
     }
     ego->flags = flags;

     MPI_Comm_dup(comm, &ego->comm);

     return &(ego->super);
}

problem *XM(mkproblem_dft_d)(dtensor *sz, INT vn,
			     R *I, R *O,
			     MPI_Comm comm,
			     int sign,
			     unsigned flags)
{
     problem *p = XM(mkproblem_dft)(sz, vn, I, O, comm, sign, flags);
     XM(dtensor_destroy)(sz);
     return p;
}
