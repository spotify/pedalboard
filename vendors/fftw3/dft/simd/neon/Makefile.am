AM_CFLAGS = $(NEON_CFLAGS)
SIMD_HEADER=simd-support/simd-neon.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_NEON

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_neon_codelets.la
libdft_neon_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
