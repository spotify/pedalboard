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

#include "StretcherImpl.h"

#include "audiocurves/PercussiveAudioCurve.h"
#include "audiocurves/HighFrequencyAudioCurve.h"
#include "audiocurves/SpectralDifferenceAudioCurve.h"
#include "audiocurves/SilentAudioCurve.h"
#include "audiocurves/ConstantAudioCurve.h"
#include "audiocurves/CompoundAudioCurve.h"

#include "dsp/Resampler.h"

#include "StretchCalculator.h"
#include "StretcherChannelData.h"

#include "base/Profiler.h"

#include "system/sysutils.h"

#include <cassert>
#include <cmath>
#include <set>
#include <map>
#include <algorithm>

using namespace RubberBand;

using std::cerr;
using std::endl;
using std::vector;
using std::map;
using std::set;
using std::max;
using std::min;

namespace RubberBand {

const size_t
RubberBandStretcher::Impl::m_defaultIncrement = 256;

const size_t
RubberBandStretcher::Impl::m_defaultFftSize = 2048;

int
RubberBandStretcher::Impl::m_defaultDebugLevel = 0;

static bool _initialised = false;

RubberBandStretcher::Impl::Impl(size_t sampleRate,
                                size_t channels,
                                Options options,
                                double initialTimeRatio,
                                double initialPitchScale) :
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_timeRatio(initialTimeRatio),
    m_pitchScale(initialPitchScale),
    m_fftSize(m_defaultFftSize),
    m_aWindowSize(m_defaultFftSize),
    m_sWindowSize(m_defaultFftSize),
    m_increment(m_defaultIncrement),
    m_outbufSize(m_defaultFftSize * 2),
    m_maxProcessSize(m_defaultFftSize),
    m_expectedInputDuration(0),
#ifndef NO_THREADING
    m_threaded(false),
#endif
    m_realtime(false),
    m_options(options),
    m_debugLevel(m_defaultDebugLevel),
    m_mode(JustCreated),
    m_awindow(0),
    m_afilter(0),
    m_swindow(0),
    m_studyFFT(0),
#ifndef NO_THREADING
    m_spaceAvailable("space"),
#endif
    m_inputDuration(0),
    m_detectorType(CompoundAudioCurve::CompoundDetector),
    m_silentHistory(0),
    m_lastProcessOutputIncrements(16),
    m_lastProcessPhaseResetDf(16),
    m_emergencyScavenger(10, 4),
    m_phaseResetAudioCurve(0),
    m_stretchAudioCurve(0),
    m_silentAudioCurve(0),
    m_stretchCalculator(0),
    m_freq0(600),
    m_freq1(1200),
    m_freq2(12000),
    m_baseFftSize(m_defaultFftSize)
{
    if (!_initialised) {
        system_specific_initialise();
        _initialised = true;
    }

    if (m_debugLevel > 0) {
        cerr << "RubberBandStretcher::Impl::Impl: rate = " << m_sampleRate << ", options = " << options << endl;
    }

    // Window size will vary according to the audio sample rate, but
    // we don't let it drop below the 48k default
    m_rateMultiple = float(m_sampleRate) / 48000.f;
//    if (m_rateMultiple < 1.f) m_rateMultiple = 1.f;
    m_baseFftSize = roundUp(int(m_defaultFftSize * m_rateMultiple));

    if ((options & OptionWindowShort) || (options & OptionWindowLong)) {
        if ((options & OptionWindowShort) && (options & OptionWindowLong)) {
            cerr << "RubberBandStretcher::Impl::Impl: Cannot specify OptionWindowLong and OptionWindowShort together; falling back to OptionWindowStandard" << endl;
        } else if (options & OptionWindowShort) {
            m_baseFftSize = m_baseFftSize / 2;
            if (m_debugLevel > 0) {
                cerr << "setting baseFftSize to " << m_baseFftSize << endl;
            }
        } else if (options & OptionWindowLong) {
            m_baseFftSize = m_baseFftSize * 2;
            if (m_debugLevel > 0) {
                cerr << "setting baseFftSize to " << m_baseFftSize << endl;
            }
        }
        m_fftSize = m_baseFftSize;
        m_aWindowSize = m_baseFftSize;
        m_sWindowSize = m_baseFftSize;
        m_outbufSize = m_sWindowSize * 2;
        m_maxProcessSize = m_aWindowSize;
    }

    if (m_options & OptionProcessRealTime) {

        m_realtime = true;

        if (!(m_options & OptionStretchPrecise)) {
            m_options |= OptionStretchPrecise;
        }
    }

#ifndef NO_THREADING
    if (m_channels > 1) {

        m_threaded = true;

        if (m_realtime) {
            m_threaded = false;
        } else if (m_options & OptionThreadingNever) {
            m_threaded = false;
        } else if (!(m_options & OptionThreadingAlways) &&
                   !system_is_multiprocessor()) {
            m_threaded = false;
        }

        if (m_threaded && m_debugLevel > 0) {
            cerr << "Going multithreaded..." << endl;
        }
    }
#endif

    configure();
}

RubberBandStretcher::Impl::~Impl()
{
#ifndef NO_THREADING
    if (m_threaded) {
        MutexLocker locker(&m_threadSetMutex);
        for (set<ProcessThread *>::iterator i = m_threadSet.begin();
             i != m_threadSet.end(); ++i) {
            if (m_debugLevel > 0) {
                cerr << "RubberBandStretcher::~RubberBandStretcher: joining (channel " << *i << ")" << endl;
            }
            (*i)->abandon();
            (*i)->wait();
            delete *i;
        }
    }
#endif

    for (size_t c = 0; c < m_channels; ++c) {
        delete m_channelData[c];
    }

    delete m_phaseResetAudioCurve;
    delete m_stretchAudioCurve;
    delete m_silentAudioCurve;
    delete m_stretchCalculator;
    delete m_studyFFT;

    for (map<size_t, Window<float> *>::iterator i = m_windows.begin();
         i != m_windows.end(); ++i) {
        delete i->second;
    }
    for (map<size_t, SincWindow<float> *>::iterator i = m_sincs.begin();
         i != m_sincs.end(); ++i) {
        delete i->second;
    }
}

void
RubberBandStretcher::Impl::reset()
{
#ifndef NO_THREADING
    if (m_threaded) {
        m_threadSetMutex.lock();
        for (set<ProcessThread *>::iterator i = m_threadSet.begin();
             i != m_threadSet.end(); ++i) {
            if (m_debugLevel > 0) {
                cerr << "RubberBandStretcher::~RubberBandStretcher: joining (channel " << *i << ")" << endl;
            }
            (*i)->abandon();
            (*i)->wait();
            delete *i;
        }
        m_threadSet.clear();
    }
#endif

    m_emergencyScavenger.scavenge();

    if (m_stretchCalculator) {
        m_stretchCalculator->reset();
    }

    for (size_t c = 0; c < m_channels; ++c) {
        m_channelData[c]->reset();
    }

    m_mode = JustCreated;
    if (m_phaseResetAudioCurve) m_phaseResetAudioCurve->reset();
    if (m_stretchAudioCurve) m_stretchAudioCurve->reset();
    if (m_silentAudioCurve) m_silentAudioCurve->reset();
    m_inputDuration = 0;
    m_silentHistory = 0;

#ifndef NO_THREADING
    if (m_threaded) m_threadSetMutex.unlock();
#endif

    reconfigure();
}

void
RubberBandStretcher::Impl::setTimeRatio(double ratio)
{
    if (!m_realtime) {
        if (m_mode == Studying || m_mode == Processing) {
            cerr << "RubberBandStretcher::Impl::setTimeRatio: Cannot set ratio while studying or processing in non-RT mode" << endl;
            return;
        }
    }

    if (ratio == m_timeRatio) return;
    m_timeRatio = ratio;

    reconfigure();
}

void
RubberBandStretcher::Impl::setPitchScale(double fs)
{
    if (!m_realtime) {
        if (m_mode == Studying || m_mode == Processing) {
            cerr << "RubberBandStretcher::Impl::setPitchScale: Cannot set ratio while studying or processing in non-RT mode" << endl;
            return;
        }
    }

    if (fs == m_pitchScale) return;
    
    bool was1 = (m_pitchScale == 1.f);
    bool rbs = resampleBeforeStretching();

    m_pitchScale = fs;

    reconfigure();

    if (!(m_options & OptionPitchHighConsistency) &&
        (was1 || resampleBeforeStretching() != rbs) &&
        m_pitchScale != 1.f) {
        
        // resampling mode has changed
        for (int c = 0; c < int(m_channels); ++c) {
            if (m_channelData[c]->resampler) {
                m_channelData[c]->resampler->reset();
            }
        }
    }
}

double
RubberBandStretcher::Impl::getTimeRatio() const
{
    return m_timeRatio;
}

double
RubberBandStretcher::Impl::getPitchScale() const
{
    return m_pitchScale;
}

void
RubberBandStretcher::Impl::setExpectedInputDuration(size_t samples)
{
    if (samples == m_expectedInputDuration) return;
    m_expectedInputDuration = samples;

    reconfigure();
}

void
RubberBandStretcher::Impl::setMaxProcessSize(size_t samples)
{
    if (samples <= m_maxProcessSize) return;
    m_maxProcessSize = samples;

    reconfigure();
}

void
RubberBandStretcher::Impl::setKeyFrameMap(const std::map<size_t, size_t> &
                                          mapping)
{
    if (m_realtime) {
        cerr << "RubberBandStretcher::Impl::setKeyFrameMap: Cannot specify key frame map in RT mode" << endl;
        return;
    }
    if (m_mode == Processing) {
        cerr << "RubberBandStretcher::Impl::setKeyFrameMap: Cannot specify key frame map after process() has begun" << endl;
        return;
    }

    if (m_stretchCalculator) {
        m_stretchCalculator->setKeyFrameMap(mapping);
    }
}

float
RubberBandStretcher::Impl::getFrequencyCutoff(int n) const
{
    switch (n) {
    case 0: return m_freq0;
    case 1: return m_freq1;
    case 2: return m_freq2;
    }
    return 0.f;
}

void
RubberBandStretcher::Impl::setFrequencyCutoff(int n, float f)
{
    switch (n) {
    case 0: m_freq0 = f; break;
    case 1: m_freq1 = f; break;
    case 2: m_freq2 = f; break;
    }
}

double
RubberBandStretcher::Impl::getEffectiveRatio() const
{
    // Returns the ratio that the internal time stretcher needs to
    // achieve, not the resulting duration ratio of the output (which
    // is simply m_timeRatio).

    // A frequency shift is achieved using an additional time shift,
    // followed by resampling back to the original time shift to
    // change the pitch.  Note that the resulting frequency change is
    // fixed, as it is effected by the resampler -- in contrast to
    // time shifting, which is variable aiming to place the majority
    // of the stretch or squash in low-interest regions of audio.

    return m_timeRatio * m_pitchScale;
}

size_t
RubberBandStretcher::Impl::roundUp(size_t value)
{
    if (!(value & (value - 1))) return value;
    int bits = 0;
    while (value) { ++bits; value >>= 1; }
    value = 1 << bits;
    return value;
}

void
RubberBandStretcher::Impl::calculateSizes()
{
    size_t inputIncrement = m_defaultIncrement;
    size_t windowSize = m_baseFftSize;
    size_t outputIncrement;

    if (m_pitchScale <= 0.0) {
        // This special case is likelier than one might hope, because
        // of naive initialisations in programs that set it from a
        // variable
        std::cerr << "RubberBandStretcher: WARNING: Pitch scale must be greater than zero!\nResetting it from " << m_pitchScale << " to the default of 1.0: no pitch change will occur" << std::endl;
        m_pitchScale = 1.0;
    }
    if (m_timeRatio <= 0.0) {
        // Likewise
        std::cerr << "RubberBandStretcher: WARNING: Time ratio must be greater than zero!\nResetting it from " << m_timeRatio << " to the default of 1.0: no time stretch will occur" << std::endl;
        m_timeRatio = 1.0;
    }

    double r = getEffectiveRatio();

    if (m_realtime) {

        if (r < 1) {
            
            bool rsb = (m_pitchScale < 1.0 && !resampleBeforeStretching());
            float windowIncrRatio = 4.5;
            if (r == 1.0) windowIncrRatio = 4;
            else if (rsb) windowIncrRatio = 4.5;
            else windowIncrRatio = 6;

            inputIncrement = int(windowSize / windowIncrRatio);
            outputIncrement = int(floor(inputIncrement * r));

            // Very long stretch or very low pitch shift
            if (outputIncrement < m_defaultIncrement / 4) {
                if (outputIncrement < 1) outputIncrement = 1;
                while (outputIncrement < m_defaultIncrement / 4 &&
                       windowSize < m_baseFftSize * 4) {
                    outputIncrement *= 2;
                    inputIncrement = lrint(ceil(outputIncrement / r));
                    windowSize = roundUp(lrint(ceil(inputIncrement * windowIncrRatio)));
                }
            }

        } else {

            bool rsb = (m_pitchScale > 1.0 && resampleBeforeStretching());
            float windowIncrRatio = 4.5;
            if (r == 1.0) windowIncrRatio = 4;
            else if (rsb) windowIncrRatio = 4.5;
            else windowIncrRatio = 8;

            outputIncrement = int(windowSize / windowIncrRatio);
            inputIncrement = int(outputIncrement / r);
            while (outputIncrement > 1024 * m_rateMultiple &&
                   inputIncrement > 1) {
                outputIncrement /= 2;
                inputIncrement = int(outputIncrement / r);
            }
            while (inputIncrement < 1) {
                outputIncrement *= 2;
                inputIncrement = int(outputIncrement / r);
            }
            size_t minwin = roundUp(lrint(outputIncrement * windowIncrRatio));
            if (windowSize < minwin) windowSize = minwin;

            if (rsb) {
//                cerr << "adjusting window size from " << windowSize;
                size_t newWindowSize = roundUp(lrint(windowSize / m_pitchScale));
                if (newWindowSize < 512) newWindowSize = 512;
                size_t div = windowSize / newWindowSize;
                if (inputIncrement > div && outputIncrement > div) {
                    inputIncrement /= div;
                    outputIncrement /= div;
                    windowSize /= div;
                }
//                cerr << " to " << windowSize << " (inputIncrement = " << inputIncrement << ", outputIncrement = " << outputIncrement << ")" << endl;
            }
        }

    } else {

        if (r < 1) {
            inputIncrement = windowSize / 4;
            while (inputIncrement >= 512) inputIncrement /= 2;
            outputIncrement = int(floor(inputIncrement * r));
            if (outputIncrement < 1) {
                outputIncrement = 1;
                inputIncrement = roundUp(lrint(ceil(outputIncrement / r)));
                windowSize = inputIncrement * 4;
            }
        } else {
            outputIncrement = windowSize / 6;
            inputIncrement = int(outputIncrement / r);
            while (outputIncrement > 1024 && inputIncrement > 1) {
                outputIncrement /= 2;
                inputIncrement = int(outputIncrement / r);
            }
            while (inputIncrement < 1) {
                outputIncrement *= 2;
                inputIncrement = int(outputIncrement / r);
            }
            windowSize = std::max(windowSize, roundUp(outputIncrement * 6));
            if (r > 5) while (windowSize < 8192) windowSize *= 2;
        }
    }

    if (m_expectedInputDuration > 0) {
        while (inputIncrement * 4 > m_expectedInputDuration &&
               inputIncrement > 1) {
            inputIncrement /= 2;
        }
    }

    // m_fftSize can be almost anything, but it can't be greater than
    // 4 * m_baseFftSize unless ratio is less than 1/1024.

    m_fftSize = windowSize;
    
    if (m_options & OptionSmoothingOn) {
        m_aWindowSize = windowSize * 2;
        m_sWindowSize = windowSize * 2;
    } else {
        m_aWindowSize = windowSize;
        m_sWindowSize = windowSize;
    }

    m_increment = inputIncrement;

    // When squashing, the greatest theoretically possible output
    // increment is the input increment.  When stretching adaptively
    // the sky's the limit in principle, but we expect
    // StretchCalculator to restrict itself to using no more than
    // twice the basic output increment (i.e. input increment times
    // ratio) for any chunk.

    if (m_debugLevel > 0) {
        cerr << "calculateSizes: time ratio = " << m_timeRatio << ", pitch scale = " << m_pitchScale << ", effective ratio = " << getEffectiveRatio() << endl;
        cerr << "calculateSizes: analysis window size = " << m_aWindowSize << ", synthesis window size = " << m_sWindowSize << ", fft size = " << m_fftSize << ", increment = " << m_increment << " (approx output increment = " << int(lrint(m_increment * getEffectiveRatio())) << ")" << endl;
    }

    if (std::max(m_aWindowSize, m_sWindowSize) > m_maxProcessSize) {
        m_maxProcessSize = std::max(m_aWindowSize, m_sWindowSize);
    }

    m_outbufSize =
        size_t
        (ceil(max
              (m_maxProcessSize / m_pitchScale,
               m_maxProcessSize * 2 * (m_timeRatio > 1.f ? m_timeRatio : 1.f))));

    if (m_realtime) {
        // This headroom is so as to try to avoid reallocation when
        // the pitch scale changes
        m_outbufSize = m_outbufSize * 16;
    } else {
#ifndef NO_THREADING
        if (m_threaded) {
            // This headroom is to permit the processing threads to
            // run ahead of the buffer output drainage; the exact
            // amount of headroom is a question of tuning rather than
            // results
            m_outbufSize = m_outbufSize * 16;
        }
#endif
    }

    if (m_debugLevel > 0) {
        cerr << "calculateSizes: outbuf size = " << m_outbufSize << endl;
    }
}

void
RubberBandStretcher::Impl::configure()
{
    if (m_debugLevel > 0) {
        std::cerr << "configure[" << this << "]: realtime = " << m_realtime << ", pitch scale = "
                  << m_pitchScale << ", channels = " << m_channels << std::endl;
    }

    size_t prevFftSize = m_fftSize;
    size_t prevAWindowSize = m_aWindowSize;
    size_t prevSWindowSize = m_sWindowSize;
    size_t prevOutbufSize = m_outbufSize;
    if (m_windows.empty()) {
        prevFftSize = 0;
        prevAWindowSize = 0;
        prevSWindowSize = 0;
        prevOutbufSize = 0;
    }

    calculateSizes();

    bool fftSizeChanged = (prevFftSize != m_fftSize);
    bool windowSizeChanged = ((prevAWindowSize != m_aWindowSize) ||
                              (prevSWindowSize != m_sWindowSize));
    bool outbufSizeChanged = (prevOutbufSize != m_outbufSize);

    // This function may be called at any time in non-RT mode, after a
    // parameter has changed.  It shouldn't be legal to call it after
    // processing has already begun.

    // This function is only called once (on construction) in RT
    // mode.  After that reconfigure() does the work in a hopefully
    // RT-safe way.

    set<size_t> windowSizes;
    if (m_realtime) {
        windowSizes.insert(m_baseFftSize);
        windowSizes.insert(m_baseFftSize / 2);
        windowSizes.insert(m_baseFftSize * 2);
//        windowSizes.insert(m_baseFftSize * 4);
    }
    windowSizes.insert(m_fftSize);
    windowSizes.insert(m_aWindowSize);
    windowSizes.insert(m_sWindowSize);

    if (windowSizeChanged) {

        for (set<size_t>::const_iterator i = windowSizes.begin();
             i != windowSizes.end(); ++i) {
            if (m_windows.find(*i) == m_windows.end()) {
                m_windows[*i] = new Window<float>(HanningWindow, *i);
            }
            if (m_sincs.find(*i) == m_sincs.end()) {
                m_sincs[*i] = new SincWindow<float>(*i, *i);
            }
        }
        m_awindow = m_windows[m_aWindowSize];
        m_afilter = m_sincs[m_aWindowSize];
        m_swindow = m_windows[m_sWindowSize];

        if (m_debugLevel > 0) {
            cerr << "Window area: " << m_awindow->getArea() << "; synthesis window area: " << m_swindow->getArea() << endl;
        }
    }

    if (windowSizeChanged || outbufSizeChanged) {
        
        for (size_t c = 0; c < m_channelData.size(); ++c) {
            delete m_channelData[c];
        }
        m_channelData.clear();

        for (size_t c = 0; c < m_channels; ++c) {
            m_channelData.push_back
                (new ChannelData(windowSizes,
                                 std::max(m_aWindowSize, m_sWindowSize),
                                 m_fftSize,
                                 m_outbufSize));
        }
    }

    if (!m_realtime && fftSizeChanged) {
        delete m_studyFFT;
        m_studyFFT = new FFT(m_fftSize, m_debugLevel);
        m_studyFFT->initFloat();
    }

    if (m_pitchScale != 1.0 ||
        (m_options & OptionPitchHighConsistency) ||
        m_realtime) {

        for (size_t c = 0; c < m_channels; ++c) {

            if (m_channelData[c]->resampler) continue;

            Resampler::Parameters params;
            params.quality = Resampler::FastestTolerable;

            if (m_realtime) {
                params.dynamism = Resampler::RatioOftenChanging;
                params.ratioChange = Resampler::SmoothRatioChange;
            } else {
                // ratio can't be changed in offline mode
                params.dynamism = Resampler::RatioMostlyFixed;
                params.ratioChange = Resampler::SuddenRatioChange;
            }
            
            params.maxBufferSize = 4096 * 16;
            params.debugLevel = (m_debugLevel > 0 ? m_debugLevel-1 : 0);
            
            m_channelData[c]->resampler = new Resampler(params, 1);

            // rbs is the amount of buffer space we think we'll need
            // for resampling; but allocate a sensible amount in case
            // the pitch scale changes during use
            size_t rbs = 
                lrintf(ceil((m_increment * m_timeRatio * 2) / m_pitchScale));
            if (rbs < m_increment * 16) rbs = m_increment * 16;
            m_channelData[c]->setResampleBufSize(rbs);
        }
    }
    
    // stretchAudioCurve is unused in RT mode; phaseResetAudioCurve,
    // silentAudioCurve and stretchCalculator however are used in all
    // modes

    delete m_phaseResetAudioCurve;
    m_phaseResetAudioCurve = new CompoundAudioCurve
        (CompoundAudioCurve::Parameters(m_sampleRate, m_fftSize));
    m_phaseResetAudioCurve->setType(m_detectorType);

    delete m_silentAudioCurve;
    m_silentAudioCurve = new SilentAudioCurve
        (SilentAudioCurve::Parameters(m_sampleRate, m_fftSize));

    if (!m_realtime) {
        delete m_stretchAudioCurve;
        if (!(m_options & OptionStretchPrecise)) {
            m_stretchAudioCurve = new SpectralDifferenceAudioCurve
                (SpectralDifferenceAudioCurve::Parameters(m_sampleRate, m_fftSize));
        } else {
            m_stretchAudioCurve = new ConstantAudioCurve
                (ConstantAudioCurve::Parameters(m_sampleRate, m_fftSize));
        }
    }

    delete m_stretchCalculator;
    m_stretchCalculator = new StretchCalculator
        (m_sampleRate, m_increment,
         !(m_options & OptionTransientsSmooth));

    m_stretchCalculator->setDebugLevel(m_debugLevel);
    m_inputDuration = 0;

    // Prepare the inbufs with half a chunk of emptiness.  The centre
    // point of the first processing chunk for the onset detector
    // should be the first sample of the audio, and we continue until
    // we can no longer centre a chunk within the input audio.  The
    // number of onset detector chunks will be the number of audio
    // samples input, divided by the input increment, plus one.

    // In real-time mode, we don't do this prefill -- it's better to
    // start with a swoosh than introduce more latency, and we don't
    // want gaps when the ratio changes.

    if (!m_realtime) {
        if (m_debugLevel > 1) {
            cerr << "Not real time mode: prefilling with " << m_aWindowSize/2 << " samples" << endl;
        }
        for (size_t c = 0; c < m_channels; ++c) {
            m_channelData[c]->reset();
            m_channelData[c]->inbuf->zero(m_aWindowSize/2);
        }
    }
}


void
RubberBandStretcher::Impl::reconfigure()
{
    if (!m_realtime) {
        if (m_mode == Studying) {
            // stop and calculate the stretch curve so far, then reset
            // the df vectors
            calculateStretch();
            m_phaseResetDf.clear();
            m_stretchDf.clear();
            m_silence.clear();
            m_inputDuration = 0;
        }
        configure();
    }

    size_t prevFftSize = m_fftSize;
    size_t prevAWindowSize = m_aWindowSize;
    size_t prevSWindowSize = m_sWindowSize;
    size_t prevOutbufSize = m_outbufSize;

    calculateSizes();

    bool somethingChanged = false;
    
    // There are various allocations in this function, but they should
    // never happen in normal use -- they just recover from the case
    // where not all of the things we need were correctly created when
    // we first configured (for whatever reason).  This is intended to
    // be "effectively" realtime safe.  The same goes for
    // ChannelData::setOutbufSize and setSizes.

    if (m_aWindowSize != prevAWindowSize ||
        m_sWindowSize != prevSWindowSize) {

        if (m_windows.find(m_aWindowSize) == m_windows.end()) {
            std::cerr << "WARNING: reconfigure(): window allocation (size " << m_aWindowSize << ") required in RT mode" << std::endl;
            m_windows[m_aWindowSize] = new Window<float>
                (HanningWindow, m_aWindowSize);
            m_sincs[m_aWindowSize] = new SincWindow<float>
                (m_aWindowSize, m_aWindowSize);
        }

        if (m_windows.find(m_sWindowSize) == m_windows.end()) {
            std::cerr << "WARNING: reconfigure(): window allocation (size " << m_sWindowSize << ") required in RT mode" << std::endl;
            m_windows[m_sWindowSize] = new Window<float>
                (HanningWindow, m_sWindowSize);
            m_sincs[m_sWindowSize] = new SincWindow<float>
                (m_sWindowSize, m_sWindowSize);
        }

        m_awindow = m_windows[m_aWindowSize];
        m_afilter = m_sincs[m_aWindowSize];
        m_swindow = m_windows[m_sWindowSize];

        for (size_t c = 0; c < m_channels; ++c) {
            m_channelData[c]->setSizes(std::max(m_aWindowSize, m_sWindowSize),
                                       m_fftSize);
        }

        somethingChanged = true;
    }

    if (m_outbufSize != prevOutbufSize) {
        for (size_t c = 0; c < m_channels; ++c) {
            m_channelData[c]->setOutbufSize(m_outbufSize);
        }
        somethingChanged = true;
    }

    if (m_pitchScale != 1.0) {
        for (size_t c = 0; c < m_channels; ++c) {

            if (m_channelData[c]->resampler) continue;

            std::cerr << "WARNING: reconfigure(): resampler construction required in RT mode" << std::endl;

            Resampler::Parameters params;
            params.quality = Resampler::FastestTolerable;
            params.dynamism = Resampler::RatioOftenChanging;
            params.ratioChange = Resampler::SmoothRatioChange;
            params.maxBufferSize = m_sWindowSize;
            params.debugLevel = (m_debugLevel > 0 ? m_debugLevel-1 : 0);
            
            m_channelData[c]->resampler = new Resampler(params, 1);

            size_t rbs = 
                lrintf(ceil((m_increment * m_timeRatio * 2) / m_pitchScale));
            if (rbs < m_increment * 16) rbs = m_increment * 16;
            m_channelData[c]->setResampleBufSize(rbs);

            somethingChanged = true;
        }
    }

    if (m_fftSize != prevFftSize) {
        m_phaseResetAudioCurve->setFftSize(m_fftSize);
        m_silentAudioCurve->setFftSize(m_fftSize);
        if (m_stretchAudioCurve) {
            m_stretchAudioCurve->setFftSize(m_fftSize);
        }
        somethingChanged = true;
    }

    if (m_debugLevel > 0) {
        if (somethingChanged) {
            std::cerr << "reconfigure: at least one parameter changed" << std::endl;
        } else {
            std::cerr << "reconfigure: nothing changed" << std::endl;
        }
    }
}

size_t
RubberBandStretcher::Impl::getLatency() const
{
    if (!m_realtime) return 0;
    return lrint((m_aWindowSize/2) / m_pitchScale);
}

void
RubberBandStretcher::Impl::setTransientsOption(Options options)
{
    if (!m_realtime) {
        cerr << "RubberBandStretcher::Impl::setTransientsOption: Not permissible in non-realtime mode" << endl;
        return;
    }
    int mask = (OptionTransientsMixed | OptionTransientsSmooth | OptionTransientsCrisp);
    m_options &= ~mask;
    options &= mask;
    m_options |= options;

    m_stretchCalculator->setUseHardPeaks
        (!(m_options & OptionTransientsSmooth));
}

void
RubberBandStretcher::Impl::setDetectorOption(Options options)
{
    if (!m_realtime) {
        cerr << "RubberBandStretcher::Impl::setDetectorOption: Not permissible in non-realtime mode" << endl;
        return;
    }
    int mask = (OptionDetectorPercussive | OptionDetectorCompound | OptionDetectorSoft);
    m_options &= ~mask;
    options &= mask;
    m_options |= options;

    CompoundAudioCurve::Type dt = CompoundAudioCurve::CompoundDetector;
    if (m_options & OptionDetectorPercussive) dt = CompoundAudioCurve::PercussiveDetector;
    else if (m_options & OptionDetectorSoft) dt = CompoundAudioCurve::SoftDetector;
    
    if (dt == m_detectorType) return;
    m_detectorType = dt;

    if (m_phaseResetAudioCurve) {
        m_phaseResetAudioCurve->setType(m_detectorType);
    }
}

void
RubberBandStretcher::Impl::setPhaseOption(Options options)
{
    int mask = (OptionPhaseLaminar | OptionPhaseIndependent);
    m_options &= ~mask;
    options &= mask;
    m_options |= options;
}

void
RubberBandStretcher::Impl::setFormantOption(Options options)
{
    int mask = (OptionFormantShifted | OptionFormantPreserved);
    m_options &= ~mask;
    options &= mask;
    m_options |= options;
}

void
RubberBandStretcher::Impl::setPitchOption(Options options)
{
    if (!m_realtime) {
        cerr << "RubberBandStretcher::Impl::setPitchOption: Pitch option is not used in non-RT mode" << endl;
        return;
    }

    Options prior = m_options;

    int mask = (OptionPitchHighQuality |
                OptionPitchHighSpeed |
                OptionPitchHighConsistency);
    m_options &= ~mask;
    options &= mask;
    m_options |= options;

    if (prior != m_options) reconfigure();
}

void
RubberBandStretcher::Impl::study(const float *const *input, size_t samples, bool final)
{
    Profiler profiler("RubberBandStretcher::Impl::study");

    if (m_realtime) {
        if (m_debugLevel > 1) {
            cerr << "RubberBandStretcher::Impl::study: Not meaningful in realtime mode" << endl;
        }
        return;
    }

    if (m_mode == Processing || m_mode == Finished) {
        cerr << "RubberBandStretcher::Impl::study: Cannot study after processing" << endl;
        return;
    }
    m_mode = Studying;
    
    size_t consumed = 0;

    ChannelData &cd = *m_channelData[0];
    RingBuffer<float> &inbuf = *cd.inbuf;

    const float *mixdown;
    float *mdalloc = 0;

    if (m_channels > 1 || final) {
        // mix down into a single channel for analysis
        mdalloc = new float[samples];
        for (size_t i = 0; i < samples; ++i) {
            if (i < samples) {
                mdalloc[i] = input[0][i];
            } else {
                mdalloc[i] = 0.f;
            }
        }
        for (size_t c = 1; c < m_channels; ++c) {
            for (size_t i = 0; i < samples; ++i) {
                mdalloc[i] += input[c][i];
            }
        }
        for (size_t i = 0; i < samples; ++i) {
            mdalloc[i] /= m_channels;
        }
        mixdown = mdalloc;
    } else {
        mixdown = input[0];
    }

    while (consumed < samples) {

	size_t writable = inbuf.getWriteSpace();
	writable = min(writable, samples - consumed);

	if (writable == 0) {
            // warn
            cerr << "WARNING: writable == 0 (consumed = " << consumed << ", samples = " << samples << ")" << endl;
	} else {
            inbuf.write(mixdown + consumed, writable);
            consumed += writable;
        }

	while ((inbuf.getReadSpace() >= int(m_aWindowSize)) ||
               (final && (inbuf.getReadSpace() >= int(m_aWindowSize/2)))) {

	    // We know we have at least m_aWindowSize samples
	    // available in m_inbuf.  We need to peek m_aWindowSize of
	    // them for processing, and then skip m_increment to
	    // advance the read pointer.

            // cd.accumulator is not otherwise used during studying,
            // so we can use it as a temporary buffer here

            size_t ready = inbuf.getReadSpace();
            assert(final || ready >= m_aWindowSize);
            inbuf.peek(cd.accumulator, std::min(ready, m_aWindowSize));

            if (m_aWindowSize == m_fftSize) {

                // We don't need the fftshift for studying, as we're
                // only interested in magnitude.

                m_awindow->cut(cd.accumulator);

            } else {

                // If we need to fold (i.e. if the window size is
                // greater than the fft size so we are doing a
                // time-aliased presum fft) or zero-pad, then we might
                // as well use our standard function for it.  This
                // means we retain the m_afilter cut if folding as well,
                // which is good for consistency with real-time mode.
                // We get fftshift as well, which we don't want, but
                // the penalty is nominal.

                // Note that we can't do this in-place.  Pity

                float *tmp = (float *)alloca
                    (std::max(m_fftSize, m_aWindowSize) * sizeof(float));

                if (m_aWindowSize > m_fftSize) {
                    m_afilter->cut(cd.accumulator);
                }

                cutShiftAndFold(tmp, m_fftSize, cd.accumulator, m_awindow);
                v_copy(cd.accumulator, tmp, m_fftSize);
            }

            m_studyFFT->forwardMagnitude(cd.accumulator, cd.fltbuf);

            float df = m_phaseResetAudioCurve->processFloat(cd.fltbuf, m_increment);
            m_phaseResetDf.push_back(df);

//            cout << m_phaseResetDf.size() << " [" << final << "] -> " << df << " \t: ";

            df = m_stretchAudioCurve->processFloat(cd.fltbuf, m_increment);
            m_stretchDf.push_back(df);

            df = m_silentAudioCurve->processFloat(cd.fltbuf, m_increment);
            bool silent = (df > 0.f);
            if (silent && m_debugLevel > 1) {
                cerr << "silence found at " << m_inputDuration << endl;
            }
            m_silence.push_back(silent);

//            cout << df << endl;

            // We have augmented the input by m_aWindowSize/2 so that
            // the first chunk is centred on the first audio sample.
            // We want to ensure that m_inputDuration contains the
            // exact input duration without including this extra bit.
            // We just add up all the increments here, and deduct the
            // extra afterwards.

            m_inputDuration += m_increment;
//                cerr << "incr input duration by increment: " << m_increment << " -> " << m_inputDuration << endl;
            inbuf.skip(m_increment);
	}
    }

    if (final) {
        int rs = inbuf.getReadSpace();
        m_inputDuration += rs;
//        cerr << "incr input duration by read space: " << rs << " -> " << m_inputDuration << endl;

        if (m_inputDuration > m_aWindowSize/2) { // deducting the extra
            m_inputDuration -= m_aWindowSize/2;
        }
    }

    if (m_channels > 1 || final) delete[] mdalloc;
}

vector<int>
RubberBandStretcher::Impl::getOutputIncrements() const
{
    if (!m_realtime) {
        return m_outputIncrements;
    } else {
        vector<int> increments;
        while (m_lastProcessOutputIncrements.getReadSpace() > 0) {
            increments.push_back(m_lastProcessOutputIncrements.readOne());
        }
        return increments;
    }
}

vector<float>
RubberBandStretcher::Impl::getPhaseResetCurve() const
{
    if (!m_realtime) {
        return m_phaseResetDf;
    } else {
        vector<float> df;
        while (m_lastProcessPhaseResetDf.getReadSpace() > 0) {
            df.push_back(m_lastProcessPhaseResetDf.readOne());
        }
        return df;
    }
}

vector<int>
RubberBandStretcher::Impl::getExactTimePoints() const
{
    std::vector<int> points;
    if (!m_realtime) {
        std::vector<StretchCalculator::Peak> peaks =
            m_stretchCalculator->getLastCalculatedPeaks();
        for (size_t i = 0; i < peaks.size(); ++i) {
            points.push_back(peaks[i].chunk);
        }
    }
    return points;
}

void
RubberBandStretcher::Impl::calculateStretch()
{
    Profiler profiler("RubberBandStretcher::Impl::calculateStretch");

    size_t inputDuration = m_inputDuration;

    if (!m_realtime && m_expectedInputDuration > 0) {
        if (m_expectedInputDuration != inputDuration) {
            std::cerr << "RubberBandStretcher: WARNING: Actual study() duration differs from duration set by setExpectedInputDuration (" << m_inputDuration << " vs " << m_expectedInputDuration << ", diff = " << (m_expectedInputDuration - m_inputDuration) << "), using the latter for calculation" << std::endl;
            inputDuration = m_expectedInputDuration;
        }
    }

    double prdm = 0, sdm = 0;
    if (!m_phaseResetDf.empty()) {
        for (int i = 0; i < (int)m_phaseResetDf.size(); ++i) {
            prdm += m_phaseResetDf[i];
        }
        prdm /= m_phaseResetDf.size();
    }
    if (!m_stretchDf.empty()) {
        for (int i = 0; i < (int)m_stretchDf.size(); ++i) {
            sdm += m_stretchDf[i];
        }
        sdm /= m_stretchDf.size();
    }
//    std::cerr << "phase reset df mean = " << prdm << ", stretch df mean = " << sdm << std::endl;

    std::vector<int> increments = m_stretchCalculator->calculate
        (getEffectiveRatio(),
         inputDuration,
         m_phaseResetDf,
         m_stretchDf);

    int history = 0;
    for (size_t i = 0; i < increments.size(); ++i) {
        if (i >= m_silence.size()) break;
        if (m_silence[i]) ++history;
        else history = 0;
        if (history >= int(m_aWindowSize / m_increment) && increments[i] >= 0) {
            increments[i] = -increments[i];
            if (m_debugLevel > 1) {
                std::cerr << "phase reset on silence (silent history == "
                          << history << ")" << std::endl;
            }
        }
    }

    if (m_outputIncrements.empty()) m_outputIncrements = increments;
    else {
        for (size_t i = 0; i < increments.size(); ++i) {
            m_outputIncrements.push_back(increments[i]);
        }
    }
    
    return;
}

void
RubberBandStretcher::Impl::setDebugLevel(int level)
{
    m_debugLevel = level;
    if (m_stretchCalculator) m_stretchCalculator->setDebugLevel(level);
}	

size_t
RubberBandStretcher::Impl::getSamplesRequired() const
{
    Profiler profiler("RubberBandStretcher::Impl::getSamplesRequired");

    size_t reqd = 0;

    for (size_t c = 0; c < m_channels; ++c) {

        size_t reqdHere = 0;

        ChannelData &cd = *m_channelData[c];
        RingBuffer<float> &inbuf = *cd.inbuf;
        RingBuffer<float> &outbuf = *cd.outbuf;

        size_t rs = inbuf.getReadSpace();
        size_t ws = outbuf.getReadSpace();

        if (m_debugLevel > 2) {
            cerr << "getSamplesRequired: ws = " << ws << ", rs = " << rs << ", m_aWindowSize = " << m_aWindowSize << endl;
        }

        // We should never return zero in non-threaded modes if
        // available() would also return zero, i.e. if ws == 0.  If we
        // do that, nothing will ever happen again!  We need to demand
        // at least one increment (i.e. a nominal amount) to feed the
        // engine.

        if (ws == 0 && reqd == 0) reqd = m_increment;

        // See notes in testInbufReadSpace 

        if (rs < m_aWindowSize && !cd.draining) {
            
            if (cd.inputSize == -1) {
                reqdHere = m_aWindowSize - rs;
                if (reqdHere > reqd) reqd = reqdHere;
                continue;
            }
        
            if (rs == 0) {
                reqdHere = m_aWindowSize;
                if (reqdHere > reqd) reqd = reqdHere;
                continue;
            }
        }
    }
    
    return reqd;
}    

void
RubberBandStretcher::Impl::process(const float *const *input, size_t samples, bool final)
{
    Profiler profiler("RubberBandStretcher::Impl::process");

    if (m_mode == Finished) {
        cerr << "RubberBandStretcher::Impl::process: Cannot process again after final chunk" << endl;
        return;
    }

    if (m_mode == JustCreated || m_mode == Studying) {

        if (m_mode == Studying) {

            calculateStretch();

            if (!m_realtime) {
                // See note in configure() above. Of course, we should
                // never enter Studying unless we are non-RT anyway
                if (m_debugLevel > 1) {
                    cerr << "Not real time mode: prefilling" << endl;
                }
                for (size_t c = 0; c < m_channels; ++c) {
                    m_channelData[c]->reset();
                    m_channelData[c]->inbuf->zero(m_aWindowSize/2);
                }
            }
        }

#ifndef NO_THREADING
        if (m_threaded) {
            MutexLocker locker(&m_threadSetMutex);

            for (size_t c = 0; c < m_channels; ++c) {
                ProcessThread *thread = new ProcessThread(this, c);
                m_threadSet.insert(thread);
                thread->start();
            }
            
            if (m_debugLevel > 0) {
                cerr << m_channels << " threads created" << endl;
            }
        }
#endif
        
        m_mode = Processing;
    }

    bool allConsumed = false;

    size_t *consumed = (size_t *)alloca(m_channels * sizeof(size_t));
    for (size_t c = 0; c < m_channels; ++c) {
        consumed[c] = 0;
    }

    while (!allConsumed) {

        // In a threaded mode, our "consumed" counters only indicate
        // the number of samples that have been taken into the input
        // ring buffers waiting to be processed by the process thread.
        // In non-threaded mode, "consumed" counts the number that
        // have actually been processed.

        allConsumed = true;

        for (size_t c = 0; c < m_channels; ++c) {
            consumed[c] += consumeChannel(c,
                                          input,
                                          consumed[c],
                                          samples - consumed[c],
                                          final);
            if (consumed[c] < samples) {
                allConsumed = false;
//                cerr << "process: waiting on input consumption for channel " << c << endl;
            } else {
                if (final) {
                    m_channelData[c]->inputSize = m_channelData[c]->inCount;
                }
//                cerr << "process: happy with channel " << c << endl;
            }
            if (
#ifndef NO_THREADING
                !m_threaded &&
#endif
                !m_realtime) {
                bool any = false, last = false;
                processChunks(c, any, last);
            }
        }

        if (m_realtime) {
            // When running in real time, we need to process both
            // channels in step because we will need to use the sum of
            // their frequency domain representations as the input to
            // the realtime onset detector
            processOneChunk();
        }
#ifndef NO_THREADING
        if (m_threaded) {
            for (ThreadSet::iterator i = m_threadSet.begin();
                 i != m_threadSet.end(); ++i) {
                (*i)->signalDataAvailable();
            }
            m_spaceAvailable.lock();
            if (!allConsumed) {
                m_spaceAvailable.wait(500);
            }
            m_spaceAvailable.unlock();
        }
#endif

        if (m_debugLevel > 1) {
            if (!allConsumed) cerr << "process looping" << endl;
        }
    }

    if (m_debugLevel > 1) {
        cerr << "process returning" << endl;
    }

    if (final) m_mode = Finished;
}


}

