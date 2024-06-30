AM_CFLAGS = $(AVX2_CFLAGS)
SIMD_HEADER=simd-support/simd-avx2.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_AVX2

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_avx2_codelets.la
libdft_avx2_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
