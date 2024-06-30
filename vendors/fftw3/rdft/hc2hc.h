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

#include "rdft/rdft.h"

typedef void (*hc2hcapply) (const plan *ego, R *IO);
typedef struct hc2hc_solver_s hc2hc_solver;
typedef plan *(*hc2hc_mkinferior)(const hc2hc_solver *ego,
			    rdft_kind kind, INT r, INT m, INT s, 
			    INT vl, INT vs, INT mstart, INT mcount,
			    R *IO, planner *plnr);

typedef struct {
     plan super;
     hc2hcapply apply;
} plan_hc2hc;

extern plan *X(mkplan_hc2hc)(size_t size, const plan_adt *adt, 
			     hc2hcapply apply);

#define MKPLAN_HC2HC(type, adt, apply) \
  (type *)X(mkplan_hc2hc)(sizeof(type), adt, apply)

struct hc2hc_solver_s {
     solver super;
     INT r;

     hc2hc_mkinferior mkcldw;
};

hc2hc_solver *X(mksolver_hc2hc)(size_t size, INT r, hc2hc_mkinferior mkcldw);
extern hc2hc_solver *(*X(mksolver_hc2hc_hook))(size_t, INT, hc2hc_mkinferior);

void X(regsolver_hc2hc_direct)(planner *plnr, khc2hc codelet, 
			       const hc2hc_desc *desc);

int X(hc2hc_applicable)(const hc2hc_solver *, const problem *, planner *);
