#! /bin/sh

# Script to generate Fortran 2003 interface declarations for FFTW from
# the fftw3.h header file.

# This is designed so that the Fortran caller can do:
#   use, intrinsic :: iso_c_binding
#   implicit none
#   include 'fftw3.f03'
# and then call the C FFTW functions directly, with type checking.

echo "! Generated automatically.  DO NOT EDIT!"
echo

# C_FFTW_R2R_KIND is determined by configure and inserted by the Makefile
# echo "  integer, parameter :: C_FFTW_R2R_KIND = @C_FFTW_R2R_KIND@"

# Extract constants
perl -pe 's/([A-Z0-9_]+)=([+-]?[0-9]+)/\n  integer\(C_INT\), parameter :: \1 = \2\n/g' < fftw3.h | grep 'integer(C_INT)'
perl -pe 's/#define +([A-Z0-9_]+) +\(([+-]?[0-9]+)U?\)/\n  integer\(C_INT\), parameter :: \1 = \2\n/g' < fftw3.h | grep 'integer(C_INT)'
perl -pe 'if (/#define +([A-Z0-9_]+) +\(([0-9]+)U? *<< *([0-9]+)\)/) { print "\n  integer\(C_INT\), parameter :: $1 = ",$2 << $3,"\n"; }' < fftw3.h | grep 'integer(C_INT)'

# Extract function declarations
for p in $*; do
    if test "$p" = "d"; then p=""; fi

    echo
    cat <<EOF
  type, bind(C) :: fftw${p}_iodim
     integer(C_INT) n, is, os
  end type fftw${p}_iodim
  type, bind(C) :: fftw${p}_iodim64
     integer(C_INTPTR_T) n, is, os
  end type fftw${p}_iodim64
EOF

    echo
    echo "  interface"
    gcc -D__GNUC__=5 -D__i386__ -E fftw3.h |grep "fftw${p}_plan_dft" |tr ';' '\n' | grep -v "fftw${p}_execute(" | perl genf03.pl
    echo "  end interface"

done
