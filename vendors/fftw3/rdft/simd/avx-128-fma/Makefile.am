AM_CFLAGS = $(AVX_128_FMA_CFLAGS)
SIMD_HEADER=simd-support/simd-avx-128-fma.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_AVX_128_FMA

noinst_LTLIBRARIES = librdft_avx_128_fma_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_avx_128_fma_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


