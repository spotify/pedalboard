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

     int sign; /* FFTW_FORWARD / FFTW_BACKWARD */
     unsigned flags; /* TRANSPOSED_IN/OUT meaningful for rnk>1 only
			SCRAMBLED_IN/OUT meaningful for 1d transforms only */

     MPI_Comm comm;
} problem_mpi_dft;

problem *XM(mkproblem_dft)(const dtensor *sz, INT vn,
			      R *I, R *O, MPI_Comm comm,
			      int sign, unsigned flags);
problem *XM(mkproblem_dft_d)(dtensor *sz, INT vn,
			     R *I, R *O, MPI_Comm comm,
			     int sign, unsigned flags);

/* solve.c: */
void XM(dft_solve)(const plan *ego_, const problem *p_);

/* plans have same operands as rdft plans, so just re-use */
typedef plan_rdft plan_mpi_dft;
#define MKPLAN_MPI_DFT(type, adt, apply) \
  (type *)X(mkplan_rdft)(sizeof(type), adt, apply)

int XM(dft_serial_applicable)(const problem_mpi_dft *p);

/* various solvers */
void XM(dft_rank_geq2_register)(planner *p);
void XM(dft_rank_geq2_transposed_register)(planner *p);
void XM(dft_serial_register)(planner *p);
void XM(dft_rank1_bigvec_register)(planner *p);
void XM(dft_rank1_register)(planner *p);
