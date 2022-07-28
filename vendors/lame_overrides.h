/**
 * LAME's MP3 decoding routines are not thread-safe, and the functions that
 * would need to be called to make them thread-safe from user code are `static`
 * (i.e.: private) to the C file that contains them.
 */

#include "lame/include/lame.h"

/*
 * For hip_decode:  return code
 *  -1     error
 *   0     ok, but need more data before outputing any samples
 *   n     number of samples output.  a multiple of 576 or 1152 depending on MP3
 * file.
 */
int hip_decode_threadsafe(hip_t hip, unsigned char *buffer, size_t len,
                          short pcm_l[], short pcm_r[]);