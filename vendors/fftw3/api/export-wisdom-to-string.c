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

char *X(export_wisdom_to_string)(void)
{
     printer *p;
     planner *plnr = X(the_planner)();
     size_t cnt;
     char *s;

     p = X(mkprinter_cnt)(&cnt);
     plnr->adt->exprt(plnr, p);
     X(printer_destroy)(p);

     s = (char *) malloc(sizeof(char) * (cnt + 1));
     if (s) {
          p = X(mkprinter_str)(s);
          plnr->adt->exprt(plnr, p);
          X(printer_destroy)(p);
     }

     return s;
}
