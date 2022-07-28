/**
 * LAME's MP3 decoding routines are not thread-safe, and the functions that
 * would need to be called to make them thread-safe from user code are `static`
 * (i.e.: private) to the C file that contains them.
 */

#include "lame_overrides.h"

#include "lame/libmp3lame/machine.h"

#include "lame/libmp3lame/encoder.h"

#include "lame/libmp3lame/util.h"
#include "lame/mpglib/interface.h"
#include "lame/mpglib/mpglib.h"

#define hip_global_struct mpstr_tag

/* copy mono samples */
#define COPY_MONO(DST_TYPE, SRC_TYPE)                                          \
  {                                                                            \
    DST_TYPE *pcm_l = (DST_TYPE *)pcm_l_raw;                                   \
    SRC_TYPE const *p_samples = (SRC_TYPE const *)p;                           \
    for (i = 0; i < processed_samples; i++)                                    \
      *pcm_l++ = (DST_TYPE)(*p_samples++);                                     \
  }

/* copy stereo samples */
#define COPY_STEREO(DST_TYPE, SRC_TYPE)                                        \
  {                                                                            \
    DST_TYPE *pcm_l = (DST_TYPE *)pcm_l_raw, *pcm_r = (DST_TYPE *)pcm_r_raw;   \
    SRC_TYPE const *p_samples = (SRC_TYPE const *)p;                           \
    for (i = 0; i < processed_samples; i++) {                                  \
      *pcm_l++ = (DST_TYPE)(*p_samples++);                                     \
      *pcm_r++ = (DST_TYPE)(*p_samples++);                                     \
    }                                                                          \
  }

int decode1_headersB_clipchoice(
    PMPSTR pmp, unsigned char *buffer, size_t len, char pcm_l_raw[],
    char pcm_r_raw[], mp3data_struct *mp3data, int *enc_delay, int *enc_padding,
    char *p, size_t psize, int decoded_sample_size,
    int (*decodeMP3_ptr)(PMPSTR, unsigned char *, int, char *, int, int *)) {
  static const int smpls[2][4] = {
      /* Layer   I    II   III */
      {0, 384, 1152, 1152}, /* MPEG-1     */
      {0, 384, 1152, 576}   /* MPEG-2(.5) */
  };

  int processed_bytes;
  int processed_samples; /* processed samples per channel */
  int ret;
  int i;
  int const len_l = len < INT_MAX ? (int)len : INT_MAX;
  int const psize_l = psize < INT_MAX ? (int)psize : INT_MAX;

  mp3data->header_parsed = 0;
  ret = (*decodeMP3_ptr)(pmp, buffer, len_l, p, psize_l, &processed_bytes);
  /* three cases:
   * 1. headers parsed, but data not complete
   *       pmp->header_parsed==1
   *       pmp->framesize=0
   *       pmp->fsizeold=size of last frame, or 0 if this is first frame
   *
   * 2. headers, data parsed, but ancillary data not complete
   *       pmp->header_parsed==1
   *       pmp->framesize=size of frame
   *       pmp->fsizeold=size of last frame, or 0 if this is first frame
   *
   * 3. frame fully decoded:
   *       pmp->header_parsed==0
   *       pmp->framesize=0
   *       pmp->fsizeold=size of frame (which is now the last frame)
   *
   */
  if (pmp->header_parsed || pmp->fsizeold > 0 || pmp->framesize > 0) {
    mp3data->header_parsed = 1;
    mp3data->stereo = pmp->fr.stereo;
    mp3data->samplerate = freqs[pmp->fr.sampling_frequency];
    mp3data->mode = pmp->fr.mode;
    mp3data->mode_ext = pmp->fr.mode_ext;
    mp3data->framesize = smpls[pmp->fr.lsf][pmp->fr.lay];

    /* free format, we need the entire frame before we can determine
     * the bitrate.  If we haven't gotten the entire frame, bitrate=0 */
    if (pmp->fsizeold > 0) /* works for free format and fixed, no overrun,
                              temporal results are < 400.e6 */
      mp3data->bitrate = 8 * (4 + pmp->fsizeold) * mp3data->samplerate /
                             (1.e3 * mp3data->framesize) +
                         0.5;
    else if (pmp->framesize > 0)
      mp3data->bitrate = 8 * (4 + pmp->framesize) * mp3data->samplerate /
                             (1.e3 * mp3data->framesize) +
                         0.5;
    else
      mp3data->bitrate =
          tabsel_123[pmp->fr.lsf][pmp->fr.lay - 1][pmp->fr.bitrate_index];

    if (pmp->num_frames > 0) {
      /* Xing VBR header found and num_frames was set */
      mp3data->totalframes = pmp->num_frames;
      mp3data->nsamp = mp3data->framesize * pmp->num_frames;
      *enc_delay = pmp->enc_delay;
      *enc_padding = pmp->enc_padding;
    }
  }

  switch (ret) {
  case MP3_OK:
    switch (pmp->fr.stereo) {
    case 1:
      processed_samples = processed_bytes / decoded_sample_size;
      COPY_MONO(short, short)
      break;
    case 2:
      processed_samples = (processed_bytes / decoded_sample_size) >> 1;
      COPY_STEREO(short, short)
      break;
    default:
      processed_samples = -1;
      assert(0);
      break;
    }
    break;

  case MP3_NEED_MORE:
    processed_samples = 0;
    break;

  case MP3_ERR:
    processed_samples = -1;
    break;

  default:
    processed_samples = -1;
    assert(0);
    break;
  }

  /*fprintf(stderr,"ok, more, err:  %i %i %i\n", MP3_OK, MP3_NEED_MORE, MP3_ERR
   * ); */
  /*fprintf(stderr,"ret = %i out=%i\n", ret, processed_samples ); */
  return processed_samples;
}

#define OUTSIZE_CLIPPED (4096 * sizeof(short))

/*
 * For hip_decode:  return code
 *  -1     error
 *   0     ok, but need more data before outputing any samples
 *   n     number of samples output.  a multiple of 576 or 1152 depending on MP3
 * file.
 */
int hip_decode_threadsafe(hip_t hip, unsigned char *buffer, size_t len,
                          short pcm_l[], short pcm_r[]) {
  mp3data_struct mp3data;
  int ret;
  int totsize = 0; /* number of decoded samples per channel */
  int enc_delay, enc_padding;

  if (!hip) {
    return -1;
  }

  char out[OUTSIZE_CLIPPED];

  for (;;) {
    int ret = decode1_headersB_clipchoice(
        hip, buffer, len, (char *)pcm_l + totsize, (char *)pcm_r + totsize,
        &mp3data, &enc_delay, &enc_padding, out, OUTSIZE_CLIPPED, sizeof(short),
        decodeMP3);

    switch (ret) {
    case -1:
      return ret;
    case 0:
      return totsize;
    default:
      totsize += ret;
      len = 0; /* future calls to decodeMP3 are just to flush buffers */
      break;
    }
  }
}