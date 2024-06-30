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

/* Return the radix r for a 1d MPI transform of a distributed dimension d,
   with the given flags and transform size.   That is, decomposes d.n
   as r * m, Cooley-Tukey style.  Also computes the block sizes rblock
   and mblock.  Returns 0 if such a decomposition is not feasible.
   This is unfortunately somewhat complicated.

   A distributed Cooley-Tukey algorithm works as follows (see dft-rank1.c):

   d.n is initially distributed as an m x r array with block size mblock[IB].
   Then it is internally transposed to an r x m array with block size
   rblock[IB].  Then it is internally transposed to m x r again with block
   size mblock[OB].  Finally, it is transposed to r x m with block size
   rblock[IB].

   If flags & SCRAMBLED_IN, then the first transpose is skipped (the array
   starts out as r x m).  If flags & SCRAMBLED_OUT, then the last transpose
   is skipped (the array ends up as m x r).  To make sure the forward
   and backward transforms use the same "scrambling" format, we swap r
   and m when sign != FFT_SIGN.

   There are some downsides to this, especially in the case where
   either m or r is not divisible by n_pes.  For one thing, it means
   that in general we can't use the same block size for the input and
   output.  For another thing, it means that we can't in general honor
   a user's "requested" block sizes in d.b[].  Therefore, for simplicity,
   we simply ignore d.b[] for now.
*/
INT XM(choose_radix)(ddim d, int n_pes, unsigned flags, int sign,
		     INT rblock[2], INT mblock[2])
{
     INT r, m;

     UNUSED(flags); /* we would need this if we paid attention to d.b[*] */

     /* If n_pes is a factor of d.n, then choose r to be d.n / n_pes.
        This not only ensures that the input (the m dimension) is
        equally distributed if possible, and at the r dimension is
        maximally equally distributed (if d.n/n_pes >= n_pes), it also
        makes one of the local transpositions in the algorithm
        trivial. */
     if (d.n % n_pes == 0 /* it's good if n_pes divides d.n ...*/
	 && d.n / n_pes >= n_pes /* .. unless we can't use n_pes processes */)
	  r = d.n / n_pes;
     else {  /* n_pes does not divide d.n, pick a factor close to sqrt(d.n) */
	  for (r = X(isqrt)(d.n); d.n % r != 0; ++r)
	       ;
     }
     if (r == 1 || r == d.n) return 0; /* punt if we can't reduce size */

     if (sign != FFT_SIGN) { /* swap {m,r} so that scrambling is reversible */
	  m = r;
	  r = d.n / m;
     }
     else
	  m = d.n / r;

     rblock[IB] = rblock[OB] = XM(default_block)(r, n_pes);
     mblock[IB] = mblock[OB] = XM(default_block)(m, n_pes);

     return r;
}
