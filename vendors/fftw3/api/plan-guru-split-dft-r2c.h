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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include "api/api.h"
#include "rdft/rdft.h"

X(plan)
XGURU(split_dft_r2c)(int rank, const IODIM *dims, int howmany_rank,
                     const IODIM *howmany_dims, R *in, R *ro, R *io,
                     unsigned flags) {
  if (!GURU_KOSHERP(rank, dims, howmany_rank, howmany_dims))
    return 0;

  return X(mkapiplan)(0, flags,
                      X(mkproblem_rdft2_d_3pointers)(
                          MKTENSOR_IODIMS(rank, dims, 1, 1),
                          MKTENSOR_IODIMS(howmany_rank, howmany_dims, 1, 1),
                          TAINT_UNALIGNED(in, flags),
                          TAINT_UNALIGNED(ro, flags),
                          TAINT_UNALIGNED(io, flags), R2HC));
}
