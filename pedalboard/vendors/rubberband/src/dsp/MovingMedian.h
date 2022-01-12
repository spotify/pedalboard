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

#ifndef RUBBERBAND_MOVING_MEDIAN_H
#define RUBBERBAND_MOVING_MEDIAN_H

#include "SampleFilter.h"

#include "../system/Allocators.h"

#include <algorithm>

#include <iostream>

namespace RubberBand
{

template <typename T>
class MovingMedian : public SampleFilter<T>
{
    typedef SampleFilter<T> P;

public:
    MovingMedian(int size, float percentile = 50.f) :
        SampleFilter<T>(size),
	m_frame(allocate_and_zero<T>(size)),
	m_sorted(allocate_and_zero<T>(size)),
	m_sortend(m_sorted + P::m_size - 1) {
        setPercentile(percentile);
    }

    ~MovingMedian() { 
	deallocate(m_frame);
	deallocate(m_sorted);
    }

    void setPercentile(float p) {
        m_index = int((P::m_size * p) / 100.f);
        if (m_index >= P::m_size) m_index = P::m_size-1;
        if (m_index < 0) m_index = 0;
    }

    void push(T value) {
        if (value != value) {
            std::cerr << "WARNING: MovingMedian: NaN encountered" << std::endl;
            value = T();
        }
	drop(m_frame[0]);
	v_move(m_frame, m_frame+1, P::m_size-1);
	m_frame[P::m_size-1] = value;
	put(value);
    }

    T get() const {
	return m_sorted[m_index];
    }

    void reset() {
	v_zero(m_frame, P::m_size);
	v_zero(m_sorted, P::m_size);
    }

private:
    T *const m_frame;
    T *const m_sorted;
    T *const m_sortend;
    int m_index;

    void put(T value) {
	// precondition: m_sorted contains m_size-1 values, packed at start
	// postcondition: m_sorted contains m_size values, one of which is value
	T *index = std::lower_bound(m_sorted, m_sortend, value);
	v_move(index + 1, index, m_sortend - index);
	*index = value;
    }

    void drop(T value) {
	// precondition: m_sorted contains m_size values, one of which is value
	// postcondition: m_sorted contains m_size-1 values, packed at start
	T *index = std::lower_bound(m_sorted, m_sortend + 1, value);
	assert(*index == value);
	v_move(index, index + 1, m_sortend - index);
	*m_sortend = T(0);
    }
};

}

#endif

