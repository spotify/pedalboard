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

#include "api/api.h"
#include <math.h>

/* a flag operation: x is either a flag, in which case xm == 0, or
   a mask, in which case xm == x; using this we can compactly code
   the various bit operations via (flags & x) ^ xm or (flags | x) ^ xm. */
typedef struct {
     unsigned x, xm;
} flagmask;

typedef struct {
     flagmask flag;
     flagmask op;
} flagop;

#define FLAGP(f, msk)(((f) & (msk).x) ^ (msk).xm)
#define OP(f, msk)(((f) | (msk).x) ^ (msk).xm)

#define YES(x) {x, 0}
#define NO(x) {x, x}
#define IMPLIES(predicate, consequence) { predicate, consequence }
#define EQV(a, b) IMPLIES(YES(a), YES(b)), IMPLIES(NO(a), NO(b))
#define NEQV(a, b) IMPLIES(YES(a), NO(b)), IMPLIES(NO(a), YES(b))

static void map_flags(unsigned *iflags, unsigned *oflags,
		      const flagop flagmap[], size_t nmap)
{
     size_t i;
     for (i = 0; i < nmap; ++i)
          if (FLAGP(*iflags, flagmap[i].flag))
               *oflags = OP(*oflags, flagmap[i].op);
}

/* encoding of the planner timelimit into a BITS_FOR_TIMELIMIT-bits
   nonnegative integer, such that we can still view the integer as
   ``impatience'': higher means *lower* time limit, and 0 is the
   highest possible value (about 1 year of calendar time) */
static unsigned timelimit_to_flags(double timelimit)
{
     const double tmax = 365 * 24 * 3600;
     const double tstep = 1.05;
     const int nsteps = (1 << BITS_FOR_TIMELIMIT);
     int x;
     
     if (timelimit < 0 || timelimit >= tmax)
	  return 0;
     if (timelimit <= 1.0e-10)
	  return nsteps - 1;
     
     x = (int) (0.5 + (log(tmax / timelimit) / log(tstep)));

     if (x < 0) x = 0;
     if (x >= nsteps) x = nsteps - 1;
     return x;
}

void X(mapflags)(planner *plnr, unsigned flags)
{
     unsigned l, u, t;

     /* map of api flags -> api flags, to implement consistency rules
        and combination flags */
     const flagop self_flagmap[] = {
	  /* in some cases (notably for halfcomplex->real transforms),
	     DESTROY_INPUT is the default, so we need to support
	     an inverse flag to disable it.

	     (PRESERVE, DESTROY)   ->   (PRESERVE, DESTROY)
               (0, 0)                       (1, 0)
               (0, 1)                       (0, 1)
               (1, 0)                       (1, 0)
               (1, 1)                       (1, 0)
	  */
	  IMPLIES(YES(FFTW_PRESERVE_INPUT), NO(FFTW_DESTROY_INPUT)),
	  IMPLIES(NO(FFTW_DESTROY_INPUT), YES(FFTW_PRESERVE_INPUT)),

	  IMPLIES(YES(FFTW_EXHAUSTIVE), YES(FFTW_PATIENT)),

	  IMPLIES(YES(FFTW_ESTIMATE), NO(FFTW_PATIENT)),
	  IMPLIES(YES(FFTW_ESTIMATE),
		  YES(FFTW_ESTIMATE_PATIENT 
		      | FFTW_NO_INDIRECT_OP
		      | FFTW_ALLOW_PRUNING)),

	  IMPLIES(NO(FFTW_EXHAUSTIVE), 
		  YES(FFTW_NO_SLOW)),

	  /* a canonical set of fftw2-like impatience flags */
	  IMPLIES(NO(FFTW_PATIENT),
		  YES(FFTW_NO_VRECURSE
		      | FFTW_NO_RANK_SPLITS
		      | FFTW_NO_VRANK_SPLITS
		      | FFTW_NO_NONTHREADED
		      | FFTW_NO_DFT_R2HC
		      | FFTW_NO_FIXED_RADIX_LARGE_N
		      | FFTW_BELIEVE_PCOST))
     };

     /* map of (processed) api flags to internal problem/planner flags */
     const flagop l_flagmap[] = {
	  EQV(FFTW_PRESERVE_INPUT, NO_DESTROY_INPUT),
	  EQV(FFTW_NO_SIMD, NO_SIMD),
	  EQV(FFTW_CONSERVE_MEMORY, CONSERVE_MEMORY),
	  EQV(FFTW_NO_BUFFERING, NO_BUFFERING),
	  NEQV(FFTW_ALLOW_LARGE_GENERIC, NO_LARGE_GENERIC)
     };

     const flagop u_flagmap[] = {
	  IMPLIES(YES(FFTW_EXHAUSTIVE), NO(0xFFFFFFFF)),
	  IMPLIES(NO(FFTW_EXHAUSTIVE), YES(NO_UGLY)),

	  /* the following are undocumented, "beyond-guru" flags that
	     require some understanding of FFTW internals */
	  EQV(FFTW_ESTIMATE_PATIENT, ESTIMATE),
	  EQV(FFTW_ALLOW_PRUNING, ALLOW_PRUNING),
	  EQV(FFTW_BELIEVE_PCOST, BELIEVE_PCOST),
	  EQV(FFTW_NO_DFT_R2HC, NO_DFT_R2HC),
	  EQV(FFTW_NO_NONTHREADED, NO_NONTHREADED),
	  EQV(FFTW_NO_INDIRECT_OP, NO_INDIRECT_OP),
	  EQV(FFTW_NO_RANK_SPLITS, NO_RANK_SPLITS),
	  EQV(FFTW_NO_VRANK_SPLITS, NO_VRANK_SPLITS),
	  EQV(FFTW_NO_VRECURSE, NO_VRECURSE),
	  EQV(FFTW_NO_SLOW, NO_SLOW),
	  EQV(FFTW_NO_FIXED_RADIX_LARGE_N, NO_FIXED_RADIX_LARGE_N)
     };

     map_flags(&flags, &flags, self_flagmap, NELEM(self_flagmap));

     l = u = 0;
     map_flags(&flags, &l, l_flagmap, NELEM(l_flagmap));
     map_flags(&flags, &u, u_flagmap, NELEM(u_flagmap));

     /* enforce l <= u  */
     PLNR_L(plnr) = l;
     PLNR_U(plnr) = u | l;

     /* assert that the conversion didn't lose bits */
     A(PLNR_L(plnr) == l);
     A(PLNR_U(plnr) == (u | l));

     /* compute flags representation of the timelimit */
     t = timelimit_to_flags(plnr->timelimit);

     PLNR_TIMELIMIT_IMPATIENCE(plnr) = t;
     A(PLNR_TIMELIMIT_IMPATIENCE(plnr) == t);
}
