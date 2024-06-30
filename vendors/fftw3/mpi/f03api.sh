#! /bin/sh

# Script to generate Fortran 2003 interface declarations for FFTW's MPI
# interface from the fftw3-mpi.h header file.

# This is designed so that the Fortran caller can do:
#   use, intrinsic :: iso_c_binding
#   implicit none
#   include 'fftw3-mpi.f03'
# and then call the C FFTW MPI functions directly, with type checking.
#
# One caveat: because there is no standard way to conver MPI_Comm objects
# from Fortran (= integer) to C (= opaque type), the Fortran interface
# technically calls C wrapper functions (also auto-generated) which
# call MPI_Comm_f2c to convert the communicators as needed.

echo "! Generated automatically.  DO NOT EDIT!"
echo

echo "  include 'fftw3.f03'"
echo

# Extract constants
perl -pe 's/#define +([A-Z0-9_]+) +\(([+-]?[0-9]+)U?\)/\n  integer\(C_INTPTR_T\), parameter :: \1 = \2\n/g' < fftw3-mpi.h | grep 'integer(C_INTPTR_T)'
perl -pe 'if (/#define +([A-Z0-9_]+) +\(([0-9]+)U? *<< *([0-9]+)\)/) { print "\n  integer\(C_INT\), parameter :: $1 = ",$2 << $3,"\n"; }' < fftw3-mpi.h | grep 'integer(C_INT)'

# Extract function declarations
for p in $*; do
    if test "$p" = "d"; then p=""; fi

    echo
    cat <<EOF
  type, bind(C) :: fftw${p}_mpi_ddim
     integer(C_INTPTR_T) n, ib, ob
  end type fftw${p}_mpi_ddim
EOF

    echo
    echo "  interface"
    grep -v 'mpi.h' fftw3-mpi.h | gcc -I../api -D__GNUC__=5 -D__i386__ -E - |grep "fftw${p}_mpi_init" |tr ';' '\n' | perl ../api/genf03.pl
    echo "  end interface"

done
