/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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


#include "libbench2/bench.h"

/* set I/O arrays to zero.  Default routine */
void problem_zero(bench_problem *p)
{
     bench_complex czero = {0, 0};
     if (p->kind == PROBLEM_COMPLEX) {
	  caset((bench_complex *) p->inphys, p->iphyssz, czero);
	  caset((bench_complex *) p->outphys, p->ophyssz, czero);
     } else if (p->kind == PROBLEM_R2R) {
	  aset((bench_real *) p->inphys, p->iphyssz, 0.0);
	  aset((bench_real *) p->outphys, p->ophyssz, 0.0);
     } else if (p->kind == PROBLEM_REAL && p->sign < 0) {
	  aset((bench_real *) p->inphys, p->iphyssz, 0.0);
	  caset((bench_complex *) p->outphys, p->ophyssz, czero);
     } else if (p->kind == PROBLEM_REAL && p->sign > 0) {
	  caset((bench_complex *) p->inphys, p->iphyssz, czero);
	  aset((bench_real *) p->outphys, p->ophyssz, 0.0);
     } else {
	  BENCH_ASSERT(0); /* TODO */
     }
}
