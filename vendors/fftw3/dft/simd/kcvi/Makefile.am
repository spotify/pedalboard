AM_CFLAGS = $(KCVI_CFLAGS)
SIMD_HEADER=simd-support/simd-kcvi.h

include $(top_srcdir)/dft/simd/codlist.mk
include $(top_srcdir)/dft/simd/simd.mk

if HAVE_KCVI

BUILT_SOURCES = $(EXTRA_DIST)
noinst_LTLIBRARIES = libdft_kcvi_codelets.la
libdft_kcvi_codelets_la_SOURCES = $(BUILT_SOURCES)

endif
