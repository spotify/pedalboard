This directory contains a benchmarking and testing program
for fftw3.

The `bench' program has a zillion options, because we use it for
benchmarking other FFT libraries as well.  This file only documents
the basic usage of bench.

Usage: bench <commands>

where each command is as follows:

-s <problem>
--speed <problem>

    Benchmarks the speed of <problem>.

    The syntax for problems is [i|o][r|c][f|b]<size>, where

      i/o means in-place or out-of-place.  Out of place is the default.
      r/c means real or complex transform.  Complex is the default.
      f/b means forward or backward transform.  Forward is the default.
      <size> is an arbitrary multidimensional sequence of integers
        separated by the character 'x'.

    (The syntax for problems is actually richer, but we do not document
    it here.  See the man page for fftw-wisdom for more information.)

    Example:

        ib256 : in-place backward complex transform of size 256
        32x64 : out-of-place forward complex 2D transform of 32 rows
                and 64 columns.

-y <problem>
--verify <problem>

   Verify that FFTW is computing the correct answer.

   The program does not output anything unless an error occurs or
   verbosity is at least one.

-v<n>

   Set verbosity to <n>, or 1 if <n> is omitted.  -v2 will output
   the created plans with fftw_print_plan.
   
-oestimate
-opatient
-oexhaustive
 
  Plan with FFTW_ESTIMATE, FFTW_PATIENT, or FFTW_EXHAUSTIVE, respectively.
  The default is FFTW_MEASURE.

  If you benchmark FFTW, please use -opatient.
      
-onthreads=N

  Use N threads, if FFTW was compiled with --enable-threads.  N
  must be a positive integer; the default is N=1.

-onosimd

  Disable SIMD instructions (e.g. SSE or SSE2).

-ounaligned

  Plan with the FFTW_UNALIGNED flag.

-owisdom

  On startup, read wisdom from a file wis.dat in the current directory
  (if it exists).  On completion, write accumulated wisdom to wis.dat
  (overwriting any existing file of that name).
