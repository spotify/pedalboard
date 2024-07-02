AM_CFLAGS = $(NEON_CFLAGS)
SIMD_HEADER=simd-support/simd-neon.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_NEON

noinst_LTLIBRARIES = librdft_neon_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_neon_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
