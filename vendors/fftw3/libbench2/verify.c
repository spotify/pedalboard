/*
 * Copyright (c) 2000 Matteo Frigo
 * Copyright (c) 2000 Massachusetts Institute of Technology
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


#include <stdio.h>
#include <stdlib.h>

#include "verify.h"

void verify_problem(bench_problem *p, int rounds, double tol)
{
     errors e;
     const char *pstring = p->pstring ? p->pstring : "<unknown problem>";

     switch (p->kind) {
	 case PROBLEM_COMPLEX: verify_dft(p, rounds, tol, &e); break;
	 case PROBLEM_REAL: verify_rdft2(p, rounds, tol, &e); break;
	 case PROBLEM_R2R: verify_r2r(p, rounds, tol, &e); break;
     }

     if (verbose)
	  ovtpvt("%s %g %g %g\n", pstring, e.l, e.i, e.s);
}

void verify(const char *param, int rounds, double tol)
{
     bench_problem *p;

     p = problem_parse(param);
     problem_alloc(p);

     if (!can_do(p)) {
	  ovtpvt_err("No can_do for %s\n", p->pstring);
	  BENCH_ASSERT(0);
     }

     problem_zero(p);
     setup(p);

     verify_problem(p, rounds, tol);

     done(p);
     problem_destroy(p);
}


static void do_accuracy(bench_problem *p, int rounds, int impulse_rounds)
{
     double t[6];

     switch (p->kind) {
	 case PROBLEM_COMPLEX:
	      accuracy_dft(p, rounds, impulse_rounds, t); break;
	 case PROBLEM_REAL:
	      accuracy_rdft2(p, rounds, impulse_rounds, t); break;
	 case PROBLEM_R2R:
	      accuracy_r2r(p, rounds, impulse_rounds, t); break;
     }

     /* t[0] : L1 error
	t[1] : L2 error
	t[2] : Linf error
	t[3..5]: L1, L2, Linf backward error */
     ovtpvt("%6.2e %6.2e %6.2e %6.2e %6.2e %6.2e\n", 
	    t[0], t[1], t[2], t[3], t[4], t[5]);
}

void accuracy(const char *param, int rounds, int impulse_rounds)
{
     bench_problem *p;
     p = problem_parse(param);
     BENCH_ASSERT(can_do(p));
     problem_alloc(p);
     problem_zero(p);
     setup(p);
     do_accuracy(p, rounds, impulse_rounds);
     done(p);
     problem_destroy(p);
}
