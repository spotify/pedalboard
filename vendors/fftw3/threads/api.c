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
#include "threads/threads.h"

static int threads_inited = 0;

static void threads_register_hooks(void)
{
     X(mksolver_ct_hook) = X(mksolver_ct_threads);
     X(mksolver_hc2hc_hook) = X(mksolver_hc2hc_threads);
}

static void threads_unregister_hooks(void)
{
     X(mksolver_ct_hook) = 0;
     X(mksolver_hc2hc_hook) = 0;
}

/* should be called before all other FFTW functions! */
int X(init_threads)(void)
{
     if (!threads_inited) {
	  planner *plnr;

          if (X(ithreads_init)())
               return 0;

	  threads_register_hooks();

	  /* this should be the first time the_planner is called,
	     and hence the time it is configured */
	  plnr = X(the_planner)();
	  X(threads_conf_standard)(plnr);

          threads_inited = 1;
     }
     return 1;
}


void X(cleanup_threads)(void)
{
     X(cleanup)();
     if (threads_inited) {
	  X(threads_cleanup)();
	  threads_unregister_hooks();
	  threads_inited = 0;
     }
}

void X(plan_with_nthreads)(int nthreads)
{
     planner *plnr;

     if (!threads_inited) {
	  X(cleanup)();
	  X(init_threads)();
     }
     A(threads_inited);
     plnr = X(the_planner)();
     plnr->nthr = X(imax)(1, nthreads);
}

int X(planner_nthreads)(void)
{
    return X(the_planner)()->nthr;
}

void X(make_planner_thread_safe)(void)
{
     X(threads_register_planner_hooks)();
}

spawnloop_function X(spawnloop_callback) = (spawnloop_function) 0;
void *X(spawnloop_callback_data) = (void *) 0;
void X(threads_set_callback)(void (*spawnloop)(void *(*work)(char *), char *, size_t, int, void *), void *data)
{
     X(spawnloop_callback) = (spawnloop_function) spawnloop;
     X(spawnloop_callback_data) = data;
}
