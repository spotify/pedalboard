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

#ifndef BQ_BQRESAMPLER_H
#define BQ_BQRESAMPLER_H

#include <vector>

#include "../system/Allocators.h"
#include "../system/VectorOps.h"

namespace RubberBand {

class BQResampler
{
public:
    enum Quality { Best, FastestTolerable, Fastest };
    enum Dynamism { RatioOftenChanging, RatioMostlyFixed };
    enum RatioChange { SmoothRatioChange, SuddenRatioChange };
    
    struct Parameters {
        Quality quality;
        Dynamism dynamism; 
        RatioChange ratioChange;
        double referenceSampleRate;
        int debugLevel;

        Parameters() :
            quality(FastestTolerable),
            dynamism(RatioMostlyFixed),
            ratioChange(SmoothRatioChange),
            referenceSampleRate(44100),
            debugLevel(0) { }
    };

    BQResampler(Parameters parameters, int channels);
    BQResampler(const BQResampler &);

    int resampleInterleaved(float *const out, int outspace,
                            const float *const in, int incount,
                            double ratio, bool final);

    double getEffectiveRatio(double ratio) const;
    
    void reset();

private:
    struct QualityParams {
        int p_multiple;
        int proto_p;
        double k_snr;
        double k_transition;
        double cut;
        QualityParams(Quality);
    };

    const QualityParams m_qparams;
    const Dynamism m_dynamism;
    const RatioChange m_ratio_change;
    const int m_debug_level;
    const double m_initial_rate;
    const int m_channels;

    struct params {
        double ratio;
        int numerator;
        int denominator;
        double effective;
        double peak_to_zero;
        double scale;
        params() : ratio(1.0), numerator(1), denominator(1),
                   effective(1.0), peak_to_zero(0), scale(1.0) { }
    };

    struct phase_rec {
        int next_phase;
        int length;
        int start_index;
        int drop;
        phase_rec() : next_phase(0), length(0), start_index(0), drop(0) { }
    };

    typedef std::vector<float, RubberBand::StlAllocator<float> > floatbuf;
    
    struct state {
        params parameters;
        int initial_phase;
        int current_phase;
        int current_channel;
        int filter_length;
        std::vector<phase_rec> phase_info;
        floatbuf phase_sorted_filter;
        floatbuf buffer;
        int left;
        int centre;
        int fill;
        state() : initial_phase(0), current_phase(0), current_channel(0),
                  filter_length(0), left(0), centre(0), fill(0) { }
    };

    state m_state_a;
    state m_state_b;

    state *m_s;        // points at either m_state_a or m_state_b
    state *m_fade;     // whichever one m_s does not point to
    
    int m_fade_count;
    
    std::vector<double> m_prototype;
    int m_proto_length;
    bool m_initialised;

    int gcd(int a, int b) const;
    double bessel0(double x) const;
    std::vector<double> kaiser(double beta, int len) const;
    void kaiser_params(double attenuation, double transition,
                       double &beta, int &len) const;
    std::vector<double> kaiser_for(double attenuation, double transition,
                                   int minlen, int maxlen) const;
    void sinc_multiply(double peak_to_zero, std::vector<double> &buf) const;

    params fill_params(double ratio, double numd, double denomd) const;
    params pick_params(double ratio) const;

    std::vector<double> make_filter(int filter_length,
                                    double peak_to_zero) const;
    
    void phase_data_for(std::vector<phase_rec> &target_phase_data,
                        floatbuf &target_phase_sorted_filter,
                        int filter_length,
                        const std::vector<double> *filter,
                        int initial_phase,
                        int input_spacing,
                        int output_spacing) const;
    
    void state_for_ratio(state &target_state,
                         double ratio,
                         const state &R__ prev_state) const;
    
    double reconstruct_one(state *s) const;

    BQResampler &operator=(const BQResampler &); // not provided
};

}

#endif
