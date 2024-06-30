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

void X(flops)(const X(plan) p, double *add, double *mul, double *fma)
{
     planner *plnr = X(the_planner)();
     opcnt *o = &p->pln->ops;
     *add = o->add; *mul = o->mul; *fma = o->fma;
     if (plnr->cost_hook) {
	  *add = plnr->cost_hook(p->prb, *add, COST_SUM);
	  *mul = plnr->cost_hook(p->prb, *mul, COST_SUM);
	  *fma = plnr->cost_hook(p->prb, *fma, COST_SUM);
     }
}

double X(estimate_cost)(const X(plan) p)
{
     return X(iestimate_cost)(X(the_planner)(), p->pln, p->prb);
}

double X(cost)(const X(plan) p)
{
     return p->pln->pcost;
}
