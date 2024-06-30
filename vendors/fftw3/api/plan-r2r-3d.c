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

X(plan) X(plan_r2r_3d)(int nx, int ny, int nz,
		       R *in, R *out, X(r2r_kind) kindx,
		       X(r2r_kind) kindy, X(r2r_kind) kindz, unsigned flags)
{
     int n[3];
     X(r2r_kind) kind[3];
     n[0] = nx;
     n[1] = ny;
     n[2] = nz;
     kind[0] = kindx;
     kind[1] = kindy;
     kind[2] = kindz;
     return X(plan_r2r)(3, n, in, out, kind, flags);
}
