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

#ifndef RUBBERBAND_RINGBUFFER_H
#define RUBBERBAND_RINGBUFFER_H

#include <sys/types.h>

//#define DEBUG_RINGBUFFER 1

#include "../system/sysutils.h"
#include "../system/Allocators.h"

#include <iostream>

#include <atomic>

namespace RubberBand {

/**
 * RingBuffer implements a lock-free ring buffer for one writer and
 * one reader, that is to be used to store a sample type T.
 *
 * RingBuffer is thread-safe provided only one thread writes and only
 * one thread reads.
 */

template <typename T>
class RingBuffer
{
public:
    /**
     * Create a ring buffer with room to write n samples.
     *
     * Note that the internal storage size will actually be n+1
     * samples, as one element is unavailable for administrative
     * reasons.  Since the ring buffer performs best if its size is a
     * power of two, this means n should ideally be some power of two
     * minus one.
     */
    RingBuffer(int n);

    virtual ~RingBuffer();

    /**
     * Return the total capacity of the ring buffer in samples.
     * (This is the argument n passed to the constructor.)
     */
    int getSize() const;

    /**
     * Return a new ring buffer (allocated with "new" -- caller must
     * delete when no longer needed) of the given size, containing the
     * same data as this one.  If another thread reads from or writes
     * to this buffer during the call, the results may be incomplete
     * or inconsistent.  If this buffer's data will not fit in the new
     * size, the contents are undefined.
     */
    RingBuffer<T> *resized(int newSize) const;

    /**
     * Lock the ring buffer into physical memory.  Returns true
     * for success.
     */
    bool mlock();

    /**
     * Reset read and write pointers, thus emptying the buffer.
     * Should be called from the write thread.
     */
    void reset();

    /**
     * Return the amount of data available for reading, in samples.
     */
    int getReadSpace() const;

    /**
     * Return the amount of space available for writing, in samples.
     */
    int getWriteSpace() const;

    /**
     * Read n samples from the buffer.  If fewer than n are available,
     * the remainder will be zeroed out.  Returns the number of
     * samples actually read.
     *
     * This is a template function, taking an argument S for the target
     * sample type, which is permitted to differ from T if the two
     * types are compatible for arithmetic operations.
     */
    template <typename S>
    int read(S *const R__ destination, int n);

    /**
     * Read n samples from the buffer, adding them to the destination.
     * If fewer than n are available, the remainder will be left
     * alone.  Returns the number of samples actually read.
     *
     * This is a template function, taking an argument S for the target
     * sample type, which is permitted to differ from T if the two
     * types are compatible for arithmetic operations.
     */
    template <typename S>
    int readAdding(S *const R__ destination, int n);

    /**
     * Read one sample from the buffer.  If no sample is available,
     * this will silently return zero.  Calling this repeatedly is
     * obviously slower than calling read once, but it may be good
     * enough if you don't want to allocate a buffer to read into.
     */
    T readOne();

    /**
     * Read n samples from the buffer, if available, without advancing
     * the read pointer -- i.e. a subsequent read() or skip() will be
     * necessary to empty the buffer.  If fewer than n are available,
     * the remainder will be zeroed out.  Returns the number of
     * samples actually read.
     */
    int peek(T *const R__ destination, int n) const;

    /**
     * Read one sample from the buffer, if available, without
     * advancing the read pointer -- i.e. a subsequent read() or
     * skip() will be necessary to empty the buffer.  Returns zero if
     * no sample was available.
     */
    T peekOne() const;

    /**
     * Pretend to read n samples from the buffer, without actually
     * returning them (i.e. discard the next n samples).  Returns the
     * number of samples actually available for discarding.
     */
    int skip(int n);

    /**
     * Write n samples to the buffer.  If insufficient space is
     * available, not all samples may actually be written.  Returns
     * the number of samples actually written.
     *
     * This is a template function, taking an argument S for the source
     * sample type, which is permitted to differ from T if the two
     * types are compatible for assignment.
     */
    template <typename S>
    int write(const S *const R__ source, int n);

    /**
     * Write n zero-value samples to the buffer.  If insufficient
     * space is available, not all zeros may actually be written.
     * Returns the number of zeroes actually written.
     */
    int zero(int n);

protected:
    T *const R__      m_buffer;
    std::atomic<int>  m_writer;
    std::atomic<int>  m_reader;
    const int         m_size;
    bool              m_mlocked;

    int readSpaceFor(int w, int r) const {
        int space;
        if (w > r) space = w - r;
        else if (w < r) space = (w + m_size) - r;
        else space = 0;
        return space;
    }

    int writeSpaceFor(int w, int r) const {
        int space = (r + m_size - w - 1);
        if (space >= m_size) space -= m_size;
        return space;
    }

private:
    RingBuffer(const RingBuffer &); // not provided
    RingBuffer &operator=(const RingBuffer &); // not provided
};

template <typename T>
RingBuffer<T>::RingBuffer(int n) :
    m_buffer(allocate<T>(n + 1)),
    m_writer(0),
    m_size(n + 1),
    m_mlocked(false)
{
#ifdef DEBUG_RINGBUFFER
    std::cerr << "RingBuffer<T>[" << this << "]::RingBuffer(" << n << ")" << std::endl;
#endif

    m_reader = 0;
}

template <typename T>
RingBuffer<T>::~RingBuffer()
{
#ifdef DEBUG_RINGBUFFER
    std::cerr << "RingBuffer<T>[" << this << "]::~RingBuffer" << std::endl;
#endif

    if (m_mlocked) {
	MUNLOCK((void *)m_buffer, m_size * sizeof(T));
    }

    deallocate(m_buffer);
}

template <typename T>
int
RingBuffer<T>::getSize() const
{
#ifdef DEBUG_RINGBUFFER
    std::cerr << "RingBuffer<T>[" << this << "]::getSize(): " << m_size-1 << std::endl;
#endif

    return m_size - 1;
}

template <typename T>
RingBuffer<T> *
RingBuffer<T>::resized(int newSize) const
{
    RingBuffer<T> *newBuffer = new RingBuffer<T>(newSize);
    
    MBARRIER();
    int w = m_writer;
    int r = m_reader;

    while (r != w) {
        T value = m_buffer[r];
        newBuffer->write(&value, 1);
        if (++r == m_size) r = 0;
    }

    return newBuffer;
}

template <typename T>
bool
RingBuffer<T>::mlock()
{
    if (MLOCK((void *)m_buffer, m_size * sizeof(T))) return false;
    m_mlocked = true;
    return true;
}

template <typename T>
void
RingBuffer<T>::reset()
{
#ifdef DEBUG_RINGBUFFER
    std::cerr << "RingBuffer<T>[" << this << "]::reset" << std::endl;
#endif

    int r = m_reader;
    m_writer = r;
}

template <typename T>
int
RingBuffer<T>::getReadSpace() const
{
    return readSpaceFor(m_writer, m_reader);
}

template <typename T>
int
RingBuffer<T>::getWriteSpace() const
{
    return writeSpaceFor(m_writer, m_reader);
}

template <typename T>
template <typename S>
int
RingBuffer<T>::read(S *const R__ destination, int n)
{
    int w = m_writer;
    int r = m_reader;

    int available = readSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::read: " << n << " requested, only "
                  << available << " available" << std::endl;
	n = available;
    }
    if (n == 0) return n;

    int here = m_size - r;
    T *const R__ bufbase = m_buffer + r;

    if (here >= n) {
        v_convert(destination, bufbase, n);
    } else {
        v_convert(destination, bufbase, here);
        v_convert(destination + here, m_buffer, n - here);
    }

    r += n;
    while (r >= m_size) r -= m_size;

    m_reader = r;

    return n;
}

template <typename T>
template <typename S>
int
RingBuffer<T>::readAdding(S *const R__ destination, int n)
{
    int w = m_writer;
    int r = m_reader;

    int available = readSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::read: " << n << " requested, only "
                  << available << " available" << std::endl;
	n = available;
    }
    if (n == 0) return n;

    int here = m_size - r;
    T *const R__ bufbase = m_buffer + r;

    if (here >= n) {
        v_add(destination, bufbase, n);
    } else {
        v_add(destination, bufbase, here);
        v_add(destination + here, m_buffer, n - here);
    }

    r += n;
    while (r >= m_size) r -= m_size;

    m_reader = r;

    return n;
}

template <typename T>
T
RingBuffer<T>::readOne()
{
    int w = m_writer;
    int r = m_reader;

    if (w == r) {
	std::cerr << "WARNING: RingBuffer::readOne: no sample available"
		  << std::endl;
	return T();
    }

    T value = m_buffer[r];
    if (++r == m_size) r = 0;

    m_reader = r;

    return value;
}

template <typename T>
int
RingBuffer<T>::peek(T *const R__ destination, int n) const
{
    int w = m_writer;
    int r = m_reader;

    int available = readSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::peek: " << n << " requested, only "
                  << available << " available" << std::endl;
	memset(destination + available, 0, (n - available) * sizeof(T));
	n = available;
    }
    if (n == 0) return n;

    int here = m_size - r;
    const T *const R__ bufbase = m_buffer + r;

    if (here >= n) {
        v_copy(destination, bufbase, n);
    } else {
        v_copy(destination, bufbase, here);
        v_copy(destination + here, m_buffer, n - here);
    }

    return n;
}

template <typename T>
T
RingBuffer<T>::peekOne() const
{
    int w = m_writer;
    int r = m_reader;

    if (w == r) {
	std::cerr << "WARNING: RingBuffer::peekOne: no sample available"
		  << std::endl;
	return 0;
    }

    T value = m_buffer[r];
    return value;
}

template <typename T>
int
RingBuffer<T>::skip(int n)
{
    int w = m_writer;
    int r = m_reader;

    int available = readSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::skip: " << n << " requested, only "
                  << available << " available" << std::endl;
	n = available;
    }
    if (n == 0) return n;

    r += n;
    while (r >= m_size) r -= m_size;

    m_reader = r;

    return n;
}

template <typename T>
template <typename S>
int
RingBuffer<T>::write(const S *const R__ source, int n)
{
    int w = m_writer;
    int r = m_reader;

    int available = writeSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::write: " << n
                  << " requested, only room for " << available << std::endl;
	n = available;
    }
    if (n == 0) return n;

    int here = m_size - w;
    T *const R__ bufbase = m_buffer + w;

    if (here >= n) {
        v_convert<S, T>(bufbase, source, n);
    } else {
        v_convert<S, T>(bufbase, source, here);
        v_convert<S, T>(m_buffer, source + here, n - here);
    }

    w += n;
    while (w >= m_size) w -= m_size;

    MBARRIER();
    m_writer = w;

    return n;
}

template <typename T>
int
RingBuffer<T>::zero(int n)
{
    int w = m_writer;
    int r = m_reader;

    int available = writeSpaceFor(w, r);
    if (n > available) {
	std::cerr << "WARNING: RingBuffer::zero: " << n
                  << " requested, only room for " << available << std::endl;
	n = available;
    }
    if (n == 0) return n;

    int here = m_size - w;
    T *const R__ bufbase = m_buffer + w;

    if (here >= n) {
        v_zero(bufbase, n);
    } else {
        v_zero(bufbase, here);
        v_zero(m_buffer, n - here);
    }

    w += n;
    while (w >= m_size) w -= m_size;

    MBARRIER();
    m_writer = w;

    return n;
}

}

#endif // _RINGBUFFER_H
