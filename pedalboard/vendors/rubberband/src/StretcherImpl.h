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

#ifndef RUBBERBAND_STRETCHERIMPL_H
#define RUBBERBAND_STRETCHERIMPL_H

#include "../rubberband/RubberBandStretcher.h"

#include "dsp/Window.h"
#include "dsp/SincWindow.h"
#include "dsp/FFT.h"

#include "audiocurves/CompoundAudioCurve.h"

#include "base/RingBuffer.h"
#include "base/Scavenger.h"
#include "system/Thread.h"
#include "system/sysutils.h"

#include <set>
#include <algorithm>

using namespace RubberBand;

namespace RubberBand
{

#ifdef PROCESS_SAMPLE_TYPE
typedef PROCESS_SAMPLE_TYPE process_t;
#else
typedef double process_t;
#endif

class AudioCurveCalculator;
class StretchCalculator;

class RubberBandStretcher::Impl
{
public:
    Impl(size_t sampleRate, size_t channels, Options options,
         double initialTimeRatio, double initialPitchScale);
    ~Impl();
    
    void reset();
    void setTimeRatio(double ratio);
    void setPitchScale(double scale);

    double getTimeRatio() const;
    double getPitchScale() const;

    size_t getLatency() const;

    void setTransientsOption(Options);
    void setDetectorOption(Options);
    void setPhaseOption(Options);
    void setFormantOption(Options);
    void setPitchOption(Options);

    void setExpectedInputDuration(size_t samples);
    void setMaxProcessSize(size_t samples);
    void setKeyFrameMap(const std::map<size_t, size_t> &);

    size_t getSamplesRequired() const;

    void study(const float *const *input, size_t samples, bool final);
    void process(const float *const *input, size_t samples, bool final);

    int available() const;
    size_t retrieve(float *const *output, size_t samples) const;

    float getFrequencyCutoff(int n) const;
    void setFrequencyCutoff(int n, float f);

    size_t getInputIncrement() const {
        return m_increment;
    }

    std::vector<int> getOutputIncrements() const;
    std::vector<float> getPhaseResetCurve() const;
    std::vector<int> getExactTimePoints() const;

    size_t getChannelCount() const {
        return m_channels;
    }
    
    void calculateStretch();

    void setDebugLevel(int level);
    static void setDefaultDebugLevel(int level) { m_defaultDebugLevel = level; }

protected:
    size_t m_sampleRate;
    size_t m_channels;

    void prepareChannelMS(size_t channel, const float *const *inputs,
                          size_t offset, size_t samples, float *prepared);
    size_t consumeChannel(size_t channel, const float *const *inputs,
                          size_t offset, size_t samples, bool final);
    void processChunks(size_t channel, bool &any, bool &last);
    bool processOneChunk(); // across all channels, for real time use
    bool processChunkForChannel(size_t channel, size_t phaseIncrement,
                                size_t shiftIncrement, bool phaseReset);
    bool testInbufReadSpace(size_t channel);
    void calculateIncrements(size_t &phaseIncrement,
                             size_t &shiftIncrement, bool &phaseReset);
    bool getIncrements(size_t channel, size_t &phaseIncrement,
                       size_t &shiftIncrement, bool &phaseReset);
    void analyseChunk(size_t channel);
    void modifyChunk(size_t channel, size_t outputIncrement, bool phaseReset);
    void formantShiftChunk(size_t channel);
    void synthesiseChunk(size_t channel, size_t shiftIncrement);
    void writeChunk(size_t channel, size_t shiftIncrement, bool last);

    void calculateSizes();
    void configure();
    void reconfigure();

    double getEffectiveRatio() const;
    
    size_t roundUp(size_t value); // to next power of two

    template <typename T, typename S>
    void cutShiftAndFold(T *target, int targetSize,
                         S *src, // destructive to src
                         Window<float> *window) {
        window->cut(src);
        const int windowSize = window->getSize();
        const int hs = targetSize / 2;
        if (windowSize == targetSize) {
            v_convert(target, src + hs, hs);
            v_convert(target + hs, src, hs);
        } else {
            v_zero(target, targetSize);
            int j = targetSize - windowSize/2;
            while (j < 0) j += targetSize;
            for (int i = 0; i < windowSize; ++i) {
                target[j] += src[i];
                if (++j == targetSize) j = 0;
            }
        }
    }

    bool resampleBeforeStretching() const;
    
    double m_timeRatio;
    double m_pitchScale;

    // n.b. either m_fftSize is an integer multiple of m_windowSize,
    // or vice versa
    size_t m_fftSize;
    size_t m_aWindowSize; //!!! or use m_awindow->getSize() throughout?
    size_t m_sWindowSize; //!!! or use m_swindow->getSize() throughout?
    size_t m_increment;
    size_t m_outbufSize;

    size_t m_maxProcessSize;
    size_t m_expectedInputDuration;

#ifndef NO_THREADING    
    bool m_threaded;
#endif

    bool m_realtime;
    Options m_options;
    int m_debugLevel;

    enum ProcessMode {
        JustCreated,
        Studying,
        Processing,
        Finished
    };

    ProcessMode m_mode;

    std::map<size_t, Window<float> *> m_windows;
    std::map<size_t, SincWindow<float> *> m_sincs;
    Window<float> *m_awindow;
    SincWindow<float> *m_afilter;
    Window<float> *m_swindow;
    FFT *m_studyFFT;

#ifndef NO_THREADING
    Condition m_spaceAvailable;
    
    class ProcessThread : public Thread
    {
    public:
        ProcessThread(Impl *s, size_t c);
        void run();
        void signalDataAvailable();
        void abandon();
    private:
        Impl *m_s;
        size_t m_channel;
        Condition m_dataAvailable;
        bool m_abandoning;
    };

    mutable Mutex m_threadSetMutex;
    typedef std::set<ProcessThread *> ThreadSet;
    ThreadSet m_threadSet;
    
#if defined HAVE_IPP && !defined USE_SPEEX
    // Exasperatingly, the IPP polyphase resampler does not appear to
    // be thread-safe as advertised -- a good reason to prefer the
    // Speex alternative
    Mutex m_resamplerMutex;
#endif
#endif

    size_t m_inputDuration;
    CompoundAudioCurve::Type m_detectorType;
    std::vector<float> m_phaseResetDf;
    std::vector<float> m_stretchDf;
    std::vector<bool> m_silence;
    int m_silentHistory;

    class ChannelData; 
    std::vector<ChannelData *> m_channelData;

    std::vector<int> m_outputIncrements;

    mutable RingBuffer<int> m_lastProcessOutputIncrements;
    mutable RingBuffer<float> m_lastProcessPhaseResetDf;
    Scavenger<RingBuffer<float> > m_emergencyScavenger;

    CompoundAudioCurve *m_phaseResetAudioCurve;
    AudioCurveCalculator *m_stretchAudioCurve;
    AudioCurveCalculator *m_silentAudioCurve;
    StretchCalculator *m_stretchCalculator;

    float m_freq0;
    float m_freq1;
    float m_freq2;

    size_t m_baseFftSize;
    float m_rateMultiple;

    void writeOutput(RingBuffer<float> &to, float *from,
                     size_t qty, size_t &outCount, size_t theoreticalOut);

    static int m_defaultDebugLevel;
    static const size_t m_defaultIncrement;
    static const size_t m_defaultFftSize;
};

}

#endif
