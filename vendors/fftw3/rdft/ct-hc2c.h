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

typedef void (*hc2capply) (const plan *ego, R *cr, R *ci);
typedef struct hc2c_solver_s hc2c_solver;
typedef plan *(*hc2c_mkinferior)(const hc2c_solver *ego, rdft_kind kind,
				 INT r, INT rs,
				 INT m, INT ms, 
				 INT v, INT vs,
				 R *cr, R *ci,
				 planner *plnr);

typedef struct {
     plan super;
     hc2capply apply;
} plan_hc2c;

extern plan *X(mkplan_hc2c)(size_t size, const plan_adt *adt, 
			    hc2capply apply);

#define MKPLAN_HC2C(type, adt, apply) \
  (type *)X(mkplan_hc2c)(sizeof(type), adt, apply)

struct hc2c_solver_s {
     solver super;
     INT r;

     hc2c_mkinferior mkcldw;
     hc2c_kind hc2ckind;
};

hc2c_solver *X(mksolver_hc2c)(size_t size, INT r,
			      hc2c_kind hc2ckind,
			      hc2c_mkinferior mkcldw);

void X(regsolver_hc2c_direct)(planner *plnr, khc2c codelet, 
			      const hc2c_desc *desc,
			      hc2c_kind hc2ckind);
