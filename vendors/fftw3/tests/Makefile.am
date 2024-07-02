AM_CPPFLAGS = -I $(top_srcdir)
noinst_PROGRAMS = bench
EXTRA_DIST = check.pl README

if THREADS
bench_CFLAGS = $(PTHREAD_CFLAGS)
if !COMBINED_THREADS
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_threads.la
endif
else
if OPENMP
bench_CFLAGS = $(OPENMP_CFLAGS)
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_omp.la
endif
endif

bench_SOURCES = bench.c hook.c fftw-bench.c fftw-bench.h
bench_LDADD = $(LIBFFTWTHREADS)				\
$(top_builddir)/libfftw3@PREC_SUFFIX@.la		\
$(top_builddir)/libbench2/libbench2.a $(THREADLIBS)

check-local: bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) -r -c=30 -v `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW transforms passed basic tests!"
	@echo "--------------------------------------------------------------"
if SMP
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) -r -c=30 -v --nthreads=2 `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) -r -c=5 -v --threads_callback --nthreads=2 `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW threaded transforms passed basic tests!"
	@echo "--------------------------------------------------------------"
endif

bigcheck: bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) --validate-wisdom -a -v `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW transforms passed big tests!"
	@echo "--------------------------------------------------------------"
if SMP
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) --validate-wisdom -a -v --nthreads=2 `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) --validate-wisdom -a -v --nthreads=3 `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) --validate-wisdom -a -v --nthreads=10 `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl $(CHECK_PL_OPTS) --validate-wisdom -a -v --threads_callback --nthreads=2 `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW threaded transforms passed big tests!"
	@echo "--------------------------------------------------------------"
endif

smallcheck: bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -r -c=1 -v `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -r --estimate -c=5 -v `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW transforms passed a few tests!"
	@echo "--------------------------------------------------------------"
if SMP
	perl -w $(srcdir)/check.pl -r --estimate -c=2 -v --nthreads=2 `pwd`/bench$(EXEEXT)
	@echo "--------------------------------------------------------------"
	@echo "         FFTW threaded transforms passed a few tests!"
	@echo "--------------------------------------------------------------"
endif

paranoid-check: bench$(EXEEXT)
if SMP
	perl -w $(srcdir)/check.pl -a --patient --nthreads=10 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --patient --nthreads=7 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --patient --nthreads=3 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --patient --nthreads=2 --paranoid `pwd`/bench$(EXEEXT)
endif
	perl -w $(srcdir)/check.pl -a --patient --paranoid `pwd`/bench$(EXEEXT)

exhaustive-check: bench$(EXEEXT)
if SMP
	perl -w $(srcdir)/check.pl -a --exhaustive --nthreads=10 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --exhaustive --nthreads=7 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --exhaustive --nthreads=3 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --exhaustive --nthreads=2 --paranoid `pwd`/bench$(EXEEXT)
	perl -w $(srcdir)/check.pl -a --exhaustive --threads_callback --nthreads=2 --paranoid `pwd`/bench$(EXEEXT)
endif
	perl -w $(srcdir)/check.pl -a --exhaustive --paranoid `pwd`/bench$(EXEEXT)
