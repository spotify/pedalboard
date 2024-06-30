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

void X(export_wisdom_to_file)(FILE *output_file)
{
     printer *p = X(mkprinter_file)(output_file);
     planner *plnr = X(the_planner)();
     plnr->adt->exprt(plnr, p);
     X(printer_destroy)(p);
}

int X(export_wisdom_to_filename)(const char *filename)
{
     FILE *f = fopen(filename, "w");
     int ret;
     if (!f) return 0; /* error opening file */
     X(export_wisdom_to_file)(f);
     ret = !ferror(f);
     if (fclose(f)) ret = 0; /* error closing file */
     return ret;
}
