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


#include "kernel/ifftw.h"

#if HAVE_SIMD
#  define ALGN 16
#else
   /* disable the alignment machinery, because it will break,
      e.g., if sizeof(R) == 12 (as in long-double/x86) */
#  define ALGN 0
#endif

/* NONPORTABLE */
int X(ialignment_of)(R *p)
{
#if ALGN == 0
     UNUSED(p);
     return 0;
#else
     return (int)(((uintptr_t) p) % ALGN);
#endif
}
