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

#include "ifftw-mpi.h"

/* tproblem.c: */
typedef struct {
     problem super;
     INT vn; /* vector length (vector stride 1) */
     INT nx, ny; /* nx x ny transposed to ny x nx */
     R *I, *O; /* contiguous real arrays (both same size!) */

     unsigned flags; /* TRANSPOSED_IN: input is *locally* transposed
			TRANSPOSED_OUT: output is *locally* transposed */

     INT block, tblock; /* block size, slab decomposition;
			   tblock is for transposed blocks on output */

     MPI_Comm comm;
} problem_mpi_transpose;

problem *XM(mkproblem_transpose)(INT nx, INT ny, INT vn,
				 R *I, R *O,
				 INT block, INT tblock,
				 MPI_Comm comm,
				 unsigned flags);

/* tsolve.c: */
void XM(transpose_solve)(const plan *ego_, const problem *p_);

/* plans have same operands as rdft plans, so just re-use */
typedef plan_rdft plan_mpi_transpose;
#define MKPLAN_MPI_TRANSPOSE(type, adt, apply) \
  (type *)X(mkplan_rdft)(sizeof(type), adt, apply)

/* transpose-pairwise.c: */
int XM(mkplans_posttranspose)(const problem_mpi_transpose *p, planner *plnr,
			      R *I, R *O, int my_pe,
			      plan **cld2, plan **cld2rest, plan **cld3,
			      INT *rest_Ioff, INT *rest_Ooff);
/* various solvers */
void XM(transpose_pairwise_register)(planner *p);
void XM(transpose_alltoall_register)(planner *p);
void XM(transpose_recurse_register)(planner *p);
