#include "libbench2/bench.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void ovtpvt(const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  if (verbose >= 0)
    vfprintf(stdout, format, ap);
  va_end(ap);
  fflush(stdout);
}

void ovtpvt_err(const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  if (verbose >= 0) {
    fflush(stdout);
    vfprintf(stderr, format, ap);
  }
  va_end(ap);
  fflush(stdout);
}
