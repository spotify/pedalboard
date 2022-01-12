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

#include "sysutils.h"

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else /* !_WIN32 */
#include <signal.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#else /* !__APPLE__, !_WIN32 */
#include <stdio.h>
#include <string.h>
#endif /* !__APPLE__, !_WIN32 */
#endif /* !_WIN32 */

#ifdef __sun
#include <sys/processor.h>
#endif

#include <cstdlib>
#include <iostream>

#ifdef HAVE_IPP
#include <ippversion.h>
#include <ipp.h>
#endif

#ifdef HAVE_VDSP
#include <Accelerate/Accelerate.h>
#include <fenv.h>
#endif

#ifdef _WIN32
#include <fstream>
#endif


namespace RubberBand {

const char *
system_get_platform_tag()
{
#ifdef _WIN32
    return "win32";
#else /* !_WIN32 */
#ifdef __APPLE__
    return "osx";
#else /* !__APPLE__ */
#ifdef __LINUX__
    if (sizeof(long) == 8) {
        return "linux64";
    } else {
        return "linux";
    }
#else /* !__LINUX__ */
    return "posix";
#endif /* !__LINUX__ */
#endif /* !__APPLE__ */
#endif /* !_WIN32 */
}

bool
system_is_multiprocessor()
{
    static bool tested = false, mp = false;

    if (tested) return mp;
    int count = 0;

#ifdef _WIN32

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    count = sysinfo.dwNumberOfProcessors;

#else /* !_WIN32 */
#ifdef __APPLE__
    
    size_t sz = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &sz, NULL, 0)) {
        count = 0;
        mp = false;
    } else {
        mp = (count > 1);
    }

#else /* !__APPLE__, !_WIN32 */
#ifdef __sun

    processorid_t i, n;
    n = sysconf(_SC_CPUID_MAX);
    for (i = 0; i <= n; ++i) {
        int status = p_online(i, P_STATUS);
        if (status == P_ONLINE) {
            ++count;
        }
        if (count > 1) break;
    }

#else /* !__sun, !__APPLE__, !_WIN32 */

    //...

    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) return false;

    char buf[256];
    while (!feof(cpuinfo)) {
        if (!fgets(buf, 256, cpuinfo)) break;
        if (!strncmp(buf, "processor", 9)) {
            ++count;
        }
        if (count > 1) break;
    }

    fclose(cpuinfo);

#endif /* !__sun, !__APPLE__, !_WIN32 */
#endif /* !__APPLE__, !_WIN32 */
#endif /* !_WIN32 */

    mp = (count > 1);
    tested = true;
    return mp;
}

#ifdef _WIN32

void gettimeofday(struct timeval *tv, void *tz)
{
    union { 
	long long ns100;  
	FILETIME ft; 
    } now; 
    
    ::GetSystemTimeAsFileTime(&now.ft); 
    tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL); 
    tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL); 
}

void usleep(unsigned long usec)
{
    ::Sleep(usec == 0 ? 0 : usec < 1000 ? 1 : usec / 1000);
}

#endif

void system_specific_initialise()
{
#if defined HAVE_IPP
#ifndef USE_IPP_DYNAMIC_LIBS
#if (IPP_VERSION_MAJOR < 9)
    // This was removed in v9
    ippStaticInit();
#endif
#endif
    ippSetDenormAreZeros(1);
#elif defined HAVE_VDSP
#if defined __i386__ || defined __x86_64__ 
    fesetenv(FE_DFL_DISABLE_SSE_DENORMS_ENV);
#elif defined __arm64__
    fesetenv(FE_DFL_DISABLE_DENORMS_ENV);
#endif
#endif
#if defined __ARMEL__
    // ARM32
    static const unsigned int x = 0x04086060;
    static const unsigned int y = 0x03000000;
    int r;
    asm volatile (
        "fmrx	%0, fpscr   \n\t"
        "and	%0, %0, %1  \n\t"
        "orr	%0, %0, %2  \n\t"
        "fmxr	fpscr, %0   \n\t"
        : "=r"(r)
        : "r"(x), "r"(y)
	);
#endif
}

void system_specific_application_initialise()
{
}


ProcessStatus
system_get_process_status(int pid)
{
#ifdef _WIN32
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle) {
        return ProcessNotRunning;
    } else {
        CloseHandle(handle);
        return ProcessRunning;
    }
#else
    if (kill(getpid(), 0) == 0) {
        if (kill(pid, 0) == 0) {
            return ProcessRunning;
        } else {
            return ProcessNotRunning;
        }
    } else {
        return UnknownProcessStatus;
    }
#endif
}

#ifdef _WIN32
void system_memorybarrier()
{
#ifdef _MSC_VER
    MemoryBarrier();
#else /* (mingw) */
    LONG Barrier = 0;
    __asm__ __volatile__("xchgl %%eax,%0 "
                         : "=r" (Barrier));
#endif
}
#else /* !_WIN32 */
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
// Not required
#else
#include <pthread.h>
void system_memorybarrier()
{
    pthread_mutex_t dummy = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&dummy);
    pthread_mutex_unlock(&dummy);
}
#endif
#endif

}



