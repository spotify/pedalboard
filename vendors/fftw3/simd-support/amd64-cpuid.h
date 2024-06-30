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


#ifdef _MSC_VER
#ifndef inline
#define inline __inline
#endif
#endif

#ifdef _MSC_VER
#include <intrin.h>
#if (_MSC_VER >= 1600) && !defined(__INTEL_COMPILER)
#include <immintrin.h>
#endif
#endif

/* cpuid version to get all registers. Donated by Erik Lindahl from Gromacs. */
static inline void
cpuid_all(int level, int ecxval, int *eax, int *ebx, int *ecx, int *edx)
{
#    ifdef _MSC_VER
    int CPUInfo[4];

#if (_MSC_VER > 1500) || (_MSC_VER == 1500 & _MSC_FULL_VER >= 150030729)
    /* MSVC 9.0 SP1 or later */
    __cpuidex(CPUInfo, level, ecxval);
#else
    __cpuid(CPUInfo, level);
#endif
    *eax = CPUInfo[0];
    *ebx = CPUInfo[1];
    *ecx = CPUInfo[2];
    *edx = CPUInfo[3];
    
#    else
    /* Not MSVC */
    *eax = level;
    *ecx = ecxval;
    *ebx = 0;
    *edx = 0;
    /* No need to save ebx if we are not in pic mode */
    __asm__ ("cpuid            \n\t"
             : "+a" (*eax), "+b" (*ebx), "+c" (*ecx), "+d" (*edx));
#    endif
}


static inline int cpuid_ecx(int op)
{
#    ifdef _MSC_VER
#    ifdef __INTEL_COMPILER
     int result;
     _asm {
	  push rbx
          mov eax,op
          cpuid
          mov result,ecx
          pop rbx
     }
     return result;
#    else
     int cpu_info[4];
     __cpuid(cpu_info,op);
     return cpu_info[2];
#    endif
#    else
     int eax, ecx = 0, edx;

     __asm__("pushq %%rbx\n\tcpuid\n\tpopq %%rbx"
	     : "=a" (eax), "+c" (ecx), "=d" (edx)
	     : "a" (op));
     return ecx;
#    endif
}

static inline int cpuid_ebx(int op)
{
#    ifdef _MSC_VER
#    ifdef __INTEL_COMPILER
     int result;
     _asm {
          push rbx
          mov eax,op
          cpuid
          mov result,ebx
          pop rbx
     }
     return result;
#    else
     int cpu_info[4];
     __cpuid(cpu_info,op);
     return cpu_info[1];
#    endif
#    else
     int eax, ecx = 0, edx;

     __asm__("pushq %%rbx\n\tcpuid\nmov %%ebx,%%ecx\n\tpopq %%rbx"
             : "=a" (eax), "+c" (ecx), "=d" (edx)
             : "a" (op));
     return ecx;
#    endif
}

static inline int xgetbv_eax(int op)
{
#    ifdef _MSC_VER
#    ifdef __INTEL_COMPILER
     int veax, vedx;
     _asm {
          mov ecx,op
          xgetbv
          mov veax,eax
          mov vedx,edx
     }
     return veax;
#    else
#    if defined(_MSC_VER) && (_MSC_VER >= 1600)
     unsigned __int64 result;
     result = _xgetbv(op);
     return (int)result;
#    else
#    error "Need at least Visual Studio 10 SP1 for AVX support"
#    endif
#    endif
#    else
     int eax, edx;
     __asm__ (".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c" (op));
     return eax;
#endif
}
