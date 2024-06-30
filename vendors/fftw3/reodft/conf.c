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


#include "reodft/reodft.h"

static const solvtab s =
{
#if 0 /* 1 to enable "standard" algorithms with substandard accuracy;
         you must also add them to Makefile.am to compile these files*/
     SOLVTAB(X(redft00e_r2hc_register)),
     SOLVTAB(X(rodft00e_r2hc_register)),
     SOLVTAB(X(reodft11e_r2hc_register)),
#endif
     SOLVTAB(X(redft00e_r2hc_pad_register)),
     SOLVTAB(X(rodft00e_r2hc_pad_register)),
     SOLVTAB(X(reodft00e_splitradix_register)),
     SOLVTAB(X(reodft010e_r2hc_register)),
     SOLVTAB(X(reodft11e_radix2_r2hc_register)),
     SOLVTAB(X(reodft11e_r2hc_odd_register)),

     SOLVTAB_END
};

void X(reodft_conf_standard)(planner *p)
{
     X(solvtab_exec)(s, p);
}
