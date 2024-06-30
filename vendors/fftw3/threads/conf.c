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


#include "threads/threads.h"

static const solvtab s =
{
     SOLVTAB(X(dft_thr_vrank_geq1_register)),
     SOLVTAB(X(rdft_thr_vrank_geq1_register)),
     SOLVTAB(X(rdft2_thr_vrank_geq1_register)),

     SOLVTAB_END
};

void X(threads_conf_standard)(planner *p)
{
     X(solvtab_exec)(s, p);
}
