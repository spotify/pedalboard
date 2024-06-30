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

/* openmp.c: thread spawning via OpenMP  */

#include "threads/threads.h"

#if !defined(_OPENMP)
#error OpenMP enabled but not using an OpenMP compiler
#endif

int X(ithreads_init)(void)
{
     return 0; /* no error */
}

/* Distribute a loop from 0 to loopmax-1 over nthreads threads.
   proc(d) is called to execute a block of iterations from d->min
   to d->max-1.  d->thr_num indicate the number of the thread
   that is executing proc (from 0 to nthreads-1), and d->data is
   the same as the data parameter passed to X(spawn_loop).

   This function returns only after all the threads have completed. */
void X(spawn_loop)(int loopmax, int nthr, spawn_function proc, void *data)
{
     int block_size;
     spawn_data d;
     int i;

     A(loopmax >= 0);
     A(nthr > 0);
     A(proc);

     if (!loopmax) return;

     /* Choose the block size and number of threads in order to (1)
        minimize the critical path and (2) use the fewest threads that
        achieve the same critical path (to minimize overhead).
        e.g. if loopmax is 5 and nthr is 4, we should use only 3
        threads with block sizes of 2, 2, and 1. */
     block_size = (loopmax + nthr - 1) / nthr;
     nthr = (loopmax + block_size - 1) / block_size;

     if (X(spawnloop_callback)) { /* user-defined spawnloop backend */
          spawn_data *sdata;
          STACK_MALLOC(spawn_data *, sdata, sizeof(spawn_data) * nthr);
          for (i = 0; i < nthr; ++i) {
               spawn_data *d = &sdata[i];
               d->max = (d->min = i * block_size) + block_size;
               if (d->max > loopmax)
                    d->max = loopmax;
               d->thr_num = i;
               d->data = data;
          }
          X(spawnloop_callback)(proc, sdata, sizeof(spawn_data), nthr, X(spawnloop_callback_data));
          STACK_FREE(sdata);
          return;
     }

#pragma omp parallel for private(d)
     for (i = 0; i < nthr; ++i) {
	  d.max = (d.min = i * block_size) + block_size;
	  if (d.max > loopmax)
	       d.max = loopmax;
	  d.thr_num = i;
	  d.data = data;
	  proc(&d);
     }
}

void X(threads_cleanup)(void)
{
}

/* FIXME [Matteo Frigo 2015-05-25] What does "thread-safe"
   mean for openmp? */
void X(threads_register_planner_hooks)(void)
{
}
