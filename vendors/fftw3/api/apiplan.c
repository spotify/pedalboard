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

static planner_hook_t before_planner_hook = 0, after_planner_hook = 0;

void X(set_planner_hooks)(planner_hook_t before, planner_hook_t after)
{
     before_planner_hook = before;
     after_planner_hook = after;
}

static plan *mkplan0(planner *plnr, unsigned flags,
		     const problem *prb, unsigned hash_info,
		     wisdom_state_t wisdom_state)
{
     /* map API flags into FFTW flags */
     X(mapflags)(plnr, flags);

     plnr->flags.hash_info = hash_info;
     plnr->wisdom_state = wisdom_state;

     /* create plan */
     return plnr->adt->mkplan(plnr, prb);
}

static unsigned force_estimator(unsigned flags)
{
     flags &= ~(FFTW_MEASURE | FFTW_PATIENT | FFTW_EXHAUSTIVE);
     return (flags | FFTW_ESTIMATE);
}

static plan *mkplan(planner *plnr, unsigned flags,
		    const problem *prb, unsigned hash_info)
{
     plan *pln;
     
     pln = mkplan0(plnr, flags, prb, hash_info, WISDOM_NORMAL);

     if (plnr->wisdom_state == WISDOM_NORMAL && !pln) {
	  /* maybe the planner failed because of inconsistent wisdom;
	     plan again ignoring infeasible wisdom */
	  pln = mkplan0(plnr, force_estimator(flags), prb,
			hash_info, WISDOM_IGNORE_INFEASIBLE);
     }

     if (plnr->wisdom_state == WISDOM_IS_BOGUS) {
	  /* if the planner detected a wisdom inconsistency,
	     forget all wisdom and plan again */
	  plnr->adt->forget(plnr, FORGET_EVERYTHING);

	  A(!pln);
	  pln = mkplan0(plnr, flags, prb, hash_info, WISDOM_NORMAL);

	  if (plnr->wisdom_state == WISDOM_IS_BOGUS) {
	       /* if it still fails, plan without wisdom */
	       plnr->adt->forget(plnr, FORGET_EVERYTHING);

	       A(!pln);
	       pln = mkplan0(plnr, force_estimator(flags),
			     prb, hash_info, WISDOM_IGNORE_ALL);
	  }
     }

     return pln;
}

apiplan *X(mkapiplan)(int sign, unsigned flags, problem *prb)
{
     apiplan *p = 0;
     plan *pln;
     unsigned flags_used_for_planning;
     planner *plnr;
     static const unsigned int pats[] = {FFTW_ESTIMATE, FFTW_MEASURE,
                                         FFTW_PATIENT, FFTW_EXHAUSTIVE};
     int pat, pat_max;
     double pcost = 0;
     
     if (before_planner_hook)
          before_planner_hook();
     
     plnr = X(the_planner)();

     if (flags & FFTW_WISDOM_ONLY) {
	  /* Special mode that returns a plan only if wisdom is present,
	     and returns 0 otherwise.  This is now documented in the manual,
	     as a way to detect whether wisdom is available for a problem. */
	  flags_used_for_planning = flags;
	  pln = mkplan0(plnr, flags, prb, 0, WISDOM_ONLY);
     } else {
	  pat_max = flags & FFTW_ESTIMATE ? 0 :
	       (flags & FFTW_EXHAUSTIVE ? 3 :
		(flags & FFTW_PATIENT ? 2 : 1));
	  pat = plnr->timelimit >= 0 ? 0 : pat_max;

	  flags &= ~(FFTW_ESTIMATE | FFTW_MEASURE |
		     FFTW_PATIENT | FFTW_EXHAUSTIVE);

	  plnr->start_time = X(get_crude_time)();

	  /* plan at incrementally increasing patience until we run
	     out of time */
	  for (pln = 0, flags_used_for_planning = 0; pat <= pat_max; ++pat) {
	       plan *pln1;
	       unsigned tmpflags = flags | pats[pat];
	       pln1 = mkplan(plnr, tmpflags, prb, 0u);

	       if (!pln1) {
		    /* don't bother continuing if planner failed or timed out */
		    A(!pln || plnr->timed_out);
		    break;
	       }

	       X(plan_destroy_internal)(pln);
	       pln = pln1;
	       flags_used_for_planning = tmpflags;
	       pcost = pln->pcost;
	  }
     }

     if (pln) {
	  /* build apiplan */
	  p = (apiplan *) MALLOC(sizeof(apiplan), PLANS);
	  p->prb = prb;
	  p->sign = sign; /* cache for execute_dft */

	  /* re-create plan from wisdom, adding blessing */
	  p->pln = mkplan(plnr, flags_used_for_planning, prb, BLESSING);

	  /* record pcost from most recent measurement for use in X(cost) */
	  p->pln->pcost = pcost;

	  if (sizeof(trigreal) > sizeof(R)) {
	       /* this is probably faster, and we have enough trigreal
		  bits to maintain accuracy */
	       X(plan_awake)(p->pln, AWAKE_SQRTN_TABLE);
	  } else {
	       /* more accurate */
	       X(plan_awake)(p->pln, AWAKE_SINCOS);
	  }

	  /* we don't use pln for p->pln, above, since by re-creating the
	     plan we might use more patient wisdom from a timed-out mkplan */
	  X(plan_destroy_internal)(pln);
     } else
	  X(problem_destroy)(prb);

     /* discard all information not necessary to reconstruct the plan */
     plnr->adt->forget(plnr, FORGET_ACCURSED);

#ifdef FFTW_RANDOM_ESTIMATOR
     X(random_estimate_seed)++; /* subsequent "random" plans are distinct */
#endif

     if (after_planner_hook)
          after_planner_hook();
     
     return p;
}

void X(destroy_plan)(X(plan) p)
{
     if (p) {
          if (before_planner_hook)
               before_planner_hook();
     
          X(plan_awake)(p->pln, SLEEPY);
          X(plan_destroy_internal)(p->pln);
          X(problem_destroy)(p->prb);
          X(ifree)(p);

          if (after_planner_hook)
               after_planner_hook();
     }
}

int X(alignment_of)(R *p)
{
     return X(ialignment_of(p));
}
