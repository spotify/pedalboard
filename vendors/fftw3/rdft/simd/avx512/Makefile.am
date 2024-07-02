AM_CFLAGS = $(AVX512_CFLAGS)
SIMD_HEADER=simd-support/simd-avx512.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_AVX512

noinst_LTLIBRARIES = librdft_avx512_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_avx512_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


