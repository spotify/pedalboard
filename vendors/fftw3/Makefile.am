OPTIONS_AUTOMAKE=gnu
lib_LTLIBRARIES = libfftw3@PREC_SUFFIX@.la

# pkgincludedir = $(includedir)/fftw3@PREC_SUFFIX@
# nodist_pkginclude_HEADERS = config.h

# recompile genfft if maintainer mode is true
if MAINTAINER_MODE
GENFFT = genfft
EXTRA_libfftw3@PREC_SUFFIX@_la_DEPENDENCIES = assert-shared-version-info
else
GENFFT =
endif

ACLOCAL_AMFLAGS=-I m4

# when using combined thread libraries (necessary on Windows), we want
# to build threads/ first, because libfftw3_threads is added to
# libfftw3.
#
# Otherwise, we want to build libfftw3_threads after libfftw3
# so that we can track the fact that libfftw3_threads depends upon
# libfftw3.
#
# This is the inescapable result of combining three bad ideas
# (threads, Windows, and shared libraries).
#
if COMBINED_THREADS
CHICKEN_EGG=threads .
else
CHICKEN_EGG=. threads
endif

# Only build in doc/ if not disabled by user (i.e. not all
# tools are available, such as fig2dev in maintainer mode)
if BUILD_DOC
DOCDIR=doc
else
DOCDIR=
endif

SUBDIRS=support $(GENFFT) kernel simd-support dft rdft reodft api	\
libbench2 $(CHICKEN_EGG) tests mpi $(DOCDIR) tools m4
EXTRA_DIST=COPYRIGHT bootstrap.sh CONVENTIONS fftw.pc.in	\
CMakeLists.txt cmake.config.h.in FFTW3Config.cmake.in		\
FFTW3ConfigVersion.cmake.in README-perfcnt.md

SIMD_LIBS = simd-support/libsimd_support.la

if HAVE_SSE2
SSE2_LIBS = dft/simd/sse2/libdft_sse2_codelets.la	\
rdft/simd/sse2/librdft_sse2_codelets.la
endif

if HAVE_AVX
AVX_LIBS = dft/simd/avx/libdft_avx_codelets.la	\
rdft/simd/avx/librdft_avx_codelets.la
endif

if HAVE_AVX_128_FMA
AVX_128_FMA_LIBS = dft/simd/avx-128-fma/libdft_avx_128_fma_codelets.la \
rdft/simd/avx-128-fma/librdft_avx_128_fma_codelets.la
endif

if HAVE_AVX2
AVX2_LIBS = dft/simd/avx2/libdft_avx2_codelets.la	\
dft/simd/avx2-128/libdft_avx2_128_codelets.la \
rdft/simd/avx2/librdft_avx2_codelets.la \
rdft/simd/avx2-128/librdft_avx2_128_codelets.la
endif

if HAVE_AVX512
AVX512_LIBS = dft/simd/avx512/libdft_avx512_codelets.la	\
rdft/simd/avx512/librdft_avx512_codelets.la
endif

if HAVE_KCVI
KCVI_LIBS = dft/simd/kcvi/libdft_kcvi_codelets.la	\
rdft/simd/kcvi/librdft_kcvi_codelets.la
endif

if HAVE_ALTIVEC
ALTIVEC_LIBS = dft/simd/altivec/libdft_altivec_codelets.la	\
rdft/simd/altivec/librdft_altivec_codelets.la
endif

if HAVE_VSX
VSX_LIBS = dft/simd/vsx/libdft_vsx_codelets.la      \
rdft/simd/vsx/librdft_vsx_codelets.la
endif

if HAVE_NEON
NEON_LIBS = dft/simd/neon/libdft_neon_codelets.la	\
rdft/simd/neon/librdft_neon_codelets.la
endif

if HAVE_GENERIC_SIMD128
GENERIC_SIMD128_LIBS = dft/simd/generic-simd128/libdft_generic_simd128_codelets.la \
rdft/simd/generic-simd128/librdft_generic_simd128_codelets.la
endif

if HAVE_GENERIC_SIMD256
GENERIC_SIMD256_LIBS = dft/simd/generic-simd256/libdft_generic_simd256_codelets.la      \
rdft/simd/generic-simd256/librdft_generic_simd256_codelets.la
endif

if THREADS
if COMBINED_THREADS
COMBINED_THREADLIBS=threads/libfftw3@PREC_SUFFIX@_threads.la
endif
endif

libfftw3@PREC_SUFFIX@_la_SOURCES = 

libfftw3@PREC_SUFFIX@_la_LIBADD =			\
	kernel/libkernel.la				\
	dft/libdft.la					\
	dft/scalar/libdft_scalar.la			\
	dft/scalar/codelets/libdft_scalar_codelets.la	\
	rdft/librdft.la					\
	rdft/scalar/librdft_scalar.la			\
	rdft/scalar/r2cf/librdft_scalar_r2cf.la		\
	rdft/scalar/r2cb/librdft_scalar_r2cb.la		\
	rdft/scalar/r2r/librdft_scalar_r2r.la		\
	reodft/libreodft.la				\
	api/libapi.la					\
        $(SIMD_LIBS) $(SSE2_LIBS) $(AVX_LIBS) $(AVX_128_FMA_LIBS) \
        $(AVX2_LIBS) $(ALTIVEC_LIBS) \
        $(VSX_LIBS) $(NEON_LIBS) $(KCVI_LIBS) $(AVX512_LIBS) \
        $(GENERIC_SIMD128_LIBS) $(GENERIC_SIMD256_LIBS) \
	$(COMBINED_THREADLIBS)

if QUAD
# cannot use -no-undefined since dependent on libquadmath
libfftw3@PREC_SUFFIX@_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@ $(ENVIRONMENT_LIBFFTW3_LDFLAGS)
else
libfftw3@PREC_SUFFIX@_la_LDFLAGS = -no-undefined -version-info	\
@SHARED_VERSION_INFO@  $(ENVIRONMENT_LIBFFTW3_LDFLAGS)
endif

fftw3@PREC_SUFFIX@.pc: fftw.pc
	cp -f fftw.pc fftw3@PREC_SUFFIX@.pc
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fftw3@PREC_SUFFIX@.pc

FFTW3@PREC_SUFFIX@Config.cmake: $(top_srcdir)/FFTW3Config.cmake.in
	$(SED) \
		-e 's|[@]PREC_SUFFIX@|@PREC_SUFFIX@|g' \
		-e 's|[@]CMAKE_INSTALL_FULL_LIBDIR@|$(libdir)|g' \
		-e 's|[@]CMAKE_INSTALL_FULL_INCLUDEDIR@|$(includedir)|g' \
		$(top_srcdir)/FFTW3Config.cmake.in > $@
FFTW3@PREC_SUFFIX@ConfigVersion.cmake: $(top_srcdir)/FFTW3ConfigVersion.cmake.in
	$(SED) \
		-e 's|[@]FFTW_VERSION@|@PACKAGE_VERSION@|g' \
		$(top_srcdir)/FFTW3ConfigVersion.cmake.in > $@
cmakedir = $(libdir)/cmake/fftw3
cmake_DATA = FFTW3@PREC_SUFFIX@Config.cmake FFTW3@PREC_SUFFIX@ConfigVersion.cmake

WISDOM_DIR = /etc/fftw
WISDOM = wisdom@PREC_SUFFIX@

WISDOM_TIME=12 # default to 12-hour limit, i.e. overnight
WISDOM_FLAGS=--verbose --canonical --time-limit=$(WISDOM_TIME)

wisdom:
	tools/fftw@PREC_SUFFIX@-wisdom -o $@ $(WISDOM_FLAGS)

install-wisdom: wisdom
	$(mkinstalldirs) $(WISDOM_DIR)
	$(INSTALL_DATA) wisdom $(WISDOM_DIR)/$(WISDOM)

if MAINTAINER_MODE
assert-shared-version-info:
	current=`echo @SHARED_VERSION_INFO@ | cut -d: -f1`;	\
	age=`echo @SHARED_VERSION_INFO@ | cut -d: -f3`;		\
	major=3;						\
	expected=`expr $$age + $$major`;			\
	test $$current -eq $$expected
endif
