AM_CPPFLAGS = -I $(top_srcdir)
bin_SCRIPTS = fftw-wisdom-to-conf
bin_PROGRAMS = fftw@PREC_SUFFIX@-wisdom

BUILT_SOURCES = fftw-wisdom-to-conf fftw@PREC_SUFFIX@-wisdom.1
EXTRA_DIST = fftw-wisdom-to-conf.in

dist_man_MANS = fftw-wisdom-to-conf.1 fftw@PREC_SUFFIX@-wisdom.1
EXTRA_MANS = fftw_wisdom.1.in
fftw@PREC_SUFFIX@-wisdom.1: fftw_wisdom.1
	rm -f $@
	cp fftw_wisdom.1 $@

if THREADS
fftw@PREC_SUFFIX@_wisdom_CFLAGS = $(PTHREAD_CFLAGS)
if !COMBINED_THREADS
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_threads.la
endif
else
if OPENMP
fftw@PREC_SUFFIX@_wisdom_CFLAGS = $(OPENMP_CFLAGS)
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_omp.la
endif
endif

fftw@PREC_SUFFIX@_wisdom_SOURCES = fftw-wisdom.c
fftw@PREC_SUFFIX@_wisdom_LDADD = $(top_builddir)/tests/bench-bench.o	\
$(top_builddir)/tests/bench-fftw-bench.o $(LIBFFTWTHREADS)	\
$(top_builddir)/libfftw3@PREC_SUFFIX@.la			\
$(top_builddir)/libbench2/libbench2.a $(THREADLIBS)
