/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * VSX SIMD implementation added 2015 Erik Lindahl.
 * Erik Lindahl places his modifications in the public domain.
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

#if HAVE_VSX

#if HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
#endif

#include <signal.h>
#include <setjmp.h>

static jmp_buf jb;

static void sighandler(int x)
{
     longjmp(jb, 1);
}

static int really_have_vsx(void)
{
     void (*oldsig)(int);
     oldsig = signal(SIGILL, sighandler);
     if (setjmp(jb)) {
	  signal(SIGILL, oldsig);
	  return 0;
     } else {
          float mem[2];
          __asm__ __volatile__ ("stxsdx 0,0,%0" :: "r" (mem) : "memory" );
	  signal(SIGILL, oldsig);
	  return 1;
     }
     return 0;
}

int X(have_simd_vsx)(void)
{
     static int init = 0, res;
     if (!init) {
	  res = really_have_vsx();
	  init = 1;
     }
     return res;
}

#endif
