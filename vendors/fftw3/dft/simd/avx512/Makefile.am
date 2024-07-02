AM_CFLAGS = $(AVX512_CFLAGS)
SIMD_HEADER=simd-support/simd-avx512.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_AVX512

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_avx512_codelets.la
libdft_avx512_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
