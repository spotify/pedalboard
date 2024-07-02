AM_CFLAGS = $(AVX_CFLAGS)
SIMD_HEADER=simd-support/simd-avx.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_AVX

noinst_LTLIBRARIES = librdft_avx_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_avx_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


