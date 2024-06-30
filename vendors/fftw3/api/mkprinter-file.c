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
#include <stdio.h>

#define BUFSZ 256

typedef struct {
     printer super;
     FILE *f;
     char buf[BUFSZ];
     char *bufw;
} P;

static void myflush(P *p)
{
     fwrite(p->buf, 1, p->bufw - p->buf, p->f);
     p->bufw = p->buf;
}

static void myputchr(printer *p_, char c)
{
     P *p = (P *) p_;
     if (p->bufw >= p->buf + BUFSZ)
	  myflush(p);
     *p->bufw++ = c;
}

static void mycleanup(printer *p_)
{
     P *p = (P *) p_;
     myflush(p);
}

printer *X(mkprinter_file)(FILE *f)
{
     P *p = (P *) X(mkprinter)(sizeof(P), myputchr, mycleanup);
     p->f = f;
     p->bufw = p->buf;
     return &p->super;
}
