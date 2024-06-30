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

/* routines shared by the various buffered solvers */

#include "kernel/ifftw.h"

#define DEFAULT_MAXNBUF ((INT)256)

/* approx. 512KB of buffers for complex data */
#define MAXBUFSZ (256 * 1024 / (INT)(sizeof(R)))

INT X(nbuf)(INT n, INT vl, INT maxnbuf)
{
     INT i, nbuf, lb; 

     if (!maxnbuf) 
	  maxnbuf = DEFAULT_MAXNBUF;

     nbuf = X(imin)(maxnbuf,
		    X(imin)(vl, X(imax)((INT)1, MAXBUFSZ / n)));

     /*
      * Look for a buffer number (not too small) that divides the
      * vector length, in order that we only need one child plan:
      */
     lb = X(imax)(1, nbuf / 4);
     for (i = nbuf; i >= lb; --i)
          if (vl % i == 0)
               return i;

     /* whatever... */
     return nbuf;
}

#define SKEW 6 /* need to be even for SIMD */
#define SKEWMOD 8 

INT X(bufdist)(INT n, INT vl)
{
     if (vl == 1)
	  return n;
     else 
	  /* return smallest X such that X >= N and X == SKEW (mod SKEWMOD) */
	  return n + X(modulo)(SKEW - n, SKEWMOD);
}

int X(toobig)(INT n)
{
     return n > MAXBUFSZ;
}

/* TRUE if there exists i < which such that maxnbuf[i] and
   maxnbuf[which] yield the same value, in which case we canonicalize
   on the minimum value */
int X(nbuf_redundant)(INT n, INT vl, size_t which, 
		      const INT *maxnbuf, size_t nmaxnbuf)
{
     size_t i;
     (void)nmaxnbuf; /* UNUSED */
     for (i = 0; i < which; ++i)
	  if (X(nbuf)(n, vl, maxnbuf[i]) == X(nbuf)(n, vl, maxnbuf[which]))
	       return 1;
     return 0;
}
