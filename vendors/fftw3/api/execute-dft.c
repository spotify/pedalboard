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
#include "dft/dft.h"

/* guru interface: requires care in alignment etcetera. */
void X(execute_dft)(const X(plan) p, C *in, C *out)
{
     plan_dft *pln = (plan_dft *) p->pln;
     if (p->sign == FFT_SIGN)
	  pln->apply((plan *) pln, in[0], in[0]+1, out[0], out[0]+1);
     else
	  pln->apply((plan *) pln, in[0]+1, in[0], out[0]+1, out[0]);
}
