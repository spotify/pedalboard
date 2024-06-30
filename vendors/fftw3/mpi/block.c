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

INT XM(num_blocks)(INT n, INT block)
{
     return (n + block - 1) / block;
}

int XM(num_blocks_ok)(INT n, INT block, MPI_Comm comm)
{
     int n_pes;
     MPI_Comm_size(comm, &n_pes);
     return n_pes >= XM(num_blocks)(n, block);
}

/* Pick a default block size for dividing a problem of size n among
   n_pes processes.  Divide as equally as possible, while minimizing
   the maximum block size among the processes as well as the number of
   processes with nonzero blocks. */
INT XM(default_block)(INT n, int n_pes)
{
     return ((n + n_pes - 1) / n_pes);
}

/* For a given block size and dimension n, compute the block size 
   on the given process. */
INT XM(block)(INT n, INT block, int which_block)
{
     INT d = n - which_block * block;
     return d <= 0 ? 0 : (d > block ? block : d);
}

static INT num_blocks_kind(const ddim *dim, block_kind k)
{
     return XM(num_blocks)(dim->n, dim->b[k]);
}

INT XM(num_blocks_total)(const dtensor *sz, block_kind k)
{
     if (FINITE_RNK(sz->rnk)) {
	  int i;
	  INT ntot = 1;
	  for (i = 0; i < sz->rnk; ++i)
	       ntot *= num_blocks_kind(sz->dims + i, k);
	  return ntot;
     }
     else
	  return 0;
}

int XM(idle_process)(const dtensor *sz, block_kind k, int which_pe)
{
     return (which_pe >= XM(num_blocks_total)(sz, k));
}

/* Given a non-idle process which_pe, computes the coordinate
   vector coords[rnk] giving the coordinates of a block in the
   matrix of blocks.  k specifies whether we are talking about
   the input or output data distribution. */
void XM(block_coords)(const dtensor *sz, block_kind k, int which_pe, 
		     INT *coords)
{
     int i;
     A(!XM(idle_process)(sz, k, which_pe) && FINITE_RNK(sz->rnk));
     for (i = sz->rnk - 1; i >= 0; --i) {
	  INT nb = num_blocks_kind(sz->dims + i, k);
	  coords[i] = which_pe % nb;
	  which_pe /= nb;
     }
}

INT XM(total_block)(const dtensor *sz, block_kind k, int which_pe)
{
     if (XM(idle_process)(sz, k, which_pe))
	  return 0;
     else {
	  int i;
	  INT N = 1, *coords;
	  STACK_MALLOC(INT*, coords, sizeof(INT) * sz->rnk);
	  XM(block_coords)(sz, k, which_pe, coords);
	  for (i = 0; i < sz->rnk; ++i)
	       N *= XM(block)(sz->dims[i].n, sz->dims[i].b[k], coords[i]);
	  STACK_FREE(coords);
	  return N;
     }
}

/* returns whether sz is local for dims >= dim */
int XM(is_local_after)(int dim, const dtensor *sz, block_kind k)
{
     if (FINITE_RNK(sz->rnk))
	  for (; dim < sz->rnk; ++dim)
	       if (XM(num_blocks)(sz->dims[dim].n, sz->dims[dim].b[k]) > 1)
		    return 0;
     return 1;
}

int XM(is_local)(const dtensor *sz, block_kind k)
{
     return XM(is_local_after)(0, sz, k);
}

/* Return whether sz is distributed for k according to a simple
   1d block distribution in the first or second dimensions */
int XM(is_block1d)(const dtensor *sz, block_kind k)
{
     int i;
     if (!FINITE_RNK(sz->rnk)) return 0;
     for (i = 0; i < sz->rnk && num_blocks_kind(sz->dims + i, k) == 1; ++i) ;
     return(i < sz->rnk && i < 2 && XM(is_local_after)(i + 1, sz, k));

}
