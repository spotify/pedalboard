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
#include <stdio.h>

/* 
 * System-dependent timing functions:
 */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BSDGETTIMEOFDAY
#ifndef HAVE_GETTIMEOFDAY
#define gettimeofday BSDgettimeofday
#define HAVE_GETTIMEOFDAY 1
#endif
#endif

double time_min;
int time_repeat;

#if !defined(HAVE_TIMER) && (defined(__WIN32__) || defined(_WIN32) || defined(_WINDOWS) || defined(__CYGWIN__))
#include <windows.h>
typedef LARGE_INTEGER mytime;

static mytime get_time(void)
{
     mytime tv;
     QueryPerformanceCounter(&tv);
     return tv;
}

static double elapsed(mytime t1, mytime t0)
{
     LARGE_INTEGER freq;
     QueryPerformanceFrequency(&freq);
     return (((double) t1.QuadPart - (double) t0.QuadPart)) /
	  ((double) freq.QuadPart);
}

#define HAVE_TIMER
#endif


#if defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_TIMER)
typedef struct timeval mytime;

static mytime get_time(void)
{
     struct timeval tv;
     gettimeofday(&tv, 0);
     return tv;
}

static double elapsed(mytime t1, mytime t0)
{
     return ((double) t1.tv_sec - (double) t0.tv_sec) +
	  ((double) t1.tv_usec - (double) t0.tv_usec) * 1.0E-6;
}

#define HAVE_TIMER
#endif

#ifndef HAVE_TIMER
#error "timer not defined"
#endif

static double calibrate(void)
{
     /* there seems to be no reasonable way to calibrate the
	clock automatically any longer.  Grrr... */

     return 0.01;
}


void timer_init(double tmin, int repeat)
{
     static int inited = 0;

     if (inited)
	  return;
     inited = 1;

     if (!repeat)
	  repeat = 8;
     time_repeat = repeat;

     if (tmin > 0)
	  time_min = tmin;
     else
	  time_min = calibrate();
}

static mytime t0[BENCH_NTIMERS];

void timer_start(int n)
{
     BENCH_ASSERT(n >= 0 && n < BENCH_NTIMERS);
     t0[n] = get_time();
}

double timer_stop(int n)
{
     mytime t1;
     BENCH_ASSERT(n >= 0 && n < BENCH_NTIMERS);
     t1 = get_time();
     return elapsed(t1, t0[n]);
}

