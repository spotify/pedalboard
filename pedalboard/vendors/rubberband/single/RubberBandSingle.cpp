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

/*
    RubberBandSingle.cpp
 
    This is a single-file compilation unit for Rubber Band Library.
  
    To use the library in your project without building it separately,
    include in your code either rubberband/RubberBandStretcher.h for
    use in C++ or rubberband/rubberband-c.h if you need the C
    interface, then add this single C++ source file to your build.
 
    Don't move this file into your source tree - keep it in the same
    place relative to the other Rubber Band code, so that the relative
    include paths work, and just add it to your list of build files.
  
    This produces a build using the built-in FFT and resampler
    implementations, except on Apple platforms, where the vDSP FFT is
    used (and where you will need the Accelerate framework when
    linking). It should work correctly on any supported platform, but
    may not be the fastest configuration available. For further
    customisation, consider using the full build system to make a
    standalone library.
*/

#define USE_BQRESAMPLER 1

#define NO_TIMING 1
#define NO_THREADING 1
#define NO_THREAD_CHECKS 1

#if defined(__APPLE__)
#define HAVE_VDSP 1
#else
#define USE_BUILTIN_FFT 1
#endif

#include "../src/audiocurves/CompoundAudioCurve.cpp"
#include "../src/audiocurves/SpectralDifferenceAudioCurve.cpp"
#include "../src/audiocurves/HighFrequencyAudioCurve.cpp"
#include "../src/audiocurves/SilentAudioCurve.cpp"
#include "../src/audiocurves/ConstantAudioCurve.cpp"
#include "../src/audiocurves/PercussiveAudioCurve.cpp"
#include "../src/base/Profiler.cpp"
#include "../src/dsp/AudioCurveCalculator.cpp"
#include "../src/dsp/FFT.cpp"
#include "../src/dsp/Resampler.cpp"
#include "../src/dsp/BQResampler.cpp"
#include "../src/system/Allocators.cpp"
#include "../src/system/sysutils.cpp"
#include "../src/system/Thread.cpp"
#include "../src/RubberBandStretcher.cpp"
#include "../src/StretchCalculator.cpp"
#include "../src/StretcherChannelData.cpp"
#include "../src/StretcherImpl.cpp"
#include "../src/StretcherProcess.cpp"

#include "../src/rubberband-c.cpp"

