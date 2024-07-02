AM_CFLAGS = $(AVX_CFLAGS)
SIMD_HEADER=simd-support/simd-avx.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_AVX

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_avx_codelets.la
libdft_avx_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
