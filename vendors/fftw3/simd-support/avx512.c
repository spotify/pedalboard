/*
 * Copyright (c) 2003, 2007-11 Matteo Frigo
 * Copyright (c) 2003, 2007-11 Massachusetts Institute of Technology
 * Copyright (c) 2012-2013 Romain Dolbeau
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "kernel/ifftw.h"

#if HAVE_AVX512

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)

#include "amd64-cpuid.h"

int X(have_simd_avx512)(void)
{
     static int init = 0, res;     
     int max_stdfn, eax, ebx, ecx, edx;

     /* NOTE: this code is a total guess.  I don't have an avx512
        machine available.  The code contributed by Erik Lindahl would
        crash on a machine without XGETBV, so I had to guess a fix. */
     if (!init) {
          cpuid_all(0,0,&eax,&ebx,&ecx,&edx);
          max_stdfn = eax;
          if (max_stdfn >= 0x1) {
               /* have OSXSAVE? (implies XGETBV exists) */
               cpuid_all(0x1, 0, &eax, &ebx, &ecx, &edx);
               if ((ecx & 0x08000000) == 0x08000000) {
                    /* have AVX512? */
                    cpuid_all(7,0,&eax,&ebx,&ecx,&edx);
                    if (ebx & (1 << 16)) {
                         /* have OS support for XMM, YMM, ZMM */
                         int zmm_ymm_xmm = (7 << 5) | (1 << 2) | (1 << 1);
                         res = ((xgetbv_eax(0) & zmm_ymm_xmm) == zmm_ymm_xmm);
                    }
               }
          }
          init = 1;
     }

     return res;
}

#else /* 32-bit code */

#error "Avx512 is 64 bits only"

#endif

#endif
