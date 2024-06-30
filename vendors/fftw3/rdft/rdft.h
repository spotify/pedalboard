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

#ifndef __RDFT_H__
#define __RDFT_H__

#include "kernel/ifftw.h"
#include "rdft/codelet-rdft.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* problem.c: */
typedef struct {
     problem super;
     tensor *sz, *vecsz;
     R *I, *O;
#if defined(STRUCT_HACK_KR)
     rdft_kind kind[1];
#elif defined(STRUCT_HACK_C99)
     rdft_kind kind[];
#else
     rdft_kind *kind;
#endif
} problem_rdft;

void X(rdft_zerotens)(tensor *sz, R *I);
problem *X(mkproblem_rdft)(const tensor *sz, const tensor *vecsz,
			   R *I, R *O, const rdft_kind *kind);
problem *X(mkproblem_rdft_d)(tensor *sz, tensor *vecsz,
			     R *I, R *O, const rdft_kind *kind);
problem *X(mkproblem_rdft_0_d)(tensor *vecsz, R *I, R *O);
problem *X(mkproblem_rdft_1)(const tensor *sz, const tensor *vecsz,
			     R *I, R *O, rdft_kind kind);
problem *X(mkproblem_rdft_1_d)(tensor *sz, tensor *vecsz,
			       R *I, R *O, rdft_kind kind);

const char *X(rdft_kind_str)(rdft_kind kind);

/* solve.c: */
void X(rdft_solve)(const plan *ego_, const problem *p_);

/* plan.c: */
typedef void (*rdftapply) (const plan *ego, R *I, R *O);

typedef struct {
     plan super;
     rdftapply apply;
} plan_rdft;

plan *X(mkplan_rdft)(size_t size, const plan_adt *adt, rdftapply apply);

#define MKPLAN_RDFT(type, adt, apply) \
  (type *)X(mkplan_rdft)(sizeof(type), adt, apply)

/* various solvers */

solver *X(mksolver_rdft_r2c_direct)(kr2c k, const kr2c_desc *desc);
solver *X(mksolver_rdft_r2c_directbuf)(kr2c k, const kr2c_desc *desc);
solver *X(mksolver_rdft_r2r_direct)(kr2r k, const kr2r_desc *desc);

void X(rdft_rank0_register)(planner *p);
void X(rdft_vrank3_transpose_register)(planner *p);
void X(rdft_rank_geq2_register)(planner *p);
void X(rdft_indirect_register)(planner *p);
void X(rdft_vrank_geq1_register)(planner *p);
void X(rdft_buffered_register)(planner *p);
void X(rdft_generic_register)(planner *p);
void X(rdft_rader_hc2hc_register)(planner *p);
void X(rdft_dht_register)(planner *p);
void X(dht_r2hc_register)(planner *p);
void X(dht_rader_register)(planner *p);
void X(dft_r2hc_register)(planner *p);
void X(rdft_nop_register)(planner *p);
void X(hc2hc_generic_register)(planner *p);

/****************************************************************************/
/* problem2.c: */
/* 
   An RDFT2 problem transforms a 1d real array r[n] with stride is/os
   to/from an "unpacked" complex array {rio,iio}[n/2 + 1] with stride
   os/is.  R0 points to the first even element of the real array.  
   R1 points to the first odd element of the real array.

   Strides on the real side of the transform express distances
   between consecutive elements of the same array (even or odd).
   E.g., for a contiguous input

     R0 R1 R2 R3 ...

   the input stride would be 2, not 1.  This convention is necessary
   for hc2c codelets to work, since they transpose even/odd with
   real/imag.
   
   Multidimensional transforms use complex DFTs for the
   noncontiguous dimensions.  vecsz has the usual interpretation.  
*/
typedef struct {
     problem super;
     tensor *sz;
     tensor *vecsz;
     R *r0, *r1;
     R *cr, *ci;
     rdft_kind kind; /* assert(kind < DHT) */
} problem_rdft2;

problem *X(mkproblem_rdft2)(const tensor *sz, const tensor *vecsz,
			    R *r0, R *r1, R *cr, R *ci, rdft_kind kind);
problem *X(mkproblem_rdft2_d)(tensor *sz, tensor *vecsz,
			      R *r0, R *r1, R *cr, R *ci, rdft_kind kind);
problem *X(mkproblem_rdft2_d_3pointers)(tensor *sz, tensor *vecsz,
					R *r, R *cr, R *ci, rdft_kind kind);
int X(rdft2_inplace_strides)(const problem_rdft2 *p, int vdim);
INT X(rdft2_tensor_max_index)(const tensor *sz, rdft_kind k);
void X(rdft2_strides)(rdft_kind kind, const iodim *d, INT *rs, INT *cs);
INT X(rdft2_complex_n)(INT real_n, rdft_kind kind);

/* verify.c: */
void X(rdft2_verify)(plan *pln, const problem_rdft2 *p, int rounds);

/* solve.c: */
void X(rdft2_solve)(const plan *ego_, const problem *p_);

/* plan.c: */
typedef void (*rdft2apply) (const plan *ego, R *r0, R *r1, R *cr, R *ci);

typedef struct {
     plan super;
     rdft2apply apply;
} plan_rdft2;

plan *X(mkplan_rdft2)(size_t size, const plan_adt *adt, rdft2apply apply);

#define MKPLAN_RDFT2(type, adt, apply) \
  (type *)X(mkplan_rdft2)(sizeof(type), adt, apply)

/* various solvers */

solver *X(mksolver_rdft2_direct)(kr2c k, const kr2c_desc *desc);

void X(rdft2_vrank_geq1_register)(planner *p);
void X(rdft2_buffered_register)(planner *p);
void X(rdft2_rdft_register)(planner *p);
void X(rdft2_nop_register)(planner *p);
void X(rdft2_rank0_register)(planner *p);
void X(rdft2_rank_geq2_register)(planner *p);

/****************************************************************************/

/* configurations */
void X(rdft_conf_standard)(planner *p);

#ifdef __cplusplus
}  /* extern "C" */
#endif /* __cplusplus */

#endif /* __RDFT_H__ */
