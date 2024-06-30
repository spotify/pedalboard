AM_CFLAGS = $(AVX2_CFLAGS)
SIMD_HEADER=simd-support/simd-avx2.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_AVX2

noinst_LTLIBRARIES = librdft_avx2_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_avx2_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


