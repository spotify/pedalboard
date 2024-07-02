# -I $(top_srcdir)/api is necessary because fftw3-mpi.h includes
# "fftw3.h", and we cannot change the latter to "api/fftw3.h" because
# fftw3-mpi.h is installed in /usr/include.
AM_CPPFLAGS = -I $(top_srcdir) -I $(top_srcdir)/api

if MPI
lib_LTLIBRARIES = libfftw3@PREC_SUFFIX@_mpi.la
include_HEADERS = fftw3-mpi.h
nodist_include_HEADERS = fftw3-mpi.f03 fftw3l-mpi.f03
noinst_PROGRAMS = mpi-bench
endif

CC=@MPICC@

EXTRA_DIST = testsched.c f03api.sh f03-wrap.sh genf03-wrap.pl fftw3-mpi.f03.in fftw3l-mpi.f03.in
BUILT_SOURCES = fftw3-mpi.f03.in fftw3-mpi.f03 fftw3l-mpi.f03.in fftw3l-mpi.f03 f03-wrap.c
CLEANFILES = fftw3-mpi.f03 fftw3l-mpi.f03

TRANSPOSE_SRC = transpose-alltoall.c transpose-pairwise.c transpose-recurse.c transpose-problem.c transpose-solve.c mpi-transpose.h
DFT_SRC = dft-serial.c dft-rank-geq2.c dft-rank-geq2-transposed.c dft-rank1.c dft-rank1-bigvec.c dft-problem.c dft-solve.c mpi-dft.h
RDFT_SRC = rdft-serial.c rdft-rank-geq2.c rdft-rank-geq2-transposed.c rdft-rank1-bigvec.c rdft-problem.c rdft-solve.c mpi-rdft.h
RDFT2_SRC = rdft2-serial.c rdft2-rank-geq2.c rdft2-rank-geq2-transposed.c rdft2-problem.c rdft2-solve.c mpi-rdft2.h
SRC = any-true.c api.c block.c choose-radix.c conf.c dtensor.c fftw3-mpi.h ifftw-mpi.h rearrange.c wisdom-api.c f03-wrap.c

libfftw3@PREC_SUFFIX@_mpi_la_SOURCES = $(SRC) $(TRANSPOSE_SRC) $(DFT_SRC) $(RDFT_SRC) $(RDFT2_SRC)

libfftw3@PREC_SUFFIX@_mpi_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@
libfftw3@PREC_SUFFIX@_mpi_la_LIBADD = ../libfftw3@PREC_SUFFIX@.la @MPILIBS@

if THREADS
mpi_bench_CFLAGS = $(PTHREAD_CFLAGS)
if !COMBINED_THREADS
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_threads.la
endif
else
if OPENMP
mpi_bench_CFLAGS = $(OPENMP_CFLAGS)
LIBFFTWTHREADS = $(top_builddir)/threads/libfftw3@PREC_SUFFIX@_omp.la
endif
endif

mpi_bench_SOURCES = mpi-bench.c $(top_srcdir)/tests/fftw-bench.c $(top_srcdir)/tests/hook.c
mpi_bench_LDADD = libfftw3@PREC_SUFFIX@_mpi.la $(LIBFFTWTHREADS) $(top_builddir)/libfftw3@PREC_SUFFIX@.la $(top_builddir)/libbench2/libbench2.a $(MPILIBS) $(THREADLIBS)

CHECK = $(top_srcdir)/tests/check.pl
NUMCHECK=10
CHECKSIZE=10000
CHECKOPTS = --verbose --random --maxsize=$(CHECKSIZE) -c=$(NUMCHECK) $(CHECK_PL_OPTS)

if MPI

check-local: mpi-bench$(EXEEXT)
	perl -w $(CHECK) $(CHECKOPTS) --mpi "$(MPIRUN) -np 1 `pwd`/mpi-bench"
	@echo "--------------------------------------------------------------"
	@echo "     MPI FFTW transforms passed "$(NUMCHECK)" tests, 1 CPU"
	@echo "--------------------------------------------------------------"
	perl -w $(CHECK) $(CHECKOPTS) --mpi "$(MPIRUN) -np 2 `pwd`/mpi-bench"
	@echo "--------------------------------------------------------------"
	@echo "      MPI FFTW transforms passed "$(NUMCHECK)" tests, 2 CPUs"
	@echo "--------------------------------------------------------------"
	perl -w $(CHECK) $(CHECKOPTS) --mpi "$(MPIRUN) -np 3 `pwd`/mpi-bench"
	@echo "--------------------------------------------------------------"
	@echo "      MPI FFTW transforms passed "$(NUMCHECK)" tests, 3 CPUs"
	@echo "--------------------------------------------------------------"
	perl -w $(CHECK) $(CHECKOPTS) --mpi "$(MPIRUN) -np 4 `pwd`/mpi-bench"
	@echo "--------------------------------------------------------------"
	@echo "      MPI FFTW transforms passed "$(NUMCHECK)" tests, 4 CPUs"
	@echo "--------------------------------------------------------------"
if SMP
	perl -w $(CHECK) $(CHECKOPTS) --mpi --nthreads=2 "$(MPIRUN) -np 3 `pwd`/mpi-bench"
	@echo "--------------------------------------------------------------"
	@echo "      MPI FFTW threaded transforms passed "$(NUMCHECK)" tests!"
	@echo "--------------------------------------------------------------"
endif

bigcheck: mpi-bench$(EXEEXT)
	$(MAKE) $(AM_MAKEFLAGS) NUMCHECK=100 CHECKSIZE=60000 check-local

smallcheck: mpi-bench$(EXEEXT)
	$(MAKE) $(AM_MAKEFLAGS) NUMCHECK=2 check-local

endif

fftw3-mpi.f03: fftw3-mpi.f03.in Makefile
	sed 's/C_MPI_FINT/@C_MPI_FINT@/' $(srcdir)/fftw3-mpi.f03.in > $@

fftw3l-mpi.f03: fftw3l-mpi.f03.in Makefile
	sed 's/C_MPI_FINT/@C_MPI_FINT@/' $(srcdir)/fftw3l-mpi.f03.in > $@

if MAINTAINER_MODE

fftw3-mpi.f03.in: fftw3-mpi.h f03api.sh $(top_srcdir)/api/genf03.pl
	sh $(srcdir)/f03api.sh d f > $@

fftw3l-mpi.f03.in: fftw3-mpi.h f03api.sh $(top_srcdir)/api/genf03.pl
	sh $(srcdir)/f03api.sh l | grep -v parameter | sed 's/fftw3.f03/fftw3l.f03/' > $@

f03-wrap.c: fftw3-mpi.h f03-wrap.sh genf03-wrap.pl
	sh $(srcdir)/f03-wrap.sh > $@

endif
