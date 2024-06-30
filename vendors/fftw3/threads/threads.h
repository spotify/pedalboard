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

#ifndef __THREADS_H__
#define __THREADS_H__

#include "kernel/ifftw.h"
#include "dft/ct.h"
#include "rdft/hc2hc.h"

typedef struct {
     int min, max, thr_num;
     void *data;
} spawn_data;

typedef void *(*spawn_function) (spawn_data *);

void X(spawn_loop)(int loopmax, int nthreads,
		   spawn_function proc, void *data);
int X(ithreads_init)(void);
void X(threads_cleanup)(void);

typedef void (*spawnloop_function)(spawn_function, spawn_data *, size_t, int, void *);
extern spawnloop_function X(spawnloop_callback);
extern void *X(spawnloop_callback_data);

/* configurations */

void X(dft_thr_vrank_geq1_register)(planner *p);
void X(rdft_thr_vrank_geq1_register)(planner *p);
void X(rdft2_thr_vrank_geq1_register)(planner *p);

ct_solver *X(mksolver_ct_threads)(size_t size, INT r, int dec,
				  ct_mkinferior mkcldw,
				  ct_force_vrecursion force_vrecursionp);
hc2hc_solver *X(mksolver_hc2hc_threads)(size_t size, INT r, hc2hc_mkinferior mkcldw);

void X(threads_conf_standard)(planner *p);
void X(threads_register_hooks)(void);
void X(threads_unregister_hooks)(void);
void X(threads_register_planner_hooks)(void);

#endif /* __THREADS_H__ */
