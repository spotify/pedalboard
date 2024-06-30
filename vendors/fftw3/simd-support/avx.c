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

#if HAVE_AVX

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#    include "amd64-cpuid.h"
#else
#    include "x86-cpuid.h"
#endif

int X(have_simd_avx)(void)
{
     static int init = 0, res = 0;
     int max_stdfn, eax, ebx, ecx, edx;

     if (!init) {
          cpuid_all(0,0,&eax,&ebx,&ecx,&edx);
          max_stdfn = eax;
          if (max_stdfn >= 0x1) {
               /* have AVX and OSXSAVE? (implies XGETBV exists) */
               cpuid_all(0x1, 0, &eax, &ebx, &ecx, &edx);
               if ((ecx & 0x18000000) == 0x18000000) {
                    /* have OS support for XMM, YMM? */
                    res = ((xgetbv_eax(0) & 0x6) == 0x6);                    
               }
          }
          init = 1;
     }
     return res;
}

#endif

