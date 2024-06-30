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

/* problem.c: */
typedef struct {
     problem super;
     dtensor *sz;
     INT vn; /* vector length (vector stride 1) */
     R *I, *O; /* contiguous interleaved arrays */

     
     unsigned flags; /* TRANSPOSED_IN/OUT meaningful for rnk>1 only
			SCRAMBLED_IN/OUT meaningful for 1d transforms only */

     MPI_Comm comm;

#if defined(STRUCT_HACK_KR)
     rdft_kind kind[1];
#elif defined(STRUCT_HACK_C99)
     rdft_kind kind[];
#else
     rdft_kind *kind;
#endif
} problem_mpi_rdft;

problem *XM(mkproblem_rdft)(const dtensor *sz, INT vn,
			    R *I, R *O, MPI_Comm comm, 
			    const rdft_kind *kind, unsigned flags);
problem *XM(mkproblem_rdft_d)(dtensor *sz, INT vn,
			      R *I, R *O, MPI_Comm comm, 
			      const rdft_kind *kind, unsigned flags);

/* solve.c: */
void XM(rdft_solve)(const plan *ego_, const problem *p_);

/* plans have same operands as rdft plans, so just re-use */
typedef plan_rdft plan_mpi_rdft;
#define MKPLAN_MPI_RDFT(type, adt, apply) \
  (type *)X(mkplan_rdft)(sizeof(type), adt, apply)

int XM(rdft_serial_applicable)(const problem_mpi_rdft *p);

/* various solvers */
void XM(rdft_rank_geq2_register)(planner *p);
void XM(rdft_rank_geq2_transposed_register)(planner *p);
void XM(rdft_serial_register)(planner *p);
void XM(rdft_rank1_bigvec_register)(planner *p);
