AM_CFLAGS = $(ALTIVEC_CFLAGS)
SIMD_HEADER=simd-support/simd-altivec.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_ALTIVEC

noinst_LTLIBRARIES = librdft_altivec_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_altivec_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
