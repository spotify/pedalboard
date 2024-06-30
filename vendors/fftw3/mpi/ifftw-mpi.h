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

/* FFTW-MPI internal header file */
#ifndef __IFFTW_MPI_H__
#define __IFFTW_MPI_H__

#include "kernel/ifftw.h"
#include "rdft/rdft.h"

#include <mpi.h>

/* mpi problem flags: problem-dependent meaning, but in general
   SCRAMBLED means some reordering *within* the dimensions, while
   TRANSPOSED means some reordering *of* the dimensions */
#define SCRAMBLED_IN (1 << 0)
#define SCRAMBLED_OUT (1 << 1)
#define TRANSPOSED_IN (1 << 2)
#define TRANSPOSED_OUT (1 << 3)
#define RANK1_BIGVEC_ONLY (1 << 4) /* for rank=1, allow only bigvec solver */

#define ONLY_SCRAMBLEDP(flags) (!((flags) & ~(SCRAMBLED_IN|SCRAMBLED_OUT)))
#define ONLY_TRANSPOSEDP(flags) (!((flags) & ~(TRANSPOSED_IN|TRANSPOSED_OUT)))

#if defined(FFTW_SINGLE)
#  define FFTW_MPI_TYPE MPI_FLOAT
#elif defined(FFTW_LDOUBLE)
#  define FFTW_MPI_TYPE MPI_LONG_DOUBLE
#elif defined(FFTW_QUAD)
#  error MPI quad-precision type is unknown
#else
#  define FFTW_MPI_TYPE MPI_DOUBLE
#endif

/* all fftw-mpi identifiers start with fftw_mpi (or fftwf_mpi etc.) */
#define XM(name) X(CONCAT(mpi_, name))

/***********************************************************************/
/* block distributions */

/* a distributed dimension of length n with input and output block
   sizes ib and ob, respectively. */
typedef enum { IB = 0, OB } block_kind;
typedef struct {
     INT n;
     INT b[2]; /* b[IB], b[OB] */
} ddim;

/* Loop over k in {IB, OB}.  Note: need explicit casts for C++. */
#define FORALL_BLOCK_KIND(k) for (k = IB; k <= OB; k = (block_kind) (((int) k) + 1))

/* unlike tensors in the serial FFTW, the ordering of the dtensor
   dimensions matters - both the array and the block layout are
   row-major order. */
typedef struct {
     int rnk;
#if defined(STRUCT_HACK_KR)
     ddim dims[1];
#elif defined(STRUCT_HACK_C99)
     ddim dims[];
#else
     ddim *dims;
#endif
} dtensor;


/* dtensor.c: */
dtensor *XM(mkdtensor)(int rnk);
void XM(dtensor_destroy)(dtensor *sz);
dtensor *XM(dtensor_copy)(const dtensor *sz);
dtensor *XM(dtensor_canonical)(const dtensor *sz, int compress);
int XM(dtensor_validp)(const dtensor *sz);
void XM(dtensor_md5)(md5 *p, const dtensor *t);
void XM(dtensor_print)(const dtensor *t, printer *p);

/* block.c: */

/* for a single distributed dimension: */
INT XM(num_blocks)(INT n, INT block);
int XM(num_blocks_ok)(INT n, INT block, MPI_Comm comm);
INT XM(default_block)(INT n, int n_pes);
INT XM(block)(INT n, INT block, int which_block);

/* for multiple distributed dimensions: */
INT XM(num_blocks_total)(const dtensor *sz, block_kind k);
int XM(idle_process)(const dtensor *sz, block_kind k, int which_pe);
void XM(block_coords)(const dtensor *sz, block_kind k, int which_pe, 
		     INT *coords);
INT XM(total_block)(const dtensor *sz, block_kind k, int which_pe);
int XM(is_local_after)(int dim, const dtensor *sz, block_kind k);
int XM(is_local)(const dtensor *sz, block_kind k);
int XM(is_block1d)(const dtensor *sz, block_kind k);

/* choose-radix.c */
INT XM(choose_radix)(ddim d, int n_pes, unsigned flags, int sign,
                     INT rblock[2], INT mblock[2]);

/***********************************************************************/
/* any_true.c */
int XM(any_true)(int condition, MPI_Comm comm);
int XM(md5_equal)(md5 m, MPI_Comm comm);

/* conf.c */
void XM(conf_standard)(planner *p);

/***********************************************************************/
/* rearrange.c */

/* Different ways to rearrange the vector dimension vn during transposition,
   reflecting different tradeoffs between ease of transposition and
   contiguity during the subsequent DFTs.

   TODO: can we pare this down to CONTIG and DISCONTIG, at least
   in MEASURE mode?  SQUARE_MIDDLE is also used for 1d destroy-input DFTs. */
typedef enum {
     CONTIG = 0, /* vn x 1: make subsequent DFTs contiguous */
     DISCONTIG, /* P x (vn/P) for P processes */
     SQUARE_BEFORE, /* try to get square transpose at beginning */
     SQUARE_MIDDLE, /* try to get square transpose in the middle */
     SQUARE_AFTER /* try to get square transpose at end */
} rearrangement;

/* skipping SQUARE_AFTER since it doesn't seem to offer any advantage
   over SQUARE_BEFORE */
#define FORALL_REARRANGE(rearrange) for (rearrange = CONTIG; rearrange <= SQUARE_MIDDLE; rearrange = (rearrangement) (((int) rearrange) + 1))

int XM(rearrange_applicable)(rearrangement rearrange, 
			     ddim dim0, INT vn, int n_pes);
INT XM(rearrange_ny)(rearrangement rearrange, ddim dim0, INT vn, int n_pes);

/***********************************************************************/

#endif /* __IFFTW_MPI_H__ */

