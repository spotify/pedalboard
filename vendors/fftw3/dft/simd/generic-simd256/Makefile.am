SIMD_HEADER=simd-support/simd-generic256.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_GENERIC_SIMD256

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_generic_simd256_codelets.la
libdft_generic_simd256_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
