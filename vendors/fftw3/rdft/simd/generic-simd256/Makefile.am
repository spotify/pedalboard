SIMD_HEADER=simd-support/simd-generic256.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_GENERIC_SIMD256

noinst_LTLIBRARIES = librdft_generic_simd256_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_generic_simd256_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
