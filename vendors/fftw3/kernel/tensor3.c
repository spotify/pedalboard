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

/* Currently, mktensor_4d and mktensor_5d are only used in the MPI
   routines, where very complicated transpositions are required.
   Therefore we split them into a separate source file. */

tensor *X(mktensor_4d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1,
		       INT n2, INT is2, INT os2,
		       INT n3, INT is3, INT os3)
{
     tensor *x = X(mktensor)(4);
     x->dims[0].n = n0;
     x->dims[0].is = is0;
     x->dims[0].os = os0;
     x->dims[1].n = n1;
     x->dims[1].is = is1;
     x->dims[1].os = os1;
     x->dims[2].n = n2;
     x->dims[2].is = is2;
     x->dims[2].os = os2;
     x->dims[3].n = n3;
     x->dims[3].is = is3;
     x->dims[3].os = os3;
     return x;
}

tensor *X(mktensor_5d)(INT n0, INT is0, INT os0,
		       INT n1, INT is1, INT os1,
		       INT n2, INT is2, INT os2,
		       INT n3, INT is3, INT os3,
		       INT n4, INT is4, INT os4)
{
     tensor *x = X(mktensor)(5);
     x->dims[0].n = n0;
     x->dims[0].is = is0;
     x->dims[0].os = os0;
     x->dims[1].n = n1;
     x->dims[1].is = is1;
     x->dims[1].os = os1;
     x->dims[2].n = n2;
     x->dims[2].is = is2;
     x->dims[2].os = os2;
     x->dims[3].n = n3;
     x->dims[3].is = is3;
     x->dims[3].os = os3;
     x->dims[4].n = n4;
     x->dims[4].is = is4;
     x->dims[4].os = os4;
     return x;
}
