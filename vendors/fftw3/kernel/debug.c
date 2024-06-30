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

#ifdef FFTW_DEBUG
#include <stdio.h>

typedef struct {
     printer super;
     FILE *f;
} P_file;

static void putchr_file(printer *p_, char c)
{
     P_file *p = (P_file *) p_;
     fputc(c, p->f);
}

static printer *mkprinter_file(FILE *f)
{
     P_file *p = (P_file *) X(mkprinter)(sizeof(P_file), putchr_file, 0);
     p->f = f;
     return &p->super;
}

void X(debug)(const char *format, ...)
{
     va_list ap;
     printer *p = mkprinter_file(stderr);
     va_start(ap, format);
     p->vprint(p, format, ap);
     va_end(ap);
     X(printer_destroy)(p);
}
#endif
