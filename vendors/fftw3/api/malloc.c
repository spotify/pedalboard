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


void *X(malloc)(size_t n)
{
     return X(kernel_malloc)(n);
}

void X(free)(void *p)
{
     X(kernel_free)(p);
}

/* The following two routines are mainly for the convenience of
   the Fortran 2003 API, although C users may find them convienent
   as well.  The problem is that, although Fortran 2003 has a
   c_sizeof intrinsic that is equivalent to sizeof, it is broken
   in some gfortran versions, and in any case is a bit unnatural
   in a Fortran context.  So we provide routines to allocate real
   and complex arrays, which are all that are really needed by FFTW. */

R *X(alloc_real)(size_t n)
{
     return (R *) X(malloc)(sizeof(R) * n);
}

C *X(alloc_complex)(size_t n)
{
     return (C *) X(malloc)(sizeof(C) * n);
}
