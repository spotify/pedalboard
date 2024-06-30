#! /bin/sh

# Script to generate Fortran 2003 wrappers for FFTW's MPI functions.  This
# is necessary because MPI provides no way to deal with C MPI_Comm handles
# from Fortran (where MPI_Comm == integer), but does provide a way to
# deal with Fortran MPI_Comm handles from C (via MPI_Comm_f2c).  So,
# every FFTW function that takes an MPI_Comm argument needs a wrapper
# function that takes a Fortran integer and converts it to MPI_Comm.

echo "/* Generated automatically.  DO NOT EDIT! */"
echo

echo "#include \"fftw3-mpi.h\""
echo "#include \"ifftw-mpi.h\""
echo

# Declare prototypes using FFTW_EXTERN, important for Windows DLLs
grep -v 'mpi.h' fftw3-mpi.h | gcc -E -I../api - |grep "fftw_mpi_init" |tr ';' '\n' | grep "MPI_Comm" | perl genf03-wrap.pl | grep "MPI_Fint" | sed 's/^/FFTW_EXTERN /;s/$/;/'

grep -v 'mpi.h' fftw3-mpi.h | gcc -E -I../api - |grep "fftw_mpi_init" |tr ';' '\n' | grep "MPI_Comm" | perl genf03-wrap.pl


