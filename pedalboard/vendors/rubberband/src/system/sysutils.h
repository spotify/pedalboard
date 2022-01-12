/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2021 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#ifndef RUBBERBAND_SYSUTILS_H
#define RUBBERBAND_SYSUTILS_H

#ifdef __clang__
#  define R__ __restrict__
#else
#  ifdef __GNUC__
#    define R__ __restrict__
#  endif
#endif

#ifdef _MSC_VER
#  if _MSC_VER < 1800
#    include "float_cast/float_cast.h"
#  endif
#  ifndef R__
#    define R__ __restrict
#  endif
#endif 

#ifndef R__
#  define R__
#endif

#if defined(_MSC_VER)
#  include <malloc.h>
#  include <process.h>
#  define alloca _alloca
#  define getpid _getpid
#else
#  if defined(__MINGW32__)
#    include <malloc.h>
#  elif defined(__GNUC__)
#    ifndef alloca
#      define alloca __builtin_alloca
#    endif
#  elif defined(HAVE_ALLOCA_H)
#    include <alloca.h>
#  else
#    ifndef __USE_MISC
#    define __USE_MISC
#    endif
#    include <stdlib.h>
#  endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1700
#  define uint8_t unsigned __int8
#  define uint16_t unsigned __int16
#  define uint32_t unsigned __int32
#elif defined(_MSC_VER)
#  define ssize_t long
#  include <stdint.h>
#else
#  include <stdint.h>
#endif

#include <math.h>

namespace RubberBand {

extern const char *system_get_platform_tag();
extern bool system_is_multiprocessor();
extern void system_specific_initialise();
extern void system_specific_application_initialise();

enum ProcessStatus { ProcessRunning, ProcessNotRunning, UnknownProcessStatus };
extern ProcessStatus system_get_process_status(int pid);

#ifdef _WIN32
struct timeval { long tv_sec; long tv_usec; };
void gettimeofday(struct timeval *p, void *tz);
#endif // _WIN32

#ifdef _MSC_VER
void usleep(unsigned long);
#endif // _MSC_VER

inline double mod(double x, double y) { return x - (y * floor(x / y)); }
inline float modf(float x, float y) { return x - (y * float(floor(x / y))); }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // M_PI

inline double princarg(double a) { return mod(a + M_PI, -2.0 * M_PI) + M_PI; }
inline float princargf(float a) { return modf(a + (float)M_PI, -2.f * (float)M_PI) + (float)M_PI; }

} // end namespace

// The following should be functions in the RubberBand namespace, really

#ifdef _WIN32

#define MLOCK(a,b)   1
#define MUNLOCK(a,b) 1
#define MUNLOCK_SAMPLEBLOCK(a) 1

namespace RubberBand {
extern void system_memorybarrier();
}
#define MBARRIER() RubberBand::system_memorybarrier()

#define DLOPEN(a,b)  LoadLibrary((a).toStdWString().c_str())
#define DLSYM(a,b)   GetProcAddress((HINSTANCE)(a),(b))
#define DLCLOSE(a)   FreeLibrary((HINSTANCE)(a))
#define DLERROR()    ""

#else // !_WIN32

#include <sys/mman.h>
#include <dlfcn.h>
#include <stdio.h>

#define MLOCK(a,b)   mlock((char *)(a),(b))
#define MUNLOCK(a,b) (munlock((char *)(a),(b)) ? (perror("munlock failed"), 0) : 0)
#define MUNLOCK_SAMPLEBLOCK(a) do { if (!(a).empty()) { const float &b = *(a).begin(); MUNLOCK(&b, (a).capacity() * sizeof(float)); } } while(0);

#ifdef __APPLE__
#  if defined __MAC_10_12
#    define MBARRIER() __sync_synchronize()
#  else
#    include <libkern/OSAtomic.h>
#    define MBARRIER() OSMemoryBarrier()
#  endif
#else
#  if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#    define MBARRIER() __sync_synchronize()
#  else
namespace RubberBand {
extern void system_memorybarrier();
}
#    define MBARRIER() ::RubberBand::system_memorybarrier()
#  endif
#endif

#define DLOPEN(a,b)  dlopen((a).toStdString().c_str(),(b))
#define DLSYM(a,b)   dlsym((a),(b))
#define DLCLOSE(a)   dlclose((a))
#define DLERROR()    dlerror()

#endif // !_WIN32

#ifdef NO_THREADING
#  undef MBARRIER
#  define MBARRIER() 
#endif // NO_THREADING

#endif

