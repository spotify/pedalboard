AM_CFLAGS = $(VSX_CFLAGS)
SIMD_HEADER=simd-support/simd-vsx.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_VSX

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_vsx_codelets.la
libdft_vsx_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
