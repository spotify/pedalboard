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

#include "Profiler.h"

#include "../system/Thread.h"

#include <algorithm>
#include <set>
#include <string>
#include <map>

#include <stdio.h>

#ifdef _MSC_VER
// Ugh --cc
#define snprintf sprintf_s
#endif

namespace RubberBand {

#ifndef NO_TIMING

Profiler::ProfileMap
Profiler::m_profiles;

Profiler::WorstCallMap
Profiler::m_worstCalls;

static Mutex profileMutex;

void
Profiler::add(const char *id, float ms)
{
    profileMutex.lock();
    
    ProfileMap::iterator pmi = m_profiles.find(id);
    if (pmi != m_profiles.end()) {
        ++pmi->second.first;
        pmi->second.second += ms;
    } else {
        m_profiles[id] = TimePair(1, ms);
    }

    WorstCallMap::iterator wci = m_worstCalls.find(id);
    if (wci != m_worstCalls.end()) {
        if (ms > wci->second) wci->second = ms;
    } else {
        m_worstCalls[id] = ms;
    }

    profileMutex.unlock();
}

void
Profiler::dump()
{
    std::string report = getReport();
    fprintf(stderr, "%s", report.c_str());
}

std::string
Profiler::getReport()
{
    profileMutex.lock();
    
    static const int buflen = 256;
    char buffer[buflen];
    std::string report;

#ifdef PROFILE_CLOCKS
    snprintf(buffer, buflen, "Profiling points [CPU time]:\n");
#else
    snprintf(buffer, buflen, "Profiling points [Wall time]:\n");
#endif
    report += buffer;

    typedef std::multimap<float, const char *> TimeRMap;
    typedef std::multimap<int, const char *> IntRMap;
    TimeRMap totmap, avgmap, worstmap;
    IntRMap ncallmap;

    for (ProfileMap::const_iterator i = m_profiles.begin();
         i != m_profiles.end(); ++i) {
        totmap.insert(TimeRMap::value_type(i->second.second, i->first));
        avgmap.insert(TimeRMap::value_type(i->second.second /
                                           i->second.first, i->first));
        ncallmap.insert(IntRMap::value_type(i->second.first, i->first));
    }

    for (WorstCallMap::const_iterator i = m_worstCalls.begin();
         i != m_worstCalls.end(); ++i) {
        worstmap.insert(TimeRMap::value_type(i->second, i->first));
    }

    snprintf(buffer, buflen, "\nBy total:\n");
    report += buffer;
    for (TimeRMap::const_iterator i = totmap.end(); i != totmap.begin(); ) {
        --i;
        snprintf(buffer, buflen, "%-40s  %f ms\n", i->second, i->first);
        report += buffer;
    }

    snprintf(buffer, buflen, "\nBy average:\n");
    report += buffer;
    for (TimeRMap::const_iterator i = avgmap.end(); i != avgmap.begin(); ) {
        --i;
        snprintf(buffer, buflen, "%-40s  %f ms\n", i->second, i->first);
        report += buffer;
    }

    snprintf(buffer, buflen, "\nBy worst case:\n");
    report += buffer;
    for (TimeRMap::const_iterator i = worstmap.end(); i != worstmap.begin(); ) {
        --i;
        snprintf(buffer, buflen, "%-40s  %f ms\n", i->second, i->first);
        report += buffer;
    }

    snprintf(buffer, buflen, "\nBy number of calls:\n");
    report += buffer;
    for (IntRMap::const_iterator i = ncallmap.end(); i != ncallmap.begin(); ) {
        --i;
        snprintf(buffer, buflen, "%-40s  %d\n", i->second, i->first);
        report += buffer;
    }

    snprintf(buffer, buflen, "\nBy name:\n");
    report += buffer;

    typedef std::set<const char *, std::less<std::string> > StringSet;

    StringSet profileNames;
    for (ProfileMap::const_iterator i = m_profiles.begin();
         i != m_profiles.end(); ++i) {
        profileNames.insert(i->first);
    }

    for (StringSet::const_iterator i = profileNames.begin();
         i != profileNames.end(); ++i) {

        ProfileMap::const_iterator j = m_profiles.find(*i);
        if (j == m_profiles.end()) continue;

        const TimePair &pp(j->second);
        snprintf(buffer, buflen, "%s(%d):\n", *i, pp.first);
        report += buffer;
        snprintf(buffer, buflen, "\tReal: \t%f ms      \t[%f ms total]\n",
                (pp.second / pp.first),
                (pp.second));
        report += buffer;

        WorstCallMap::const_iterator k = m_worstCalls.find(*i);
        if (k == m_worstCalls.end()) continue;
        
        snprintf(buffer, buflen, "\tWorst:\t%f ms/call\n", k->second);
        report += buffer;
    }

    profileMutex.unlock();
    
    return report;
}

Profiler::Profiler(const char* c) :
    m_c(c),
    m_ended(false)
{
#ifdef PROFILE_CLOCKS
    m_start = clock();
#else
    (void)gettimeofday(&m_start, 0);
#endif
}

Profiler::~Profiler()
{
    if (!m_ended) end();
}

void
Profiler::end()
{
#ifdef PROFILE_CLOCKS
    clock_t end = clock();
    clock_t elapsed = end - m_start;
    float ms = float((double(elapsed) / double(CLOCKS_PER_SEC)) * 1000.0);
#else
    struct timeval tv;
    (void)gettimeofday(&tv, 0);

    tv.tv_sec -= m_start.tv_sec;
    if (tv.tv_usec < m_start.tv_usec) {
        tv.tv_usec += 1000000;
        tv.tv_sec -= 1;
    }
    tv.tv_usec -= m_start.tv_usec;
    float ms = float((double(tv.tv_sec) + (double(tv.tv_usec) / 1000000.0)) * 1000.0);
#endif

    add(m_c, ms);

    m_ended = true;
}
 
#else /* NO_TIMING */

#ifndef NO_TIMING_COMPLETE_NOOP

Profiler::Profiler(const char *) { }
Profiler::~Profiler() { }
void Profiler::end() { }
void Profiler::dump() { }

#endif

#endif

}
