/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/* Copyright (C) 2007 Jean-Marc Valin

   File: resample.c
   Arbitrary resampling code

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

/*
   The design goals of this code are:
      - Very fast algorithm
      - SIMD-friendly algorithm
      - Low memory requirement
      - Good *perceptual* quality (and not best SNR)

   Warning: This resampler is relatively new. Although I think I got rid of
   all the major bugs and I don't expect the API to change anymore, there
   may be something I've missed. So use with caution.

   This algorithm is based on this original resampling algorithm:
   Smith, Julius O. Digital Audio Resampling Home Page
   Center for Computer Research in Music and Acoustics (CCRMA),
   Stanford University, 2007.
   Web published at http://www-ccrma.stanford.edu/~jos/resample/.

   There is one main difference, though. This resampler uses cubic
   interpolation instead of linear interpolation in the above paper. This
   makes the table much smaller and makes it possible to compute that table
   on a per-stream basis. In turn, being able to tweak the table for each
   stream makes it possible to both reduce complexity on simple ratios
   (e.g. 2/3), and get rid of the rounding operations in the inner loop.
   The latter both reduces CPU time and makes the algorithm more SIMD-friendly.
*/

/*
   NOTE: This code has been cut down and reformatted by Chris Cannam
   for personal reading preference, and for use in the Rubber Band
   time stretching and pitch shifting library.  If you have problems
   with this code, cast suspicion on the butchering it has undergone;
   it's probably my fault.  If you want a properly functioning
   version, please go for the original Speex code first.  I haven't
   made any substantial changes to this code, I've just made it less
   generally useful.
*/

#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#ifdef HAVE_IPP
#include <ipps.h>
#endif

// Simple allocators with a fixed minimum, to avoid reallocation if
// the size changes but remains smaller than that.  The system alloc
// functions no doubt do exactly the same thing for some value
// probably not too distant from ours, but we want the certainty.

#define ALLOC_MINIMUM 4096

static void *speex_alloc (int count, int size)
{
#ifdef HAVE_IPP
    void *rv;
#endif
    if (count * size < ALLOC_MINIMUM) {
        count = ALLOC_MINIMUM / size;
    }

#ifdef HAVE_IPP
    if (size == sizeof(float) && size == 4) { // or sizeof(int32) or whatever, doesn't matter
        rv = ippsMalloc_32f(count);
    } else if (size == sizeof(double) && size == 8) {
        rv = ippsMalloc_64f(count);
    } else {
        rv = ippsMalloc_8u(count * size);
    }
    memset(rv, 0, count * size);
    return rv;
#else
    return calloc(count, size);
#endif
}

static void speex_free (void *ptr) 
{
//	fprintf(stderr,"speex_free(%p)\n", ptr);
#ifdef HAVE_IPP
  	ippsFree(ptr);
#else
    free(ptr);
#endif
}

static void *speex_realloc (void *ptr, int oldcount, int newcount, int size)
{
#ifdef HAVE_IPP
	void *newptr;
#endif

//	fprintf(stderr,"speex_realloc(%p,%d,%d,%d)\n", ptr, oldcount, newcount, size);

    if (newcount * size < ALLOC_MINIMUM) {
//		fprintf(stderr,"returning %p\n",ptr);
        return ptr;
    }
//    fprintf(stderr, "NOTE: speex_realloc: actual reallocation happening (newcount = %d, size = %d)\n", newcount, size);

#ifdef HAVE_IPP
    newptr = speex_alloc(newcount, size);
    if (ptr && oldcount > 0) {
        int copy = newcount;
        if (oldcount < copy) copy = oldcount;
        memcpy(newptr, ptr, copy * size);
    }
    speex_free(ptr);
//	fprintf(stderr,"returning %p\n", ptr);
    return newptr;
#else
    return realloc(ptr, newcount * size);
#endif
}

#include "speex_resampler.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159263
#endif

#define FILTER_SIZE 64
#define OVERSAMPLE 8

#define IMAX(a,b) ((a) > (b) ? (a) : (b))
#define IMIN(a,b) ((a) < (b) ? (a) : (b))

#ifndef NULL
#define NULL 0
#endif


typedef int (*resampler_basic_func)(SpeexResamplerState *, spx_uint32_t , const float *, spx_uint32_t *, float *, spx_uint32_t *);

struct SpeexResamplerState_ {
    spx_uint32_t in_rate;
    spx_uint32_t out_rate;
    spx_uint32_t num_rate;
    spx_uint32_t den_rate;

    int    quality;
    spx_uint32_t nb_channels;
    spx_uint32_t filt_len;
    spx_uint32_t mem_alloc_size;
    int          int_advance;
    int          frac_advance;
    float  cutoff;
    spx_uint32_t oversample;
    int          initialised;
    int          started;

    /* These are per-channel */
    spx_int32_t  *last_sample;
    spx_uint32_t *samp_frac_num;
    spx_uint32_t *magic_samples;

    float *mem;
    float *sinc_table;
    spx_uint32_t sinc_table_length;
    spx_uint32_t sinc_table_alloc;
    resampler_basic_func resampler_ptr;

    int    in_stride;
    int    out_stride;
} ;

static double kaiser12_table[68] = {
    0.99859849, 1.00000000, 0.99859849, 0.99440475, 0.98745105, 0.97779076,
    0.96549770, 0.95066529, 0.93340547, 0.91384741, 0.89213598, 0.86843014,
    0.84290116, 0.81573067, 0.78710866, 0.75723148, 0.72629970, 0.69451601,
    0.66208321, 0.62920216, 0.59606986, 0.56287762, 0.52980938, 0.49704014,
    0.46473455, 0.43304576, 0.40211431, 0.37206735, 0.34301800, 0.31506490,
    0.28829195, 0.26276832, 0.23854851, 0.21567274, 0.19416736, 0.17404546,
    0.15530766, 0.13794294, 0.12192957, 0.10723616, 0.09382272, 0.08164178,
    0.07063950, 0.06075685, 0.05193064, 0.04409466, 0.03718069, 0.03111947,
    0.02584161, 0.02127838, 0.01736250, 0.01402878, 0.01121463, 0.00886058,
    0.00691064, 0.00531256, 0.00401805, 0.00298291, 0.00216702, 0.00153438,
    0.00105297, 0.00069463, 0.00043489, 0.00025272, 0.00013031, 0.0000527734,
    0.00001000, 0.00000000
};

static double kaiser10_table[36] = {
    0.99537781, 1.00000000, 0.99537781, 0.98162644, 0.95908712, 0.92831446,
    0.89005583, 0.84522401, 0.79486424, 0.74011713, 0.68217934, 0.62226347,
    0.56155915, 0.50119680, 0.44221549, 0.38553619, 0.33194107, 0.28205962,
    0.23636152, 0.19515633, 0.15859932, 0.12670280, 0.09935205, 0.07632451,
    0.05731132, 0.04193980, 0.02979584, 0.02044510, 0.01345224, 0.00839739,
    0.00488951, 0.00257636, 0.00115101, 0.00035515, 0.00000000, 0.00000000
};

static double kaiser8_table[36] = {
    0.99635258, 1.00000000, 0.99635258, 0.98548012, 0.96759014, 0.94302200,
    0.91223751, 0.87580811, 0.83439927, 0.78875245, 0.73966538, 0.68797126,
    0.63451750, 0.58014482, 0.52566725, 0.47185369, 0.41941150, 0.36897272,
    0.32108304, 0.27619388, 0.23465776, 0.19672670, 0.16255380, 0.13219758,
    0.10562887, 0.08273982, 0.06335451, 0.04724088, 0.03412321, 0.02369490,
    0.01563093, 0.00959968, 0.00527363, 0.00233883, 0.00050000, 0.00000000
};

static double kaiser6_table[36] = {
    0.99733006, 1.00000000, 0.99733006, 0.98935595, 0.97618418, 0.95799003,
    0.93501423, 0.90755855, 0.87598009, 0.84068475, 0.80211977, 0.76076565,
    0.71712752, 0.67172623, 0.62508937, 0.57774224, 0.53019925, 0.48295561,
    0.43647969, 0.39120616, 0.34752997, 0.30580127, 0.26632152, 0.22934058,
    0.19505503, 0.16360756, 0.13508755, 0.10953262, 0.08693120, 0.06722600,
    0.05031820, 0.03607231, 0.02432151, 0.01487334, 0.00752000, 0.00000000
};

struct FuncDef {
    double *table;
    int oversample;
};

static struct FuncDef _KAISER12 = {kaiser12_table, 64};
#define KAISER12 (&_KAISER12)
static struct FuncDef _KAISER10 = {kaiser10_table, 32};
#define KAISER10 (&_KAISER10)
static struct FuncDef _KAISER8 = {kaiser8_table, 32};
#define KAISER8 (&_KAISER8)
static struct FuncDef _KAISER6 = {kaiser6_table, 32};
#define KAISER6 (&_KAISER6)

struct QualityMapping {
    int base_length;
    int oversample;
    float downsample_bandwidth;
    float upsample_bandwidth;

    struct FuncDef *window_func;
};


/* This table maps conversion quality to internal parameters. There are two
   reasons that explain why the up-sampling bandwidth is larger than the
   down-sampling bandwidth:
   1) When up-sampling, we can assume that the spectrum is already attenuated
      close to the Nyquist rate (from an A/D or a previous resampling filter)
   2) Any aliasing that occurs very close to the Nyquist rate will be masked
      by the sinusoids/noise just below the Nyquist rate (guaranteed only for
      up-sampling).
*/

static const struct QualityMapping quality_map[11] = {
   {  8,  4, 0.830f, 0.860f, KAISER6 }, /* Q0 */
   { 16,  4, 0.850f, 0.880f, KAISER6 }, /* Q1 */
   { 32,  4, 0.882f, 0.910f, KAISER6 }, /* Q2 */  /* 82.3% cutoff ( ~60 dB stop) 6  */
   { 48,  8, 0.895f, 0.917f, KAISER8 }, /* Q3 */  /* 84.9% cutoff ( ~80 dB stop) 8  */
   { 64,  8, 0.921f, 0.940f, KAISER8 }, /* Q4 */  /* 88.7% cutoff ( ~80 dB stop) 8  */
   { 80, 16, 0.922f, 0.940f, KAISER10}, /* Q5 */  /* 89.1% cutoff (~100 dB stop) 10 */
   { 96, 16, 0.940f, 0.945f, KAISER10}, /* Q6 */  /* 91.5% cutoff (~100 dB stop) 10 */
   {128, 16, 0.950f, 0.950f, KAISER10}, /* Q7 */  /* 93.1% cutoff (~100 dB stop) 10 */
   {160, 16, 0.960f, 0.960f, KAISER10}, /* Q8 */  /* 94.5% cutoff (~100 dB stop) 10 */
   {192, 32, 0.968f, 0.968f, KAISER12}, /* Q9 */  /* 95.5% cutoff (~100 dB stop) 10 */
   {256, 32, 0.975f, 0.975f, KAISER12}, /* Q10 */ /* 96.6% cutoff (~100 dB stop) 10 */
};
/*8,24,40,56,80,104,128,160,200,256,320*/

static double compute_func(float x, struct FuncDef *func)
{
    float y, frac;
    double interp[4];
    int ind;

    y = x * func->oversample;
    ind = (int)floor(y);
    frac = (y - ind);

    /* CSE with handle the repeated powers */
    interp[3] =  -0.1666666667 * frac + 0.1666666667 * (frac * frac * frac);
    interp[2] = frac + 0.5 * (frac * frac) - 0.5 * (frac * frac * frac);
    interp[0] = -0.3333333333 * frac + 0.5 * (frac * frac) - 0.1666666667 * (frac * frac * frac);

    /* Just to make sure we don't have rounding problems */
    interp[1] = 1.f - interp[3] - interp[2] - interp[0];

    /*sum = frac*accum[1] + (1-frac)*accum[2];*/
    return 
	interp[0]*func->table[ind] + interp[1]*func->table[ind+1] +
	interp[2]*func->table[ind+2] + interp[3]*func->table[ind+3];
}

/* The slow way of computing a sinc for the table. Should improve that some day */
static float sinc(float cutoff, float x, int N, struct FuncDef *window_func)
{
    float xx = x * cutoff;

    if (fabsf(x) < 1e-6)
        return cutoff;
    else if (fabsf(x) > .5*N)
        return 0;

    /*FIXME: Can it really be any slower than this? */
    return cutoff*sin(M_PI*xx) / (M_PI*xx)
	* compute_func(fabs(2.*x / N), window_func);
}

static void cubic_coef(float frac, float interp[4])
{
    /* Compute interpolation coefficients. I'm not sure whether this
    corresponds to cubic interpolation but I know it's MMSE-optimal on
    a sinc */

    interp[0] =  -0.16667f * frac + 0.16667f * frac * frac * frac;
    interp[1] = frac + 0.5f * frac * frac - 0.5f * frac * frac * frac;
    interp[3] = -0.33333f * frac + 0.5f * frac * frac - 0.16667f * frac * frac * frac;

    /* Just to make sure we don't have rounding problems */
    interp[2] = 1. - interp[0] - interp[1] - interp[3];
}

static int resampler_basic_direct_single(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    int N = st->filt_len;
    int out_sample = 0;
    float *mem;
    int last_sample = st->last_sample[channel_index];
    unsigned int samp_frac_num = st->samp_frac_num[channel_index];

    mem = st->mem + channel_index * st->mem_alloc_size;

    while (!(last_sample >= (int)*in_len || out_sample >= (int)*out_len)) {

        int j;
        float sum = 0;

        /* We already have all the filter coefficients pre-computed in the table */
        const float *ptr;

        for (j = 0; last_sample - N + 1 + j < 0; j++) {
            sum += ((float)(mem[last_sample+j]) *
		    (float)(st->sinc_table[samp_frac_num*st->filt_len+j]));
        }

        /* Do the new part */
        if (in != NULL) {

            ptr = in + st->in_stride * (last_sample - N + 1 + j);

            for (; j < N; j++) {
                sum += ((float)(*ptr) *
			(float)(st->sinc_table[samp_frac_num*st->filt_len+j]));
                ptr += st->in_stride;
            }
        }

        *out = (sum);

        out += st->out_stride;
        out_sample++;
        last_sample += st->int_advance;
        samp_frac_num += st->frac_advance;

        if (samp_frac_num >= st->den_rate) {
            samp_frac_num -= st->den_rate;
            last_sample++;
        }
    }

    st->last_sample[channel_index] = last_sample;

    st->samp_frac_num[channel_index] = samp_frac_num;
    return out_sample;
}

/* This is the same as the previous function, except with a double-precision accumulator */
static int resampler_basic_direct_double(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    int N = st->filt_len;
    int out_sample = 0;
    float *mem;
    int last_sample = st->last_sample[channel_index];
    unsigned int samp_frac_num = st->samp_frac_num[channel_index];

    mem = st->mem + channel_index * st->mem_alloc_size;

    while (!(last_sample >= (int)*in_len || out_sample >= (int)*out_len)) {

        int j;
        double sum = 0;

        /* We already have all the filter coefficients pre-computed in
         * the table */
        const float *ptr;

        for (j = 0; last_sample - N + 1 + j < 0; j++) {
            sum += ((float)(mem[last_sample+j]) *
		    (float)((double)st->sinc_table[samp_frac_num*st->filt_len+j]));
        }

        /* Do the new part */
        if (in != NULL) {
            ptr = in + st->in_stride * (last_sample - N + 1 + j);

            for (; j < N; j++) {
                sum += ((float)(*ptr) *
			(float)((double)st->sinc_table[samp_frac_num*st->filt_len+j]));
                ptr += st->in_stride;
            }
        }

        *out = sum;

        out += st->out_stride;
        out_sample++;
        last_sample += st->int_advance;
        samp_frac_num += st->frac_advance;

        if (samp_frac_num >= st->den_rate) {
            samp_frac_num -= st->den_rate;
            last_sample++;
        }
    }

    st->last_sample[channel_index] = last_sample;

    st->samp_frac_num[channel_index] = samp_frac_num;
    return out_sample;
}

static int resampler_basic_interpolate_single(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    int N = st->filt_len;
    int out_sample = 0;
    float *mem;
    int last_sample = st->last_sample[channel_index];
    unsigned int samp_frac_num = st->samp_frac_num[channel_index];

    mem = st->mem + channel_index * st->mem_alloc_size;

    while (!(last_sample >= (int)*in_len || out_sample >= (int)*out_len)) {

        int j;
        float sum = 0;

        /* We need to interpolate the sinc filter */
        float accum[4] = {0.f, 0.f, 0.f, 0.f};
        float interp[4];
        const float *ptr;
        int offset;
        float frac;

        offset = samp_frac_num * st->oversample / st->den_rate;

        frac = ((float)((samp_frac_num * st->oversample) % st->den_rate))
	    / st->den_rate;

        /* This code is written like this to make it easy to optimise
	 * with SIMD.  For most DSPs, it would be best to split the
	 * loops in two because most DSPs have only two
	 * accumulators */

        for (j = 0; last_sample - N + 1 + j < 0; j++) {

            float curr_mem = mem[last_sample+j];

            accum[0] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset - 2]));
            accum[1] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset - 1]));
            accum[2] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset]));
            accum[3] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset + 1]));
        }

        if (in != NULL) {

            ptr = in + st->in_stride * (last_sample - N + 1 + j);

            /* Do the new part */
            for (; j < N; j++) {

                float curr_in = *ptr;
                ptr += st->in_stride;

                accum[0] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset - 2]));
                accum[1] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset - 1]));
                accum[2] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset]));
                accum[3] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset + 1]));
            }
        }

        cubic_coef(frac, interp);

        sum =
	    ((interp[0]) * (accum[0])) +
	    ((interp[1]) * (accum[1])) +
	    ((interp[2]) * (accum[2])) +
	    ((interp[3]) * (accum[3]));

        *out = (sum);
        out += st->out_stride;
        out_sample++;
        last_sample += st->int_advance;
        samp_frac_num += st->frac_advance;

        if (samp_frac_num >= st->den_rate) {
            samp_frac_num -= st->den_rate;
            last_sample++;
        }
    }

    st->last_sample[channel_index] = last_sample;
    st->samp_frac_num[channel_index] = samp_frac_num;
    return out_sample;
}

/* This is the same as the previous function, except with a
 * double-precision accumulator */
static int resampler_basic_interpolate_double(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len) 
{
    int N = st->filt_len;
    int out_sample = 0;
    float *mem;
    int last_sample = st->last_sample[channel_index];
    unsigned int samp_frac_num = st->samp_frac_num[channel_index];

    mem = st->mem + channel_index * st->mem_alloc_size;

    while (!(last_sample >= (int)*in_len || out_sample >= (int)*out_len)) {

        int j;
        float sum = 0;

        /* We need to interpolate the sinc filter */
        double accum[4] = {0.f, 0.f, 0.f, 0.f};
        float interp[4];
        const float *ptr;
        float alpha = ((float)samp_frac_num) / st->den_rate;
        int offset = samp_frac_num * st->oversample / st->den_rate;
        float frac = alpha * st->oversample - offset;

        /* This code is written like this to make it easy to optimise
	 * with SIMD.  For most DSPs, it would be best to split the
	 * loops in two because most DSPs have only two
	 * accumulators */

        for (j = 0; last_sample - N + 1 + j < 0; j++) {

            double curr_mem = mem[last_sample + j];

            accum[0] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset - 2]));
            accum[1] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset - 1]));
            accum[2] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset]));
            accum[3] += ((float)(curr_mem) *
                         (float)(st->sinc_table
                                 [4 + (j+1)*st->oversample - offset + 1]));
        }

        if (in != NULL) {

            ptr = in + st->in_stride * (last_sample - N + 1 + j);

            /* Do the new part */
            for (; j < N; j++) {

                double curr_in = *ptr;
                ptr += st->in_stride;

                accum[0] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset - 2]));
                accum[1] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset - 1]));
                accum[2] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset]));
                accum[3] += ((float)(curr_in) *
                             (float)(st->sinc_table
                                     [4 + (j+1)*st->oversample - offset + 1]));
            }
        }

        cubic_coef(frac, interp);

        sum =
	    interp[0] * accum[0] +
	    interp[1] * accum[1] +
	    interp[2] * accum[2] +
	    interp[3] * accum[3];

        *out = (sum);
        out += st->out_stride;
        out_sample++;
        last_sample += st->int_advance;
        samp_frac_num += st->frac_advance;

        if (samp_frac_num >= st->den_rate) {
            samp_frac_num -= st->den_rate;
            last_sample++;
        }
    }

    st->last_sample[channel_index] = last_sample;
    st->samp_frac_num[channel_index] = samp_frac_num;

    return out_sample;
}

static void update_filter(SpeexResamplerState *st)
{
    unsigned int old_length;

    /*   fprintf(stderr, "update_filter\n"); */

    old_length = st->filt_len;
    st->oversample = quality_map[st->quality].oversample;
    st->filt_len = quality_map[st->quality].base_length;

    if (st->num_rate > st->den_rate) {

        /* down-sampling */
        st->cutoff = quality_map[st->quality].downsample_bandwidth
            * st->den_rate / st->num_rate;

        st->filt_len = (unsigned int)
            ceil(st->filt_len * ((double)st->num_rate / (double)st->den_rate));

        /* Round down to make sure we have a multiple of 4 */
        st->filt_len &= (~0x3);

        if (2*st->den_rate < st->num_rate)
            st->oversample >>= 1;

        if (4*st->den_rate < st->num_rate)
            st->oversample >>= 1;

        if (8*st->den_rate < st->num_rate)
            st->oversample >>= 1;

        if (16*st->den_rate < st->num_rate)
            st->oversample >>= 1;

        if (st->oversample < 1)
            st->oversample = 1;

    } else {

        /* up-sampling */
        st->cutoff = quality_map[st->quality].upsample_bandwidth;
    }

    /* Choose the resampling type that requires the least amount of memory */

    if (st->den_rate <= st->oversample) {

        unsigned int i;

        if (!st->sinc_table) {

            st->sinc_table = (float *)speex_alloc
                (st->filt_len * st->den_rate, sizeof(float));

	} else if (st->sinc_table_alloc < st->filt_len*st->den_rate) {

//		fprintf(stderr,"sinc_table=%p\n",st->sinc_table);
            st->sinc_table = (float *)speex_realloc
                (st->sinc_table, st->sinc_table_alloc,
                 st->filt_len * st->den_rate, sizeof(float));
            st->sinc_table_alloc = st->filt_len * st->den_rate;
        }

        for (i = 0; i < st->den_rate; i++) {

            int j;

            for (j = 0; j < st->filt_len; j++) {
                st->sinc_table[i*st->filt_len+j] = sinc
                    (st->cutoff,
                     ((j - (int)st->filt_len / 2 + 1) - ((float)i) / st->den_rate), 
                     st->filt_len,
                     quality_map[st->quality].window_func);
            }
        }

        if (st->quality > 8) {
            st->resampler_ptr = resampler_basic_direct_double;
        } else {
            st->resampler_ptr = resampler_basic_direct_single;
        }

        /*      fprintf (stderr, "resampler uses direct sinc table and normalised cutoff %f\n", st->cutoff); */

    } else {

        int i;

        if (!st->sinc_table) {

            st->sinc_table = (float *)speex_alloc
                ((st->filt_len * st->oversample + 8),  sizeof(float));

	} else if (st->sinc_table_alloc < st->filt_len*st->oversample + 8) {

		//fprintf(stderr,"sinc_table=%p\n",st->sinc_table);
            st->sinc_table = (float *)speex_realloc
                (st->sinc_table, st->sinc_table_alloc,
                 (st->filt_len * st->oversample + 8), sizeof(float));
            st->sinc_table_alloc = st->filt_len * st->oversample + 8;
        }

        for (i = -4; i < (int)(st->oversample * st->filt_len + 4); i++) {
            st->sinc_table[i+4] = sinc
                (st->cutoff,
                 (i / (float)st->oversample - st->filt_len / 2),
                 st->filt_len,
                 quality_map[st->quality].window_func);
	}

        if (st->quality > 8)
            st->resampler_ptr = resampler_basic_interpolate_double;
        else
            st->resampler_ptr = resampler_basic_interpolate_single;

        /* fprintf (stderr, "resampler uses interpolated sinc table and normalised cutoff %f\n", st->cutoff); */

        /* fprintf (stderr, "table length %d, filt len %d\n", st->sinc_table_length, st->filt_len); */
    }

    st->int_advance = st->num_rate / st->den_rate;
    st->frac_advance = st->num_rate % st->den_rate;

    /* Here's the place where we update the filter memory to take into
       account the change in filter length. It's probably the messiest
       part of the code due to handling of lots of corner cases. */

    if (!st->mem) {

        unsigned int i;
        st->mem = (float*)speex_alloc
            (st->nb_channels * (st->filt_len - 1), sizeof(float));

        for (i = 0; i < st->nb_channels * (st->filt_len - 1); i++)
            st->mem[i] = 0;

        st->mem_alloc_size = st->filt_len - 1;

    } else if (!st->started) {

        unsigned int i;

		//fprintf(stderr,"mem=%p\n",st->mem);
		st->mem = (float*)speex_realloc
            (st->mem, 0, st->nb_channels * (st->filt_len - 1), sizeof(float));

        for (i = 0; i < st->nb_channels * (st->filt_len - 1); i++)
            st->mem[i] = 0;

        st->mem_alloc_size = st->filt_len - 1;

    } else if (st->filt_len > old_length) {

        int i;

        /* Increase the filter length */

        int old_alloc_size = st->mem_alloc_size;

        if (st->filt_len - 1 > st->mem_alloc_size) {
			
		//fprintf(stderr,"mem=%p\n",st->mem);

            st->mem = (float*)speex_realloc
                (st->mem, st->nb_channels * (old_length - 1),
                 st->nb_channels * (st->filt_len - 1), sizeof(float));
            st->mem_alloc_size = st->filt_len - 1;
        }

        for (i = st->nb_channels - 1; i >= 0; i--) {

            int j;
            unsigned int olen = old_length;

	    /*if (st->magic_samples[i])*/
            {

                /* Try and remove the magic samples as if nothing had happened */

                /* FIXME: This is wrong but for now we need it to
                 * avoid going over the array bounds */

                olen = old_length + 2 * st->magic_samples[i];

                for (j = old_length - 2 + st->magic_samples[i]; j >= 0; j--) {
                    st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]] =
                        st->mem[i*old_alloc_size+j];
                }

                for (j = 0; j < st->magic_samples[i]; j++) {
                    st->mem[i*st->mem_alloc_size+j] = 0;
                }

                st->magic_samples[i] = 0;
            }

            if (st->filt_len > olen) {

                /* If the new filter length is still bigger than the
                 * "augmented" length */

                /* Copy data going backward */

                for (j = 0; j < olen - 1; j++) {
                    st->mem[i*st->mem_alloc_size+(st->filt_len-2-j)] =
                        st->mem[i*st->mem_alloc_size+(olen-2-j)];
                }

                /* Then put zeros for lack of anything better */
                for (; j < st->filt_len - 1; j++) {
                    st->mem[i*st->mem_alloc_size+(st->filt_len-2-j)] = 0;
                }

                /* Adjust last_sample */
                st->last_sample[i] += (st->filt_len - olen) / 2;

            } else {

                /* Put back some of the magic! */
                st->magic_samples[i] = (olen - st->filt_len) / 2;

                for (j = 0; j < st->filt_len - 1 + st->magic_samples[i]; j++) {
                    st->mem[i*st->mem_alloc_size+j] =
                        st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]];
                }
            }
        }
    } else if (st->filt_len < old_length) {

        unsigned int i;

        /* Reduce filter length, this a bit tricky. We need to store
           some of the memory as "magic" samples so they can be used
           directly as input the next time(s) */

        for (i = 0; i < st->nb_channels; i++) {

            unsigned int j;
            unsigned int old_magic = st->magic_samples[i];
            st->magic_samples[i] = (old_length - st->filt_len) / 2;

            /* We must copy some of the memory that's no longer used */
            /* Copy data going backward */

            for (j = 0; j < st->filt_len - 1 + st->magic_samples[i] + old_magic; j++) {
                st->mem[i*st->mem_alloc_size+j] =
                    st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]];
	    }

            st->magic_samples[i] += old_magic;
        }
    }
}

SpeexResamplerState *speex_resampler_init(unsigned int nb_channels, unsigned int in_rate, unsigned int out_rate, int quality, int *err)
{
    return speex_resampler_init_frac(nb_channels, in_rate, out_rate,
				     in_rate, out_rate, quality, err);
}

SpeexResamplerState *speex_resampler_init_frac(unsigned int nb_channels, unsigned int ratio_num, unsigned int ratio_den, unsigned int in_rate, unsigned int out_rate, int quality, int *err)
{
    unsigned int i;
    SpeexResamplerState *st;

    if (quality > 10 || quality < 0) {
        if (err) *err = RESAMPLER_ERR_INVALID_ARG;
        return NULL;
    }

    st = (SpeexResamplerState *)speex_alloc(1, sizeof(SpeexResamplerState));

    st->initialised = 0;
    st->started = 0;
    st->in_rate = 0;
    st->out_rate = 0;
    st->num_rate = 0;
    st->den_rate = 0;
    st->quality = -1;
	st->sinc_table = 0;
    st->sinc_table_length = 0;
    st->sinc_table_alloc = 0;
    st->mem_alloc_size = 0;
    st->filt_len = 0;
    st->mem = 0;
    st->resampler_ptr = 0;

    st->cutoff = 1.f;
    st->nb_channels = nb_channels;
    st->in_stride = 1;
    st->out_stride = 1;

    /* Per channel data */
    st->last_sample = (int*)speex_alloc(nb_channels, sizeof(int));
    st->magic_samples = (unsigned int*)speex_alloc(nb_channels, sizeof(int));
    st->samp_frac_num = (unsigned int*)speex_alloc(nb_channels, sizeof(int));

    for (i = 0; i < nb_channels; i++) {
        st->last_sample[i] = 0;
        st->magic_samples[i] = 0;
        st->samp_frac_num[i] = 0;
    }

    speex_resampler_set_quality(st, quality);
    speex_resampler_set_rate_frac(st, ratio_num, ratio_den, in_rate, out_rate);

    update_filter(st);

    st->initialised = 1;

    if (err) *err = RESAMPLER_ERR_SUCCESS;

    return st;
}

void speex_resampler_destroy(SpeexResamplerState *st)
{
    speex_free(st->mem);
    speex_free(st->sinc_table);
    speex_free(st->last_sample);
    speex_free(st->magic_samples);
    speex_free(st->samp_frac_num);
    speex_free(st);
}

static int speex_resampler_process_native(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    int j = 0;
    int N = st->filt_len;
    int out_sample = 0;
    float *mem;
    unsigned int tmp_out_len = 0;

    mem = st->mem + channel_index * st->mem_alloc_size;
    st->started = 1;

    /* Handle the case where we have samples left from a reduction in
     * filter length */

    if (st->magic_samples[channel_index]) {

        int istride_save;
        unsigned int tmp_in_len;
        unsigned int tmp_magic;

        istride_save = st->in_stride;
        tmp_in_len = st->magic_samples[channel_index];
        tmp_out_len = *out_len;

        /* magic_samples needs to be set to zero to avoid infinite recursion */
        tmp_magic = st->magic_samples[channel_index];
        st->magic_samples[channel_index] = 0;
        st->in_stride = 1;
        speex_resampler_process_native(st, channel_index, mem + N-1,
                                       &tmp_in_len, out, &tmp_out_len);
        st->in_stride = istride_save;

        /* If we couldn't process all "magic" input samples, save the
         * rest for next time */

        if (tmp_in_len < tmp_magic) {

            unsigned int i;

            st->magic_samples[channel_index] = tmp_magic - tmp_in_len;

            for (i = 0; i < st->magic_samples[channel_index]; i++) {
                mem[N-1+i] = mem[N-1+i+tmp_in_len];
            }
        }

        out += tmp_out_len * st->out_stride;
        *out_len -= tmp_out_len;
    }

    /* Call the right resampler through the function ptr */
    out_sample = st->resampler_ptr(st, channel_index,
                                   in, in_len, out, out_len);

    if (st->last_sample[channel_index] < (int)*in_len) {
        *in_len = st->last_sample[channel_index];
    }

    *out_len = out_sample + tmp_out_len;

    st->last_sample[channel_index] -= *in_len;

    for (j = 0; j < N-1 - (int)*in_len; j++) {
        mem[j] = mem[j+*in_len];
    }

    if (in != NULL) {
        for ( ; j < N-1; j++) mem[j] = in[st->in_stride*(j+*in_len-N+1)];
    } else {
        for ( ; j < N-1; j++) mem[j] = 0;
    }

    return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_process_float(SpeexResamplerState *st, unsigned int channel_index, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    return speex_resampler_process_native(st, channel_index, in, in_len, out, out_len);
}

int speex_resampler_process_interleaved_float(SpeexResamplerState *st, const float *in, unsigned int *in_len, float *out, unsigned int *out_len)
{
    unsigned int i;
    int istride_save, ostride_save;
    unsigned int bak_len = *out_len;

    istride_save = st->in_stride;
    ostride_save = st->out_stride;
    st->in_stride = st->out_stride = st->nb_channels;

    for (i = 0; i < st->nb_channels; i++) {

        *out_len = bak_len;

        if (in != NULL) {
            speex_resampler_process_float(st, i, in + i, in_len, out + i, out_len);
        } else {
            speex_resampler_process_float(st, i, NULL, in_len, out + i, out_len);
        }
    }

    st->in_stride = istride_save;
    st->out_stride = ostride_save;

    return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_set_rate(SpeexResamplerState *st, unsigned int in_rate, unsigned int out_rate)
{
    return speex_resampler_set_rate_frac(st, in_rate, out_rate, in_rate, out_rate);
}

void speex_resampler_get_rate(SpeexResamplerState *st, unsigned int *in_rate, unsigned int *out_rate)
{
    *in_rate = st->in_rate;
    *out_rate = st->out_rate;
}

static unsigned int gcd(unsigned int a, unsigned int b)
{
    /* Euclid */

    while (b) {
        unsigned int tmp = b;
        b = a % b;
        a = tmp;
    }

    return a;
}

int speex_resampler_set_rate_frac(SpeexResamplerState *st, unsigned int ratio_num, unsigned int ratio_den, unsigned int in_rate, unsigned int out_rate)
{
    unsigned int old_den;
    unsigned int i;
	unsigned int g;

    if (st->in_rate == in_rate && st->out_rate == out_rate &&
	st->num_rate == ratio_num && st->den_rate == ratio_den) {
        return RESAMPLER_ERR_SUCCESS;
    }

    old_den = st->den_rate;

    st->in_rate = in_rate;
    st->out_rate = out_rate;

    st->num_rate = ratio_num;
    st->den_rate = ratio_den;

    g = gcd(st->num_rate, st->den_rate);

    st->num_rate /= g;
    st->den_rate /= g;

    if (old_den > 0) {

        for (i = 0; i < st->nb_channels; i++) {

            st->samp_frac_num[i] = st->samp_frac_num[i] * st->den_rate / old_den;

            if (st->samp_frac_num[i] >= st->den_rate) {
                st->samp_frac_num[i] = st->den_rate - 1;
	    }
        }
    }

    if (st->initialised) {
        update_filter(st);
    }

    return RESAMPLER_ERR_SUCCESS;
}

void speex_resampler_get_ratio(SpeexResamplerState *st, unsigned int *ratio_num, unsigned int *ratio_den)
{
    *ratio_num = st->num_rate;
    *ratio_den = st->den_rate;
}

int speex_resampler_set_quality(SpeexResamplerState *st, int quality)
{
    if (quality > 10 || quality < 0) {
        return RESAMPLER_ERR_INVALID_ARG;
    }

    if (st->quality == quality) {
        return RESAMPLER_ERR_SUCCESS;
    }

    st->quality = quality;

    if (st->initialised) {
        update_filter(st); 
    }

    return RESAMPLER_ERR_SUCCESS;
}

void speex_resampler_get_quality(SpeexResamplerState *st, int *quality)
{
    *quality = st->quality;
}

void speex_resampler_set_input_stride(SpeexResamplerState *st, unsigned int stride)
{
    st->in_stride = stride;
}

void speex_resampler_get_input_stride(SpeexResamplerState *st, unsigned int *stride)
{
    *stride = st->in_stride;
}

void speex_resampler_set_output_stride(SpeexResamplerState *st, unsigned int stride)
{
    st->out_stride = stride;
}

void speex_resampler_get_output_stride(SpeexResamplerState *st, unsigned int *stride)
{
    *stride = st->out_stride;
}

int speex_resampler_get_input_latency(SpeexResamplerState *st)
{
    return st->filt_len / 2;
}

int speex_resampler_get_output_latency(SpeexResamplerState *st) 
{
    return ((st->filt_len / 2) * st->den_rate + (st->num_rate >> 1)) / st->num_rate;
}

int speex_resampler_skip_zeros(SpeexResamplerState *st)
{
    unsigned int i;

    for (i = 0; i < st->nb_channels; i++) {
        st->last_sample[i] = st->filt_len / 2;
    }

    return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_reset_mem(SpeexResamplerState *st)
{
    unsigned int i;

    for (i = 0; i < st->nb_channels*(st->filt_len - 1); i++) {
        st->mem[i] = 0;
    }

    for (i = 0; i < st->nb_channels; i++) {
        st->last_sample[i] = 0;
        st->magic_samples[i] = 0;
        st->samp_frac_num[i] = 0;
    }

    return RESAMPLER_ERR_SUCCESS;
}

const char *speex_resampler_strerror(int err)
{
    switch (err) {

    case RESAMPLER_ERR_SUCCESS:
        return "Success.";

    case RESAMPLER_ERR_ALLOC_FAILED:
        return "Memory allocation failed.";

    case RESAMPLER_ERR_BAD_STATE:
        return "Bad resampler state.";

    case RESAMPLER_ERR_INVALID_ARG:
        return "Invalid argument.";

    case RESAMPLER_ERR_PTR_OVERLAP:
        return "Input and output buffers overlap.";

    default:
        return "Unknown error. Bad error code or strange version mismatch.";
    }
}
