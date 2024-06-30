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

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#else
#  define DS(d,s) d /* double-precision option */
#endif

#if HAVE_SSE2

# if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)

  int X(have_simd_sse2)(void)
  {
       return 1;
  }

# else /* !x86_64 */

# include <signal.h>
# include <setjmp.h>
# include "x86-cpuid.h"

  static jmp_buf jb;

  static void sighandler(int x)
  {
       UNUSED(x);
       longjmp(jb, 1);
  }

  static int sse2_works(void)
  {
       void (*oldsig)(int);
       oldsig = signal(SIGILL, sighandler);
       if (setjmp(jb)) {
	    signal(SIGILL, oldsig);
	    return 0;
       } else {
#         ifdef _MSC_VER
	    _asm { DS(xorpd,xorps) xmm0,xmm0 }
#         else
	    /* asm volatile ("xorpd/s %xmm0, %xmm0"); */
	    asm volatile(DS(".byte 0x66; .byte 0x0f; .byte 0x57; .byte 0xc0",
			                ".byte 0x0f; .byte 0x57; .byte 0xc0"));
#         endif
	    signal(SIGILL, oldsig);
	    return 1;
       }
  }

  int X(have_simd_sse2)(void)
  {
       static int init = 0, res;

       if (!init) {
	    res =   !is_386() 
		 && has_cpuid()
		 && (cpuid_edx(1) & (1 << DS(26,25)))
		 && sse2_works();
	    init = 1;
       }
       return res;
  }

# endif

#endif
