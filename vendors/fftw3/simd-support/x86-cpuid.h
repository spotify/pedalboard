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


/* this code was kindly donated by Eric J. Korpela */

#ifdef _MSC_VER
#include <intrin.h>
#ifndef inline
#define inline __inline
#endif
#endif

static inline int is_386() 
{
#ifdef _MSC_VER
    unsigned int result,tst;
    _asm {
        pushfd
        pop eax
        mov edx,eax
        xor eax,40000h
        push eax
        popfd
        pushfd
        pop eax
        push edx
        popfd
        mov tst,edx
        mov result,eax
    }
#else
    register unsigned int result,tst;
    __asm__ (
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0,%1\n\t"
        "xorl $0x40000,%0\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        "pushl %1\n\t"
        "popfl"
    : "=r" (result), "=r" (tst) /* output */
    :  /* no inputs */
    );
#endif
    return (result == tst);
}

static inline int has_cpuid() 
{
#ifdef _MSC_VER
    unsigned int result,tst;
    _asm {
        pushfd
        pop eax
        mov edx,eax
        xor eax,200000h
        push eax
        popfd
        pushfd
        pop eax
        push edx
        popfd
        mov tst,edx
        mov result,eax
    }
#else
    register unsigned int result,tst;
    __asm__ (
        "pushfl\n\t"
        "pop %0\n\t"
        "movl %0,%1\n\t"
        "xorl $0x200000,%0\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        "pushl %1\n\t"
        "popfl"
    : "=r" (result), "=r" (tst) /* output */
    : /* no inputs */
    );
#endif
    return (result != tst);
}

/* cpuid version to get all registers. Donated by Erik Lindahl from Gromacs. */
static inline void
cpuid_all(int level, int ecxval, int *eax, int *ebx, int *ecx, int *edx)
{
#if (defined _MSC_VER)
    int CPUInfo[4];
    
#    if (_MSC_VER > 1500) || (_MSC_VER == 1500 & _MSC_FULL_VER >= 150030729)
    /* MSVC 9.0 SP1 or later */
    __cpuidex(CPUInfo, level, ecxval);
#    else
    __cpuid(CPUInfo, level);
    /* Set an error code if the user wanted a non-zero ecxval, since we did not have cpuidex */
#    endif
    *eax = CPUInfo[0];
    *ebx = CPUInfo[1];
    *ecx = CPUInfo[2];
    *edx = CPUInfo[3];

#else
    /* Not MSVC */
    *eax = level;
    *ecx = ecxval;
    *ebx = 0;
    *edx = 0;
    /* Avoid clobbering global offset table in 32-bit pic code (ebx) */
#    if defined(__PIC__)
    __asm__ ("xchgl %%ebx, %1  \n\t"
             "cpuid            \n\t"
             "xchgl %%ebx, %1  \n\t"
             : "+a" (*eax), "+r" (*ebx), "+c" (*ecx), "+d" (*edx));
#    else
    /* No need to save ebx if we are not in pic mode */
    __asm__ ("cpuid            \n\t"
             : "+a" (*eax), "+b" (*ebx), "+c" (*ecx), "+d" (*edx));
#    endif
#endif
}

static inline int cpuid_edx(int op)
{
#    ifdef _MSC_VER
     int result;
     _asm {
	  push ebx
          mov eax,op
          cpuid
          mov result,edx
          pop ebx
     }
     return result;
#    else
     int eax, ecx, edx;

     __asm__("push %%ebx\n\tcpuid\n\tpop %%ebx"
	     : "=a" (eax), "=c" (ecx), "=d" (edx)
	     : "a" (op));
     return edx;
#    endif
}

static inline int cpuid_ecx(int op)
{
#    ifdef _MSC_VER
     int result;
     _asm {
	  push ebx
          mov eax,op
          cpuid
          mov result,ecx
          pop ebx
     }
     return result;
#    else
     int eax, ecx, edx;

     __asm__("push %%ebx\n\tcpuid\n\tpop %%ebx"
	     : "=a" (eax), "=c" (ecx), "=d" (edx)
	     : "a" (op));
     return ecx;
#    endif
}

static inline int xgetbv_eax(int op)
{
#    ifdef _MSC_VER
     int veax, vedx;
     _asm {
          mov ecx,op
#    if defined(__INTEL_COMPILER) || (_MSC_VER >= 1600)
          xgetbv
#    else
          __emit 15
          __emit 1
          __emit 208
#    endif
          mov veax,eax
          mov vedx,edx
     }
     return veax;
#    else
     int eax, edx;
     __asm__ (".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c" (op));
     return eax;
#endif
}
