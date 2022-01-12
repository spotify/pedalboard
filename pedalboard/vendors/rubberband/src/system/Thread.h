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

#ifndef RUBBERBAND_THREAD_H
#define RUBBERBAND_THREAD_H

#include <string>

#ifndef NO_THREADING

#ifdef _WIN32
#include <windows.h>
#else /* !_WIN32 */
#ifdef USE_PTHREADS
#include <pthread.h>
#else /* !USE_PTHREADS */
#error No thread implementation selected
#endif /* !USE_PTHREADS */
#endif /* !_WIN32 */

//#define DEBUG_THREAD 1
//#define DEBUG_MUTEX 1
//#define DEBUG_CONDITION 1

namespace RubberBand
{

class Thread
{
public:
#ifdef _WIN32
    typedef HANDLE Id;
#else
#ifdef USE_PTHREADS
    typedef pthread_t Id;
#endif
#endif

    Thread();
    virtual ~Thread();

    Id id();

    void start();
    void wait();

    static bool threadingAvailable();

protected:
    virtual void run() = 0;

private:
#ifdef _WIN32
    HANDLE m_id;
    bool m_extant;
    static DWORD WINAPI staticRun(LPVOID lpParam);
#else
#ifdef USE_PTHREADS
    pthread_t m_id;
    bool m_extant;
    static void *staticRun(void *);
#endif
#endif
};

class Mutex
{
public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();
    bool trylock();

private:
#ifdef _WIN32
    HANDLE m_mutex;
#ifndef NO_THREAD_CHECKS
    DWORD m_lockedBy;
#endif
#else
#ifdef USE_PTHREADS
    pthread_mutex_t m_mutex;
#ifndef NO_THREAD_CHECKS
    pthread_t m_lockedBy;
    bool m_locked;
#endif
#endif
#endif
};

class MutexLocker
{
public:
    MutexLocker(Mutex *);
    ~MutexLocker();

private:
    Mutex *m_mutex;
};

/**
  The Condition class bundles a condition variable and mutex.

  To wait on a condition, call lock(), test the termination condition
  if desired, then wait().  The condition will be unlocked during the
  wait and re-locked when wait() returns (which will happen when the
  condition is signalled or the timer times out).

  To signal a condition, call signal().  If the condition is signalled
  between lock() and wait(), the signal may be missed by the waiting
  thread.  To avoid this, the signalling thread should also lock the
  condition before calling signal() and unlock it afterwards.
*/

class Condition
{
public:
    Condition(std::string name);
    ~Condition();
    
    void lock();
    void unlock();
    void wait(int us = 0);

    void signal();
    
private:

#ifdef _WIN32
    HANDLE m_mutex;
    HANDLE m_condition;
    bool m_locked;
#else
#ifdef USE_PTHREADS
    pthread_mutex_t m_mutex;
    pthread_cond_t m_condition;
    bool m_locked;
#endif
#endif
#ifdef DEBUG_CONDITION
    std::string m_name;
#endif
};

}

#else

/* Stub threading interface. We do not have threading support in this code. */

namespace RubberBand
{

class Thread
{
public:
    typedef unsigned int Id;

    Thread() { }
    virtual ~Thread() { }

    Id id() { return 0; }

    void start() { } 
    void wait() { }

    static bool threadingAvailable() { return false; }

protected:
    virtual void run() = 0;

private:
};

class Mutex
{
public:
    Mutex() { }
    ~Mutex() { }

    void lock() { }
    void unlock() { }
    bool trylock() { return false; }
};

class MutexLocker
{
public:
    MutexLocker(Mutex *) { }
    ~MutexLocker() { }
};

class Condition
{
public:
    Condition(std::string name) { }
    ~Condition() { }
    
    void lock() { }
    void unlock() { }
    void wait(int us = 0) { }

    void signal() { }
};

}

#endif /* NO_THREADING */

#endif
