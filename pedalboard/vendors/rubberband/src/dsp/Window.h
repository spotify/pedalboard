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

#ifndef RUBBERBAND_WINDOW_H
#define RUBBERBAND_WINDOW_H

#include <cmath>
#include <cstdlib>
#include <map>

#include "../system/sysutils.h"
#include "../system/VectorOps.h"
#include "../system/Allocators.h"

namespace RubberBand {

enum WindowType {
    RectangularWindow,
    BartlettWindow,
    HammingWindow,
    HanningWindow,
    BlackmanWindow,
    GaussianWindow,
    ParzenWindow,
    NuttallWindow,
    BlackmanHarrisWindow
};

template <typename T>
class Window
{
public:
    /**
     * Construct a windower of the given type.
     */
    Window(WindowType type, int size) : m_type(type), m_size(size), m_cache(0) {
        encache();
    }
    Window(const Window &w) : m_type(w.m_type), m_size(w.m_size), m_cache(0) {
        encache();
    }
    Window &operator=(const Window &w) {
	if (&w == this) return *this;
	m_type = w.m_type;
	m_size = w.m_size;
        m_cache = 0;
	encache();
	return *this;
    }
    virtual ~Window() {
        deallocate(m_cache);
    }
    
    inline void cut(T *const R__ block) const {
        v_multiply(block, m_cache, m_size);
    }

    inline void cut(const T *const R__ src, T *const R__ dst) const {
        v_multiply(dst, src, m_cache, m_size);
    }

    inline void add(T *const R__ dst, T scale) const {
        v_add_with_gain(dst, m_cache, scale, m_size);
    }

    inline T getRMS() const {
        T total = 0;
        for (int i = 0; i < m_size; ++i) {
            total += m_cache[i] * m_cache[i];
        }
        T rms = sqrt(total / m_size);
        return rms;
    }

    inline T getArea() const { return m_area; }
    inline T getValue(int i) const { return m_cache[i]; }

    inline WindowType getType() const { return m_type; }
    inline int getSize() const { return m_size; }

protected:
    WindowType m_type;
    int m_size;
    T *R__ m_cache;
    T m_area;
    
    void encache();
    void cosinewin(T *, T, T, T, T);
};

template <typename T>
void Window<T>::encache()
{
    if (!m_cache) m_cache = allocate<T>(m_size);

    const int n = m_size;
    v_set(m_cache, T(1.0), n);
    int i;

    switch (m_type) {
		
    case RectangularWindow:
	for (i = 0; i < n; ++i) {
	    m_cache[i] *= 0.5;
	}
	break;
	    
    case BartlettWindow:
	for (i = 0; i < n/2; ++i) {
	    m_cache[i] *= (i / T(n/2));
	    m_cache[i + n/2] *= (1.0 - (i / T(n/2)));
	}
	break;
	    
    case HammingWindow:
        cosinewin(m_cache, 0.54, 0.46, 0.0, 0.0);
	break;
	    
    case HanningWindow:
        cosinewin(m_cache, 0.50, 0.50, 0.0, 0.0);
	break;
	    
    case BlackmanWindow:
        cosinewin(m_cache, 0.42, 0.50, 0.08, 0.0);
	break;
	    
    case GaussianWindow:
	for (i = 0; i < n; ++i) {
            m_cache[i] *= pow(2, - pow((i - (n-1)/2.0) / ((n-1)/2.0 / 3), 2));
	}
	break;
	    
    case ParzenWindow:
    {
        int N = n-1;
        for (i = 0; i < N/4; ++i) {
            T m = 2 * pow(1.0 - (T(N)/2 - i) / (T(N)/2), 3);
            m_cache[i] *= m;
            m_cache[N-i] *= m;
        }
        for (i = N/4; i <= N/2; ++i) {
            int wn = i - N/2;
            T m = 1.0 - 6 * pow(wn / (T(N)/2), 2) * (1.0 - abs(wn) / (T(N)/2));
            m_cache[i] *= m;
            m_cache[N-i] *= m;
        }            
        break;
    }

    case NuttallWindow:
        cosinewin(m_cache, 0.3635819, 0.4891775, 0.1365995, 0.0106411);
	break;

    case BlackmanHarrisWindow:
        cosinewin(m_cache, 0.35875, 0.48829, 0.14128, 0.01168);
        break;
    }
	
    m_area = 0;
    for (i = 0; i < n; ++i) {
        m_area += m_cache[i];
    }
    m_area /= n;
}

template <typename T>
void Window<T>::cosinewin(T *mult, T a0, T a1, T a2, T a3)
{
    int n = int(m_size);
    for (int i = 0; i < n; ++i) {
        mult[i] *= (a0
                    - a1 * cos(2 * M_PI * i / n)
                    + a2 * cos(4 * M_PI * i / n)
                    - a3 * cos(6 * M_PI * i / n));
    }
}

}

#endif
