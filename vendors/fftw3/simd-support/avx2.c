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

#if HAVE_AVX2

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#    include "amd64-cpuid.h"
#else
#    include "x86-cpuid.h"
#endif

int X(have_simd_avx2_128)(void)
{
    static int init = 0, res;
    int max_stdfn, eax, ebx, ecx, edx;
    
    if (!init) {
         cpuid_all(0,0,&eax,&ebx,&ecx,&edx);
         max_stdfn = eax;
         if (max_stdfn >= 0x1) {
              /* have AVX and OSXSAVE? (implies XGETBV exists) */
              cpuid_all(0x1, 0, &eax, &ebx, &ecx, &edx);
              if ((ecx & 0x18000000) == 0x18000000) {              
                   /* have AVX2? */
                   cpuid_all(7,0,&eax,&ebx,&ecx,&edx);
                   if (ebx & (1 << 5)) {
                        /* have OS support for XMM, YMM? */
                        res = ((xgetbv_eax(0) & 0x6) == 0x6);
                   }
              }
        }
        init = 1;
    }
    return res;
}

int X(have_simd_avx2)(void)
{
  /*
   * For now 256-bit AVX2 support is identical to 128-bit.
   * This might change in the future if AMD released AVX2-capable
   * chips that work better with the 128-bit flavor, but since AMD
   * might actually change it to implement 256-bit AVX2 efficiently
   * by then we don't want to disable it before we know.
   */
  return X(have_simd_avx2_128)();
}
#endif
