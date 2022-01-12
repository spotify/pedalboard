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

#ifndef RUBBERBAND_STRETCHER_H
#define RUBBERBAND_STRETCHER_H
    
#define RUBBERBAND_VERSION "2.0.0"
#define RUBBERBAND_API_MAJOR_VERSION 2
#define RUBBERBAND_API_MINOR_VERSION 6

#undef RUBBERBAND_DLLEXPORT
#ifdef _MSC_VER
#define RUBBERBAND_DLLEXPORT __declspec(dllexport)
#else
#define RUBBERBAND_DLLEXPORT
#endif

#include <vector>
#include <map>
#include <cstddef>

/**
 * @mainpage RubberBand
 * 
 * The Rubber Band API is contained in the single class
 * RubberBand::RubberBandStretcher.
 *
 * The Rubber Band stretcher supports two processing modes, offline
 * and real-time. The choice of mode is fixed on construction. In
 * offline mode, you must provide the audio block-by-block in two
 * passes: in the first pass calling study(), in the second pass
 * calling process() and receiving the output via retrieve(). In
 * real-time mode, there is no study pass, just a single streaming
 * pass in which the audio is passed to process() and output received
 * via retrieve().
 *
 * In real-time mode you can change the time and pitch ratios at any
 * time, but in offline mode they are fixed and cannot be changed
 * after the study pass has begun. (However, see setKeyFrameMap() for
 * a way to do pre-planned variable time stretching in offline mode.)
 * Offline mode typically produces slightly more precise results.
 *
 * Threading notes for real-time applications:
 * 
 * Multiple instances of RubberBandStretcher may be created and used
 * in separate threads concurrently.  However, for any single instance
 * of RubberBandStretcher, you may not call process() more than once
 * concurrently, and you may not change the time or pitch ratio while
 * a process() call is being executed (if the stretcher was created in
 * "real-time mode"; in "offline mode" you can't change the ratios
 * during use anyway).
 * 
 * So you can run process() in its own thread if you like, but if you
 * want to change ratios dynamically from a different thread, you will
 * need some form of mutex in your code.  Changing the time or pitch
 * ratio is real-time safe except in extreme circumstances, so for
 * most applications that may change these dynamically it probably
 * makes most sense to do so from the same thread as calls process(),
 * even if that is a real-time thread.
 */

namespace RubberBand
{

class RUBBERBAND_DLLEXPORT
RubberBandStretcher
{
public:
    /**
     * Processing options for the timestretcher.  The preferred
     * options should normally be set in the constructor, as a bitwise
     * OR of the option flags.  The default value (DefaultOptions) is
     * intended to give good results in most situations.
     *
     * 1. Flags prefixed \c OptionProcess determine how the timestretcher
     * will be invoked.  These options may not be changed after
     * construction.
     * 
     *   \li \c OptionProcessOffline - Run the stretcher in offline
     *   mode.  In this mode the input data needs to be provided
     *   twice, once to study(), which calculates a stretch profile
     *   for the audio, and once to process(), which stretches it.
     *
     *   \li \c OptionProcessRealTime - Run the stretcher in real-time
     *   mode.  In this mode only process() should be called, and the
     *   stretcher adjusts dynamically in response to the input audio.
     * 
     * The Process setting is likely to depend on your architecture:
     * non-real-time operation on seekable files: Offline; real-time
     * or streaming operation: RealTime.
     *
     * 2. Flags prefixed \c OptionStretch control the profile used for
     * variable timestretching.  Rubber Band always adjusts the
     * stretch profile to minimise stretching of busy broadband
     * transient sounds, but the degree to which it does so is
     * adjustable.  These options may not be changed after
     * construction.
     *
     *   \li \c OptionStretchElastic - Only meaningful in offline
     *   mode, and the default in that mode.  The audio will be
     *   stretched at a variable rate, aimed at preserving the quality
     *   of transient sounds as much as possible.  The timings of low
     *   activity regions between transients may be less exact than
     *   when the precise flag is set.
     * 
     *   \li \c OptionStretchPrecise - Although still using a variable
     *   stretch rate, the audio will be stretched so as to maintain
     *   as close as possible to a linear stretch ratio throughout.
     *   Timing may be better than when using \c OptionStretchElastic, at
     *   slight cost to the sound quality of transients.  This setting
     *   is always used when running in real-time mode.
     *
     * 3. Flags prefixed \c OptionTransients control the component
     * frequency phase-reset mechanism that may be used at transient
     * points to provide clarity and realism to percussion and other
     * significant transient sounds.  These options may be changed
     * after construction when running in real-time mode, but not when
     * running in offline mode.
     * 
     *   \li \c OptionTransientsCrisp - Reset component phases at the
     *   peak of each transient (the start of a significant note or
     *   percussive event).  This, the default setting, usually
     *   results in a clear-sounding output; but it is not always
     *   consistent, and may cause interruptions in stable sounds
     *   present at the same time as transient events.  The
     *   OptionDetector flags (below) can be used to tune this to some
     *   extent.
     *
     *   \li \c OptionTransientsMixed - Reset component phases at the
     *   peak of each transient, outside a frequency range typical of
     *   musical fundamental frequencies.  The results may be more
     *   regular for mixed stable and percussive notes than
     *   \c OptionTransientsCrisp, but with a "phasier" sound.  The
     *   balance may sound very good for certain types of music and
     *   fairly bad for others.
     *
     *   \li \c OptionTransientsSmooth - Do not reset component phases
     *   at any point.  The results will be smoother and more regular
     *   but may be less clear than with either of the other
     *   transients flags.
     *
     * 4. Flags prefixed \c OptionDetector control the type of
     * transient detector used.  These options may be changed
     * after construction when running in real-time mode, but not when
     * running in offline mode.
     *
     *   \li \c OptionDetectorCompound - Use a general-purpose
     *   transient detector which is likely to be good for most
     *   situations.  This is the default.
     *
     *   \li \c OptionDetectorPercussive - Detect percussive
     *   transients.  Note that this was the default and only option
     *   in Rubber Band versions prior to 1.5.
     *
     *   \li \c OptionDetectorSoft - Use an onset detector with less
     *   of a bias toward percussive transients.  This may give better
     *   results with certain material (e.g. relatively monophonic
     *   piano music).
     *
     * 5. Flags prefixed \c OptionPhase control the adjustment of
     * component frequency phases from one analysis window to the next
     * during non-transient segments.  These options may be changed at
     * any time.
     *
     *   \li \c OptionPhaseLaminar - Adjust phases when stretching in
     *   such a way as to try to retain the continuity of phase
     *   relationships between adjacent frequency bins whose phases
     *   are behaving in similar ways.  This, the default setting,
     *   should give good results in most situations.
     *
     *   \li \c OptionPhaseIndependent - Adjust the phase in each
     *   frequency bin independently from its neighbours.  This
     *   usually results in a slightly softer, phasier sound.
     *
     * 6. Flags prefixed \c OptionThreading control the threading
     * model of the stretcher.  These options may not be changed after
     * construction.
     *
     *   \li \c OptionThreadingAuto - Permit the stretcher to
     *   determine its own threading model.  Usually this means using
     *   one processing thread per audio channel in offline mode if
     *   the stretcher is able to determine that more than one CPU is
     *   available, and one thread only in realtime mode.  This is the
     *   default.
     *
     *   \li \c OptionThreadingNever - Never use more than one thread.
     *  
     *   \li \c OptionThreadingAlways - Use multiple threads in any
     *   situation where \c OptionThreadingAuto would do so, except omit
     *   the check for multiple CPUs and instead assume it to be true.
     *
     * 7. Flags prefixed \c OptionWindow control the window size for
     * FFT processing.  The window size actually used will depend on
     * many factors, but it can be influenced.  These options may not
     * be changed after construction.
     *
     *   \li \c OptionWindowStandard - Use the default window size.
     *   The actual size will vary depending on other parameters.
     *   This option is expected to produce better results than the
     *   other window options in most situations.
     *
     *   \li \c OptionWindowShort - Use a shorter window.  This may
     *   result in crisper sound for audio that depends strongly on
     *   its timing qualities.
     *
     *   \li \c OptionWindowLong - Use a longer window.  This is
     *   likely to result in a smoother sound at the expense of
     *   clarity and timing.
     *
     * 8. Flags prefixed \c OptionSmoothing control the use of
     * window-presum FFT and time-domain smoothing.  These options may
     * not be changed after construction.
     *
     *   \li \c OptionSmoothingOff - Do not use time-domain smoothing.
     *   This is the default.
     *
     *   \li \c OptionSmoothingOn - Use time-domain smoothing.  This
     *   will result in a softer sound with some audible artifacts
     *   around sharp transients, but it may be appropriate for longer
     *   stretches of some instruments and can mix well with
     *   OptionWindowShort.
     *
     * 9. Flags prefixed \c OptionFormant control the handling of
     * formant shape (spectral envelope) when pitch-shifting.  These
     * options may be changed at any time.
     *
     *   \li \c OptionFormantShifted - Apply no special formant
     *   processing.  The spectral envelope will be pitch shifted as
     *   normal.  This is the default.
     *
     *   \li \c OptionFormantPreserved - Preserve the spectral
     *   envelope of the unshifted signal.  This permits shifting the
     *   note frequency without so substantially affecting the
     *   perceived pitch profile of the voice or instrument.
     *
     * 10. Flags prefixed \c OptionPitch control the method used for
     * pitch shifting.  These options may be changed at any time.
     * They are only effective in realtime mode; in offline mode, the
     * pitch-shift method is fixed.
     *
     *   \li \c OptionPitchHighSpeed - Use a method with a CPU cost
     *   that is relatively moderate and predictable.  This may
     *   sound less clear than OptionPitchHighQuality, especially
     *   for large pitch shifts.  This is the default.

     *   \li \c OptionPitchHighQuality - Use the highest quality
     *   method for pitch shifting.  This method has a CPU cost
     *   approximately proportional to the required frequency shift.

     *   \li \c OptionPitchHighConsistency - Use the method that gives
     *   greatest consistency when used to create small variations in
     *   pitch around the 1.0-ratio level.  Unlike the previous two
     *   options, this avoids discontinuities when moving across the
     *   1.0 pitch scale in real-time; it also consumes more CPU than
     *   the others in the case where the pitch scale is exactly 1.0.
     *
     * 11. Flags prefixed \c OptionChannels control the method used for
     * processing two-channel audio.  These options may not be changed
     * after construction.
     *
     *   \li \c OptionChannelsApart - Each channel is processed
     *   individually, though timing is synchronised and phases are
     *   synchronised at transients (depending on the OptionTransients
     *   setting).  This gives the highest quality for the individual
     *   channels but a relative lack of stereo focus and unrealistic
     *   increase in "width".  This is the default.
     *
     *   \li \c OptionChannelsTogether - The first two channels (where
     *   two or more are present) are considered to be a stereo pair
     *   and are processed in mid-side format; mid and side are
     *   processed individually, with timing synchronised and phases
     *   synchronised at transients (depending on the OptionTransients
     *   setting).  This usually leads to better focus in the centre
     *   but a loss of stereo space and width.  Any channels beyond
     *   the first two are processed individually.
     */
    
    enum Option {

        OptionProcessOffline       = 0x00000000,
        OptionProcessRealTime      = 0x00000001,

        OptionStretchElastic       = 0x00000000,
        OptionStretchPrecise       = 0x00000010,
    
        OptionTransientsCrisp      = 0x00000000,
        OptionTransientsMixed      = 0x00000100,
        OptionTransientsSmooth     = 0x00000200,

        OptionDetectorCompound     = 0x00000000,
        OptionDetectorPercussive   = 0x00000400,
        OptionDetectorSoft         = 0x00000800,

        OptionPhaseLaminar         = 0x00000000,
        OptionPhaseIndependent     = 0x00002000,
    
        OptionThreadingAuto        = 0x00000000,
        OptionThreadingNever       = 0x00010000,
        OptionThreadingAlways      = 0x00020000,

        OptionWindowStandard       = 0x00000000,
        OptionWindowShort          = 0x00100000,
        OptionWindowLong           = 0x00200000,

        OptionSmoothingOff         = 0x00000000,
        OptionSmoothingOn          = 0x00800000,

        OptionFormantShifted       = 0x00000000,
        OptionFormantPreserved     = 0x01000000,

        OptionPitchHighSpeed       = 0x00000000,
        OptionPitchHighQuality     = 0x02000000,
        OptionPitchHighConsistency = 0x04000000,

        OptionChannelsApart        = 0x00000000,
        OptionChannelsTogether     = 0x10000000

        // n.b. Options is int, so we must stop before 0x80000000
    };

    typedef int Options;

    enum PresetOption {
        DefaultOptions             = 0x00000000,
        PercussiveOptions          = 0x00102000
    };

    /**
     * Construct a time and pitch stretcher object to run at the given
     * sample rate, with the given number of channels.
     *
     * Initial time and pitch scaling ratios and other processing
     * options may be provided. In particular, the behaviour of the
     * stretcher depends strongly on whether offline or real-time mode
     * is selected on construction (via OptionProcessOffline or
     * OptionProcessRealTime option - offline is the default).
     * 
     * In offline mode, you must provide the audio block-by-block in
     * two passes: in the first pass calling study(), in the second
     * pass calling process() and receiving the output via
     * retrieve(). In real-time mode, there is no study pass, just a
     * single streaming pass in which the audio is passed to process()
     * and output received via retrieve().
     *
     * In real-time mode you can change the time and pitch ratios at
     * any time, but in offline mode they are fixed and cannot be
     * changed after the study pass has begun. (However, see
     * setKeyFrameMap() for a way to do pre-planned variable time
     * stretching in offline mode.)
     *
     * See the option documentation above for more details.
     */
    RubberBandStretcher(size_t sampleRate,
                        size_t channels,
                        Options options = DefaultOptions,
                        double initialTimeRatio = 1.0,
                        double initialPitchScale = 1.0);
    ~RubberBandStretcher();

    /**
     * Reset the stretcher's internal buffers.  The stretcher should
     * subsequently behave as if it had just been constructed
     * (although retaining the current time and pitch ratio).
     */
    void reset();

    /**
     * Set the time ratio for the stretcher.  This is the ratio of
     * stretched to unstretched duration -- not tempo.  For example, a
     * ratio of 2.0 would make the audio twice as long (i.e. halve the
     * tempo); 0.5 would make it half as long (i.e. double the tempo);
     * 1.0 would leave the duration unaffected.
     *
     * If the stretcher was constructed in Offline mode, the time
     * ratio is fixed throughout operation; this function may be
     * called any number of times between construction (or a call to
     * reset()) and the first call to study() or process(), but may
     * not be called after study() or process() has been called.
     *
     * If the stretcher was constructed in RealTime mode, the time
     * ratio may be varied during operation; this function may be
     * called at any time, so long as it is not called concurrently
     * with process().  You should either call this function from the
     * same thread as process(), or provide your own mutex or similar
     * mechanism to ensure that setTimeRatio and process() cannot be
     * run at once (there is no internal mutex for this purpose).
     */
    void setTimeRatio(double ratio);

    /**
     * Set the pitch scaling ratio for the stretcher.  This is the
     * ratio of target frequency to source frequency.  For example, a
     * ratio of 2.0 would shift up by one octave; 0.5 down by one
     * octave; or 1.0 leave the pitch unaffected.
     *
     * To put this in musical terms, a pitch scaling ratio
     * corresponding to a shift of S equal-tempered semitones (where S
     * is positive for an upwards shift and negative for downwards) is
     * pow(2.0, S / 12.0).
     *
     * If the stretcher was constructed in Offline mode, the pitch
     * scaling ratio is fixed throughout operation; this function may
     * be called any number of times between construction (or a call
     * to reset()) and the first call to study() or process(), but may
     * not be called after study() or process() has been called.
     *
     * If the stretcher was constructed in RealTime mode, the pitch
     * scaling ratio may be varied during operation; this function may
     * be called at any time, so long as it is not called concurrently
     * with process().  You should either call this function from the
     * same thread as process(), or provide your own mutex or similar
     * mechanism to ensure that setPitchScale and process() cannot be
     * run at once (there is no internal mutex for this purpose).
     */
    void setPitchScale(double scale);

    /**
     * Return the last time ratio value that was set (either on
     * construction or with setTimeRatio()).
     */
    double getTimeRatio() const;

    /**
     * Return the last pitch scaling ratio value that was set (either
     * on construction or with setPitchScale()).
     */
    double getPitchScale() const;

    /**
     * Return the processing latency of the stretcher.  This is the
     * number of audio samples that one would have to discard at the
     * start of the output in order to ensure that the resulting audio
     * aligned with the input audio at the start.  In Offline mode,
     * latency is automatically adjusted for and the result is zero.
     * In RealTime mode, the latency may depend on the time and pitch
     * ratio and other options.
     */
    size_t getLatency() const;

    /**
     * Change an OptionTransients configuration setting.  This may be
     * called at any time in RealTime mode.  It may not be called in
     * Offline mode (for which the transients option is fixed on
     * construction).
     */
    void setTransientsOption(Options options);

    /**
     * Change an OptionDetector configuration setting.  This may be
     * called at any time in RealTime mode.  It may not be called in
     * Offline mode (for which the detector option is fixed on
     * construction).
     */
    void setDetectorOption(Options options);

    /**
     * Change an OptionPhase configuration setting.  This may be
     * called at any time in any mode.
     *
     * Note that if running multi-threaded in Offline mode, the change
     * may not take effect immediately if processing is already under
     * way when this function is called.
     */
    void setPhaseOption(Options options);

    /**
     * Change an OptionFormant configuration setting.  This may be
     * called at any time in any mode.
     *
     * Note that if running multi-threaded in Offline mode, the change
     * may not take effect immediately if processing is already under
     * way when this function is called.
     */
    void setFormantOption(Options options);

    /**
     * Change an OptionPitch configuration setting.  This may be
     * called at any time in RealTime mode.  It may not be called in
     * Offline mode (for which the pitch option is fixed on
     * construction).
     */
    void setPitchOption(Options options);

    /**
     * Tell the stretcher exactly how many input sample frames it will
     * receive.  This is only useful in Offline mode, when it allows
     * the stretcher to ensure that the number of output samples is
     * exactly correct.  In RealTime mode no such guarantee is
     * possible and this value is ignored.
     *
     * Note that the value of "samples" refers to the number of audio
     * sample frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     */
    void setExpectedInputDuration(size_t samples);

    /**
     * Tell the stretcher the maximum number of sample frames that you
     * will ever be passing in to a single process() call.  If you
     * don't call this, the stretcher will assume that you are calling
     * getSamplesRequired() at each cycle and are never passing more
     * samples than are suggested by that function.
     *
     * If your application has some external constraint that means you
     * prefer a fixed block size, then your normal mode of operation
     * would be to provide that block size to this function; to loop
     * calling process() with that size of block; after each call to
     * process(), test whether output has been generated by calling
     * available(); and, if so, call retrieve() to obtain it.  See
     * getSamplesRequired() for a more suitable operating mode for
     * applications without such external constraints.
     *
     * This function may not be called after the first call to study()
     * or process().
     *
     * Note that this value is only relevant to process(), not to
     * study() (to which you may pass any number of samples at a time,
     * and from which there is no output).
     *
     * Note that the value of "samples" refers to the number of audio
     * sample frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     */
    void setMaxProcessSize(size_t samples);

    /**
     * Ask the stretcher how many audio sample frames should be
     * provided as input in order to ensure that some more output
     * becomes available.
     * 
     * If your application has no particular constraint on processing
     * block size and you are able to provide any block size as input
     * for each cycle, then your normal mode of operation would be to
     * loop querying this function; providing that number of samples
     * to process(); and reading the output using available() and
     * retrieve().  See setMaxProcessSize() for a more suitable
     * operating mode for applications that do have external block
     * size constraints.
     *
     * Note that this value is only relevant to process(), not to
     * study() (to which you may pass any number of samples at a time,
     * and from which there is no output).
     *
     * Note that the return value refers to the number of audio sample
     * frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     */
     size_t getSamplesRequired() const;

    /**
     * Provide a set of mappings from "before" to "after" sample
     * numbers so as to enforce a particular stretch profile.  The
     * argument is a map from audio sample frame number in the source
     * material, to the corresponding sample frame number in the
     * stretched output.  The mapping should be for key frames only,
     * with a "reasonable" gap between mapped samples.
     *
     * This function cannot be used in RealTime mode.
     *
     * This function may not be called after the first call to
     * process().  It should be called after the time and pitch ratios
     * have been set; the results of changing the time and pitch
     * ratios after calling this function are undefined.  Calling
     * reset() will clear this mapping.
     *
     * The key frame map only affects points within the material; it
     * does not determine the overall stretch ratio (that is, the
     * ratio between the output material's duration and the source
     * material's duration).  You need to provide this ratio
     * separately to setTimeRatio(), otherwise the results may be
     * truncated or extended in unexpected ways regardless of the
     * extent of the frame numbers found in the key frame map.
     */
    void setKeyFrameMap(const std::map<size_t, size_t> &);
    
    /**
     * Provide a block of "samples" sample frames for the stretcher to
     * study and calculate a stretch profile from.
     *
     * This is only meaningful in Offline mode, and is required if
     * running in that mode.  You should pass the entire input through
     * study() before any process() calls are made, as a sequence of
     * blocks in individual study() calls, or as a single large block.
     *
     * "input" should point to de-interleaved audio data with one
     * float array per channel. Sample values are conventionally
     * expected to be in the range -1.0f to +1.0f.  "samples" supplies
     * the number of audio sample frames available in "input". If
     * "samples" is zero, "input" may be NULL.
     *
     * Note that the value of "samples" refers to the number of audio
     * sample frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     * 
     * Set "final" to true if this is the last block of data that will
     * be provided to study() before the first process() call.
     */
    void study(const float *const *input, size_t samples, bool final);

    /**
     * Provide a block of "samples" sample frames for processing.
     * See also getSamplesRequired() and setMaxProcessSize().
     *
     * "input" should point to de-interleaved audio data with one
     * float array per channel. Sample values are conventionally
     * expected to be in the range -1.0f to +1.0f.  "samples" supplies
     * the number of audio sample frames available in "input".
     *
     * Note that the value of "samples" refers to the number of audio
     * sample frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     *
     * Set "final" to true if this is the last block of input data.
     */
    void process(const float *const *input, size_t samples, bool final);

    /**
     * Ask the stretcher how many audio sample frames of output data
     * are available for reading (via retrieve()).
     * 
     * This function returns 0 if no frames are available: this
     * usually means more input data needs to be provided, but if the
     * stretcher is running in threaded mode it may just mean that not
     * enough data has yet been processed.  Call getSamplesRequired()
     * to discover whether more input is needed.
     *
     * Note that the return value refers to the number of audio sample
     * frames, which may be multi-channel, not the number of
     * individual samples. (For example, one second of stereo audio
     * sampled at 44100Hz yields a value of 44100 sample frames, not
     * 88200.)  This rule applies throughout the Rubber Band API.
     *
     * This function returns -1 if all data has been fully processed
     * and all output read, and the stretch process is now finished.
     */
    int available() const;

    /**
     * Obtain some processed output data from the stretcher.  Up to
     * "samples" samples will be stored in each of the output arrays
     * (one per channel for de-interleaved audio data) pointed to by
     * "output".  The number of sample frames available to be
     * retrieved can be queried beforehand with a call to available().
     * The return value is the actual number of sample frames
     * retrieved.
     *
     * Note that the value of "samples" and the return value refer to
     * the number of audio sample frames, which may be multi-channel,
     * not the number of individual samples. (For example, one second
     * of stereo audio sampled at 44100Hz yields a value of 44100
     * sample frames, not 88200.)  This rule applies throughout the
     * Rubber Band API.
     */
    size_t retrieve(float *const *output, size_t samples) const;

    /**
     * Return the value of internal frequency cutoff value n.
     *
     * This function is not for general use.
     */
    float getFrequencyCutoff(int n) const;

    /** 
     * Set the value of internal frequency cutoff n to f Hz.
     *
     * This function is not for general use.
     */
    void setFrequencyCutoff(int n, float f);
    
    /**
     * Retrieve the value of the internal input block increment value.
     *
     * This function is provided for diagnostic purposes only.
     */
    size_t getInputIncrement() const;

    /**
     * In offline mode, retrieve the sequence of internal block
     * increments for output, for the entire audio data, provided the
     * stretch profile has been calculated.  In realtime mode,
     * retrieve any output increments that have accumulated since the
     * last call to getOutputIncrements, to a limit of 16.
     *
     * This function is provided for diagnostic purposes only.
     */
    std::vector<int> getOutputIncrements() const;

    /**
     * In offline mode, retrieve the sequence of internal phase reset
     * detection function values, for the entire audio data, provided
     * the stretch profile has been calculated.  In realtime mode,
     * retrieve any phase reset points that have accumulated since the
     * last call to getPhaseResetCurve, to a limit of 16.
     *
     * This function is provided for diagnostic purposes only.
     */
    std::vector<float> getPhaseResetCurve() const;

    /**
     * In offline mode, retrieve the sequence of internal frames for
     * which exact timing has been sought, for the entire audio data,
     * provided the stretch profile has been calculated.  In realtime
     * mode, return an empty sequence.
     *
     * This function is provided for diagnostic purposes only.
     */
    std::vector<int> getExactTimePoints() const;

    /**
     * Return the number of channels this stretcher was constructed
     * with.
     */
    size_t getChannelCount() const;

    /**
     * Force the stretcher to calculate a stretch profile.  Normally
     * this happens automatically for the first process() call in
     * offline mode.
     *
     * This function is provided for diagnostic purposes only.
     */
    void calculateStretch();

    /**
     * Set the level of debug output.  The value may be from 0 (errors
     * only) to 3 (very verbose, with audible ticks in the output at
     * phase reset points).  The default is whatever has been set
     * using setDefaultDebugLevel, or 0 if that function has not been
     * called.
     */
    void setDebugLevel(int level);

    /**
     * Set the default level of debug output for subsequently
     * constructed stretchers.
     *
     * @see setDebugLevel
     */
    static void setDefaultDebugLevel(int level);

protected:
    class Impl;
    Impl *m_d;
};

}

#endif
