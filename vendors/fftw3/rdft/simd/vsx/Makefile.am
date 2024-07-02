AM_CFLAGS = $(VSX_CFLAGS)
SIMD_HEADER=simd-support/simd-vsx.h

include $(top_srcdir)/rdft/simd/codlist.mk
include $(top_srcdir)/rdft/simd/simd.mk

if HAVE_VSX

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = librdft_vsx_codelets.la
librdft_vsx_codelets_la_SOURCES = $(BUILT_SOURCES)

endif


