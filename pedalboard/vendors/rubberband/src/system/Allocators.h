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

#ifndef RUBBERBAND_ALLOCATORS_H
#define RUBBERBAND_ALLOCATORS_H

#include "VectorOps.h"

#include <new> // for std::bad_alloc
#include <stdlib.h>

#include <stdexcept>

#ifndef HAVE_POSIX_MEMALIGN
#ifndef _WIN32
#ifndef __APPLE__
#ifndef LACK_POSIX_MEMALIGN
#define HAVE_POSIX_MEMALIGN
#endif
#endif
#endif
#endif

#ifndef MALLOC_IS_ALIGNED
#ifndef MALLOC_IS_NOT_ALIGNED
#ifdef __APPLE__
#define MALLOC_IS_ALIGNED
#endif
#endif
#endif

#ifndef HAVE__ALIGNED_MALLOC
#ifndef LACK__ALIGNED_MALLOC
#ifdef _WIN32
#define HAVE__ALIGNED_MALLOC
#endif
#endif
#endif

#ifdef HAVE_POSIX_MEMALIGN
#include <sys/mman.h>
#include <errno.h>
#endif

#ifdef LACK_BAD_ALLOC
namespace std { struct bad_alloc { }; }
#endif

namespace RubberBand {

template <typename T>
T *allocate(size_t count)
{
    void *ptr = 0;

    // We'd like to check HAVE_IPP first and, if it's defined, call
    // ippsMalloc_8u(count * sizeof(T)). But that isn't a general
    // replacement for malloc() because it only takes an int
    // argument. So we save it for the specialisations of
    // allocate<float> and allocate<double> below, where we're more
    // likely to get away with it.

#ifdef MALLOC_IS_ALIGNED
    ptr = malloc(count * sizeof(T));
#else /* !MALLOC_IS_ALIGNED */

    // That's the "sufficiently aligned" functions dealt with, the
    // rest need a specific alignment provided to the call. 32-byte
    // alignment is required for at least OpenMAX
    static const int alignment = 32;

#ifdef HAVE__ALIGNED_MALLOC
    ptr = _aligned_malloc(count * sizeof(T), alignment);
#else /* !HAVE__ALIGNED_MALLOC */

#ifdef HAVE_POSIX_MEMALIGN
    int rv = posix_memalign(&ptr, alignment, count * sizeof(T));
    if (rv) {
#ifndef NO_EXCEPTIONS
        if (rv == EINVAL) {
            throw "Internal error: invalid alignment";
        } else {
            throw std::bad_alloc();
        }
#else
        abort();
#endif
    }
#else /* !HAVE_POSIX_MEMALIGN */
    
#ifdef USE_OWN_ALIGNED_MALLOC
#pragma message("Rolling own aligned malloc: this is unlikely to perform as well as the alternatives")

    // Alignment must be a power of two, bigger than the pointer
    // size. Stuff the actual malloc'd pointer in just before the
    // returned value.  This is the least desirable way to do this --
    // the other options below are all better
    size_t allocd = count * sizeof(T) + alignment;
    void *buf = malloc(allocd);
    if (buf) {
        char *adj = (char *)buf;
        while ((unsigned long long)adj & (alignment-1)) --adj;
        ptr = ((char *)adj) + alignment;
        new (((void **)ptr)[-1]) (void *);
        ((void **)ptr)[-1] = buf;
    }

#else /* !USE_OWN_ALIGNED_MALLOC */

#error "No aligned malloc available: define MALLOC_IS_ALIGNED to use system malloc, HAVE_POSIX_MEMALIGN if posix_memalign is available, HAVE__ALIGNED_MALLOC if _aligned_malloc is available, or USE_OWN_ALIGNED_MALLOC to roll our own"

#endif /* !USE_OWN_ALIGNED_MALLOC */
#endif /* !HAVE_POSIX_MEMALIGN */
#endif /* !HAVE__ALIGNED_MALLOC */
#endif /* !MALLOC_IS_ALIGNED */

    if (!ptr) {
#ifndef NO_EXCEPTIONS
        throw std::bad_alloc();
#else
        abort();
#endif
    }

    T *typed_ptr = static_cast<T *>(ptr);
    for (size_t i = 0; i < count; ++i) {
        new (typed_ptr + i) T;
    }
    return typed_ptr;
}

#ifdef HAVE_IPP

template <>
float *allocate(size_t count);

template <>
double *allocate(size_t count);

#endif
	
template <typename T>
T *allocate_and_zero(size_t count)
{
    T *ptr = allocate<T>(count);
    v_zero(ptr, count);
    return ptr;
}

template <typename T>
void deallocate(T *ptr)
{
    if (!ptr) return;
    
#ifdef MALLOC_IS_ALIGNED
    free((void *)ptr);
#else /* !MALLOC_IS_ALIGNED */

#ifdef HAVE__ALIGNED_MALLOC
    _aligned_free((void *)ptr);
#else /* !HAVE__ALIGNED_MALLOC */

#ifdef HAVE_POSIX_MEMALIGN
    free((void *)ptr);
#else /* !HAVE_POSIX_MEMALIGN */
    
#ifdef USE_OWN_ALIGNED_MALLOC
    free(((void **)ptr)[-1]);
#else /* !USE_OWN_ALIGNED_MALLOC */

#error "No aligned malloc available: define MALLOC_IS_ALIGNED to use system malloc, HAVE_POSIX_MEMALIGN if posix_memalign is available, or USE_OWN_ALIGNED_MALLOC to roll our own"

#endif /* !USE_OWN_ALIGNED_MALLOC */
#endif /* !HAVE_POSIX_MEMALIGN */
#endif /* !HAVE__ALIGNED_MALLOC */
#endif /* !MALLOC_IS_ALIGNED */
}

#ifdef HAVE_IPP

template <>
void deallocate(float *);

template <>
void deallocate(double *);

#endif

/// Reallocate preserving contents but leaving additional memory uninitialised	
template <typename T>
T *reallocate(T *ptr, size_t oldcount, size_t count)
{
    T *newptr = allocate<T>(count);
    if (oldcount && ptr) {
        v_copy(newptr, ptr, oldcount < count ? oldcount : count);
    }
    if (ptr) deallocate<T>(ptr);
    return newptr;
}

/// Reallocate, zeroing all contents
template <typename T>
T *reallocate_and_zero(T *ptr, size_t oldcount, size_t count)
{
    ptr = reallocate(ptr, oldcount, count);
    v_zero(ptr, count);
    return ptr;
}
	
/// Reallocate preserving contents and zeroing any additional memory	
template <typename T>
T *reallocate_and_zero_extension(T *ptr, size_t oldcount, size_t count)
{
    ptr = reallocate(ptr, oldcount, count);
    if (count > oldcount) v_zero(ptr + oldcount, count - oldcount);
    return ptr;
}

template <typename T>
T **allocate_channels(size_t channels, size_t count)
{
    T **ptr = allocate<T *>(channels);
    for (size_t c = 0; c < channels; ++c) {
        ptr[c] = allocate<T>(count);
    }
    return ptr;
}
	
template <typename T>
T **allocate_and_zero_channels(size_t channels, size_t count)
{
    T **ptr = allocate<T *>(channels);
    for (size_t c = 0; c < channels; ++c) {
        ptr[c] = allocate_and_zero<T>(count);
    }
    return ptr;
}

template <typename T>
void deallocate_channels(T **ptr, size_t channels)
{
    if (!ptr) return;
    for (size_t c = 0; c < channels; ++c) {
        deallocate<T>(ptr[c]);
    }
    deallocate<T *>(ptr);
}
	
template <typename T>
T **reallocate_channels(T **ptr,
                        size_t oldchannels, size_t oldcount,
                        size_t channels, size_t count)
{
    T **newptr = allocate_channels<T>(channels, count);
    if (oldcount && ptr) {
        for (size_t c = 0; c < oldchannels && c < channels; ++c) {
            for (size_t i = 0; i < oldcount && i < count; ++i) {
                newptr[c][i] = ptr[c][i];
            }
        }
    } 
    if (ptr) deallocate_channels<T>(ptr, oldchannels);
    return newptr;
}
	
template <typename T>
T **reallocate_and_zero_extend_channels(T **ptr,
                                        size_t oldchannels, size_t oldcount,
                                        size_t channels, size_t count)
{
    T **newptr = allocate_and_zero_channels<T>(channels, count);
    if (oldcount && ptr) {
        for (size_t c = 0; c < oldchannels && c < channels; ++c) {
            for (size_t i = 0; i < oldcount && i < count; ++i) {
                newptr[c][i] = ptr[c][i];
            }
        }
    } 
    if (ptr) deallocate_channels<T>(ptr, oldchannels);
    return newptr;
}

/// RAII class to call deallocate() on destruction
template <typename T>
class Deallocator
{
public:
    Deallocator(T *t) : m_t(t) { }
    ~Deallocator() { deallocate<T>(m_t); }
private:
    T *m_t;
};

/** Allocator for use with STL classes, e.g. vector, to ensure
 *  alignment.  Based on example code by Stephan T. Lavavej.
 *
 *  e.g. std::vector<float, StlAllocator<float> > v;
 */
template <typename T>
class StlAllocator
{
public:
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    StlAllocator() { }
    StlAllocator(const StlAllocator&) { }
    template <typename U> StlAllocator(const StlAllocator<U>&) { }
    ~StlAllocator() { }

    T *
    allocate(const size_t n) const {
        if (n == 0) return 0;
        if (n > max_size()) {
#ifndef NO_EXCEPTIONS
            throw std::length_error("Size overflow in StlAllocator::allocate()");
#else
            abort();
#endif
        }
        return RubberBand::allocate<T>(n);
    }

    void
    deallocate(T *const p, const size_t) const {
        RubberBand::deallocate(p);
    }

    template <typename U>
    T *
    allocate(const size_t n, const U *) const {
        return allocate(n);
    }

    T *
    address(T &r) const {
        return &r;
    }

    const T *
    address(const T &s) const {
        return &s;
    }

    size_t
    max_size() const {
        return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
    }

    template <typename U> struct rebind {
        typedef StlAllocator<U> other;
    };
    
    bool
    operator==(const StlAllocator &) const {
        return true;
    }

    bool
    operator!=(const StlAllocator &) const {
        return false;
    }

    void
    construct(T *const p, const T &t) const {
        void *const pv = static_cast<void *>(p);
        new (pv) T(t);
    }

    void
    destroy(T *const p) const {
        p->~T();
    }

private:
    StlAllocator& operator=(const StlAllocator&);
};

}

#endif

