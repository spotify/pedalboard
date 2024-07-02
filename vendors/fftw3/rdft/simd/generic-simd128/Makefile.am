SIMD_HEADER=simd-support/simd-generic128.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_GENERIC_SIMD128

noinst_LTLIBRARIES = librdft_generic_simd128_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_generic_simd128_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
