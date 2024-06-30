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


#ifndef __DFT_H__
#define __DFT_H__

#include "kernel/ifftw.h"
#include "dft/codelet-dft.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* problem.c: */
typedef struct {
     problem super;
     tensor *sz, *vecsz;
     R *ri, *ii, *ro, *io;
} problem_dft;

void X(dft_zerotens)(tensor *sz, R *ri, R *ii);
problem *X(mkproblem_dft)(const tensor *sz, const tensor *vecsz,
				R *ri, R *ii, R *ro, R *io);
problem *X(mkproblem_dft_d)(tensor *sz, tensor *vecsz,
			    R *ri, R *ii, R *ro, R *io);

/* solve.c: */
void X(dft_solve)(const plan *ego_, const problem *p_);

/* plan.c: */
typedef void (*dftapply) (const plan *ego, R *ri, R *ii, R *ro, R *io);

typedef struct {
     plan super;
     dftapply apply;
} plan_dft;

plan *X(mkplan_dft)(size_t size, const plan_adt *adt, dftapply apply);

#define MKPLAN_DFT(type, adt, apply) \
  (type *)X(mkplan_dft)(sizeof(type), adt, apply)

/* various solvers */
solver *X(mksolver_dft_direct)(kdft k, const kdft_desc *desc);
solver *X(mksolver_dft_directbuf)(kdft k, const kdft_desc *desc);

void X(dft_rank0_register)(planner *p);
void X(dft_rank_geq2_register)(planner *p);
void X(dft_indirect_register)(planner *p);
void X(dft_indirect_transpose_register)(planner *p);
void X(dft_vrank_geq1_register)(planner *p);
void X(dft_vrank2_transpose_register)(planner *p);
void X(dft_vrank3_transpose_register)(planner *p);
void X(dft_buffered_register)(planner *p);
void X(dft_generic_register)(planner *p);
void X(dft_rader_register)(planner *p);
void X(dft_bluestein_register)(planner *p);
void X(dft_nop_register)(planner *p);
void X(ct_generic_register)(planner *p);
void X(ct_genericbuf_register)(planner *p);

/* configurations */
void X(dft_conf_standard)(planner *p);

#ifdef __cplusplus
}  /* extern "C" */
#endif /* __cplusplus */

#endif /* __DFT_H__ */
