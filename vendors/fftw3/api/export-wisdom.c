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

typedef struct {
     printer super;
     void (*write_char)(char c, void *);
     void *data;
} P;

static void putchr_generic(printer * p_, char c)
{
     P *p = (P *) p_;
     (p->write_char)(c, p->data);
}

void X(export_wisdom)(void (*write_char)(char c, void *), void *data)
{
     P *p = (P *) X(mkprinter)(sizeof(P), putchr_generic, 0);
     planner *plnr = X(the_planner)();

     p->write_char = write_char;
     p->data = data;
     plnr->adt->exprt(plnr, (printer *) p);
     X(printer_destroy)((printer *) p);
}
