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


void X(md5putb)(md5 *p, const void *d_, size_t len)
{
     size_t i;
     const unsigned char *d = (const unsigned char *)d_;
     for (i = 0; i < len; ++i)
	  X(md5putc)(p, d[i]);
}

void X(md5puts)(md5 *p, const char *s)
{
     /* also hash final '\0' */
     do {
	  X(md5putc)(p, (unsigned)(*s & 0xFF));
     } while(*s++);
}

void X(md5int)(md5 *p, int i)
{
     X(md5putb)(p, &i, sizeof(i));
}

void X(md5INT)(md5 *p, INT i)
{
     X(md5putb)(p, &i, sizeof(i));
}

void X(md5unsigned)(md5 *p, unsigned i)
{
     X(md5putb)(p, &i, sizeof(i));
}

