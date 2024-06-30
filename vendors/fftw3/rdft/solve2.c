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


#include "rdft/rdft.h"

/* use the apply() operation for RDFT2 problems */
void X(rdft2_solve)(const plan *ego_, const problem *p_)
{
     const plan_rdft2 *ego = (const plan_rdft2 *) ego_;
     const problem_rdft2 *p = (const problem_rdft2 *) p_;
     ego->apply(ego_, 
		UNTAINT(p->r0), UNTAINT(p->r1),
		UNTAINT(p->cr), UNTAINT(p->ci));
}
