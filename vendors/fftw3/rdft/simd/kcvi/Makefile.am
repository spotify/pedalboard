AM_CFLAGS = $(KCVI_CFLAGS)
SIMD_HEADER=simd-support/simd-kcvi.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_KCVI

noinst_LTLIBRARIES = librdft_kcvi_codelets.la
BUILT_SOURCES = $(EXTRA_DIST)
librdft_kcvi_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


