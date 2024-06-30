AM_CFLAGS = $(AVX_128_FMA_CFLAGS)
SIMD_HEADER=simd-support/simd-avx-128-fma.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_AVX_128_FMA

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_avx_128_fma_codelets.la
libdft_avx_128_fma_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
