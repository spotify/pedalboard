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

static planner *plnr = 0;

/* create the planner for the rest of the API */
planner *X(the_planner)(void)
{
     if (!plnr) {
          plnr = X(mkplanner)();
          X(configure_planner)(plnr);
     }

     return plnr;
}

void X(cleanup)(void)
{
     if (plnr) {
          X(planner_destroy)(plnr);
          plnr = 0;
     }
}

void X(set_timelimit)(double tlim) 
{
     /* PLNR is not necessarily initialized when this function is
	called, so use X(the_planner)() */
     X(the_planner)()->timelimit = tlim; 
}
