

dnl Process this file with autoconf to produce a configure script.

dnl Define the fftw version number as M4 macros, so that we can enforce
dnl the invariant that the minor version number in FFTW-X.Y.MINOR is the same
dnl as the revision number in SHARED_VERSION_INFO.
define(FFTW_MAJOR_VERSION, 3.3)dnl
define(FFTW_MINOR_VERSION, 10)dnl

dnl Version number of the FFTW source package.
AC_INIT(fftw, FFTW_MAJOR_VERSION.FFTW_MINOR_VERSION, fftw@fftw.org)
AC_CONFIG_SRCDIR(kernel/ifftw.h)

dnl Version number for libtool shared libraries.  Libtool wants a string
dnl of the form CURRENT:REVISION:AGE.  We adopt the convention that
dnl REVISION is the same as the FFTW minor version number.
dnl fftw-3.1.x was 4:x:1
dnl fftw-3.2.x was 5:x:2
dnl fftw-3.3.x was 6:x:3 for x < 4
dnl fftw-3.3.4 was 7:x:4
dnl fftw-3.3.5 was 8:x:5 (added planner hooks)
dnl fftw-3.3.6 was 8:x:6 (8:x:6 is a bug, should have been 8:x:5.  No API changes)
dnl fftw-3.3.6.1 fixes the 8:x:6 screwup
dnl fftw-3.3.7 was 8:x:5 (No API changes)
dnl fftw-3.3.8 was 8:x:5 (No API changes)
dnl fftw-3.3.9 was 9:x:6 (added threads callback)
dnl fftw-3.3.10 was 9:x:6 (No API changes)
SHARED_VERSION_INFO="9:FFTW_MINOR_VERSION:6" # CURRENT:REVISION:AGE

AM_INIT_AUTOMAKE(1.7)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
AC_SUBST(SHARED_VERSION_INFO)
AC_DISABLE_SHARED dnl to hell with shared libraries
AC_CANONICAL_HOST

dnl configure options
case "${host_cpu}" in
  powerpc*) arch_prefers_fma=yes;;
  ia64*) arch_prefers_fma=yes;;
  hppa*) arch_prefers_fma=yes;;
  mips64*) arch_prefers_fma=yes;;
  *) arch_prefers_fma=no;;
esac

AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],[compile fftw with extra runtime checks for debugging])], ok=$enableval, ok=no)
if test "$ok" = "yes"; then
    AC_DEFINE(FFTW_DEBUG,1,[Define to enable extra FFTW debugging code.])
fi

AC_ARG_ENABLE(doc, [AC_HELP_STRING([--disable-doc],[disable building the documentation])], build_doc=$enableval, build_doc=yes)
AM_CONDITIONAL(BUILD_DOC, test x"$build_doc" = xyes)

AC_ARG_ENABLE(random-estimator, [AC_HELP_STRING([--enable-random-estimator],[enable pseudorandom estimator (debugging hack)])], ok=$enableval, ok=no)
if test "$ok" = "yes"; then
    AC_DEFINE(FFTW_RANDOM_ESTIMATOR,1,[Define to enable pseudorandom estimate planning for debugging.])
    CHECK_PL_OPTS="--estimate"
fi

AC_ARG_ENABLE(alloca, [AC_HELP_STRING([--disable-alloca],[disable use of the alloca() function (may be broken on mingw64)])], ok=$enableval, ok=yes)
if test "$ok" = "yes"; then
    AC_DEFINE(FFTW_ENABLE_ALLOCA,1,[Define to enable the use of alloca().])
fi

AC_ARG_ENABLE(single, [AC_HELP_STRING([--enable-single],[compile fftw in single precision])], ok=$enableval, ok=no)
AC_ARG_ENABLE(float, [AC_HELP_STRING([--enable-float],[synonym for --enable-single])], ok=$enableval)
if test "$ok" = "yes"; then
    AC_DEFINE(FFTW_SINGLE,1,[Define to compile in single precision.])
    AC_DEFINE(BENCHFFT_SINGLE,1,[Define to compile in single precision.])
    PRECISION=s
else
    PRECISION=d
fi
AM_CONDITIONAL(SINGLE, test "$ok" = "yes")

AC_ARG_ENABLE(long-double, [AC_HELP_STRING([--enable-long-double],[compile fftw in long-double precision])], ok=$enableval, ok=no)
if test "$ok" = "yes"; then
    if test "$PRECISION" = "s"; then
        AC_MSG_ERROR([--enable-single/--enable-long-double conflict])
    fi
    AC_DEFINE(FFTW_LDOUBLE,1,[Define to compile in long-double precision.])
    AC_DEFINE(BENCHFFT_LDOUBLE,1,[Define to compile in long-double precision.])
    PRECISION=l
fi
AM_CONDITIONAL(LDOUBLE, test "$ok" = "yes")

AC_ARG_ENABLE(quad-precision, [AC_HELP_STRING([--enable-quad-precision],[compile fftw in quadruple precision if available])], ok=$enableval, ok=no)
if test "$ok" = "yes"; then
    if test "$PRECISION" != "d"; then
        AC_MSG_ERROR([conflicting precisions specified])
    fi
    AC_DEFINE(FFTW_QUAD,1,[Define to compile in quad precision.])
    AC_DEFINE(BENCHFFT_QUAD,1,[Define to compile in quad precision.])
    PRECISION=q
fi
AM_CONDITIONAL(QUAD, test "$ok" = "yes")

AC_SUBST(PRECISION)
AC_SUBST(CHECK_PL_OPTS)

dnl SSE/SSE2 theory:
dnl
dnl Historically, you had to supply --enable-sse in single precision and --enable-sse2
dnl in double precision.
dnl
dnl This behavior is pointless in 2016.  --enable-sse2 now works in both precisions,
dnl and is interpreted as --enable-sse in single precision.  The old flag --enable--se
dnl is still supported in single-precision only.
AC_ARG_ENABLE(sse, [AC_HELP_STRING([--enable-sse],[enable SSE optimizations])], have_sse=$enableval, have_sse=no)
if test "$have_sse" = "yes"; then
    if test "$PRECISION" != "s"; then
            AC_MSG_ERROR([SSE requires single precision])
    fi
fi

AC_ARG_ENABLE(sse2, [AC_HELP_STRING([--enable-sse2],[enable SSE/SSE2 optimizations])], have_sse2=$enableval, have_sse2=no)
if test "$have_sse" = "yes"; then have_sse2=yes; fi
if test "$have_sse2" = "yes"; then
    AC_DEFINE(HAVE_SSE2,1,[Define to enable SSE/SSE2 optimizations.])
    if test "$PRECISION" != "d" -a "$PRECISION" != "s"; then
            AC_MSG_ERROR([SSE2 requires single or double precision])
    fi
fi
AM_CONDITIONAL(HAVE_SSE2, test "$have_sse2" = "yes")

AC_ARG_ENABLE(avx, [AC_HELP_STRING([--enable-avx],[enable AVX optimizations])], have_avx=$enableval, have_avx=no)
if test "$have_avx" = "yes"; then
    AC_DEFINE(HAVE_AVX,1,[Define to enable AVX optimizations.])
    if test "$PRECISION" != "d" -a "$PRECISION" != "s"; then
        AC_MSG_ERROR([AVX requires single or double precision])
    fi
fi
AM_CONDITIONAL(HAVE_AVX, test "$have_avx" = "yes")

AC_ARG_ENABLE(avx2, [AC_HELP_STRING([--enable-avx2],[enable AVX2 optimizations])], have_avx2=$enableval, have_avx2=no)
if test "$have_avx2" = "yes"; then
    AC_DEFINE(HAVE_AVX2,1,[Define to enable AVX2 optimizations.])
    if test "$PRECISION" != "d" -a "$PRECISION" != "s"; then
        AC_MSG_ERROR([AVX2 requires single or double precision])
    fi
fi
AM_CONDITIONAL(HAVE_AVX2, test "$have_avx2" = "yes")

AC_ARG_ENABLE(avx512, [AC_HELP_STRING([--enable-avx512],[enable AVX512 optimizations])], have_avx512=$enableval, have_avx512=no)
if test "$have_avx512" = "yes"; then
    AC_DEFINE(HAVE_AVX512,1,[Define to enable AVX512 optimizations.])
    if test "$PRECISION" != "d" -a "$PRECISION" != "s"; then
        AC_MSG_ERROR([AVX512 requires single or double precision])
    fi
fi
AM_CONDITIONAL(HAVE_AVX512, test "$have_avx512" = "yes")

dnl 128-bit AVX is special. There is no reason to use it on Intel processors
dnl since SSE2 is just as fast. However, on AMD processors we can both use
dnl FMA4, and 128-bit SIMD is better than 256-bit since core pairs in a
dnl compute unit can execute two 128-bit instructions independently.
AC_ARG_ENABLE(avx-128-fma, [AC_HELP_STRING([--enable-avx-128-fma],[enable AVX128/FMA optimizations])], have_avx_128_fma=$enableval, have_avx_128_fma=no)
if test "$have_avx_128_fma" = "yes"; then
    AC_DEFINE(HAVE_AVX_128_FMA,1,[Define to enable 128-bit FMA AVX optimization])
    AVX_128_FMA_CFLAGS="${AVX_CFLAGS} -mfma4"
    AC_SUBST(AVX_128_FMA_CFLAGS)
fi
AM_CONDITIONAL(HAVE_AVX_128_FMA, test "$have_avx_128_fma" = "yes")

AC_ARG_ENABLE(kcvi, [AC_HELP_STRING([--enable-kcvi],[enable Knights Corner vector instructions optimizations])], have_kcvi=$enableval, have_kcvi=no)
if test "$have_kcvi" = "yes"; then
    AC_DEFINE(HAVE_KCVI,1,[Define to enable KCVI optimizations.])
    if test "$PRECISION" != "d" -a "$PRECISION" != "s"; then
        AC_MSG_ERROR([Knights Corner vector instructions requires single or double precision])
    fi
fi
AM_CONDITIONAL(HAVE_KCVI, test "$have_kcvi" = "yes")

AC_ARG_ENABLE(altivec, [AC_HELP_STRING([--enable-altivec],[enable Altivec optimizations])], have_altivec=$enableval, have_altivec=no)
if test "$have_altivec" = "yes"; then
    AC_DEFINE(HAVE_ALTIVEC,1,[Define to enable Altivec optimizations.])
    if test "$PRECISION" != "s"; then
        AC_MSG_ERROR([Altivec requires single precision])
    fi
fi
AM_CONDITIONAL(HAVE_ALTIVEC, test "$have_altivec" = "yes")

AC_ARG_ENABLE(vsx, [AC_HELP_STRING([--enable-vsx],[enable IBM VSX optimizations])], have_vsx=$enableval, have_vsx=no)
if test "$have_vsx" = "yes"; then
        AC_DEFINE(HAVE_VSX,1,[Define to enable IBM VSX optimizations.])
fi
AM_CONDITIONAL(HAVE_VSX, test "$have_vsx" = "yes")

AC_ARG_ENABLE(neon, [AC_HELP_STRING([--enable-neon],[enable ARM NEON optimizations])], have_neon=$enableval, have_neon=no)
if test "$have_neon" = "yes"; then
    AC_DEFINE(HAVE_NEON,1,[Define to enable ARM NEON optimizations.])
        case "${host_cpu}" in
                aarch64)
                ;;
                *)
        if test "$PRECISION" != "s"; then
            AC_MSG_ERROR([NEON requires single precision])
        fi
                ;;
        esac
fi
AM_CONDITIONAL(HAVE_NEON, test "$have_neon" = "yes")

AC_ARG_ENABLE(armv8-pmccntr-el0, [AC_HELP_STRING([--enable-armv8-pmccntr-el0],[enable the cycle counter on ARMv8 via the PMCCNTR_EL0 register (see README-perfcounters for details and mandatory instructions)])], have_armv8pmccntrel0=$enableval)
if test "$have_armv8pmccntrel0"x = "yes"x; then
    AC_DEFINE(HAVE_ARMV8_PMCCNTR_EL0,1,[Define if you have enabled the PMCCNTR_EL0 cycle counter on ARMv8])
fi

AC_ARG_ENABLE(armv8-cntvct-el0, [AC_HELP_STRING([--enable-armv8-cntvct-el0],[enable the cycle counter on ARMv8 via the CNTVCT_EL0 register (see README-perfcounters for details and mandatory instructions)])], have_armv8cntvctel0=$enableval)
if test "$have_armv8cntvctel0"x = "yes"x; then
    AC_DEFINE(HAVE_ARMV8_CNTVCT_EL0,1,[Define if you have enabled the CNTVCT_EL0 cycle counter on ARMv8])
fi

AC_ARG_ENABLE(armv7a-cntvct, [AC_HELP_STRING([--enable-armv7a-cntvct],[enable the cycle counter on Armv7a via the CNTVCT register (see README-perfcounters for details and mandatory instructions)])], have_armv7acntvct=$enableval)
if test "$have_armv7acntvct"x = "yes"x; then
    AC_DEFINE(HAVE_ARMV7A_CNTVCT,1,[Define if you have enabled the CNTVCT cycle counter on ARMv7a])
fi

AC_ARG_ENABLE(armv7a-pmccntr, [AC_HELP_STRING([--enable-armv7a-pmccntr],[enable the cycle counter on Armv7a via the PMCCNTR register (see README-perfcounters for details and mandatory instructions)])], have_armv7apmccntr=$enableval)
if test "$have_armv7apmccntr"x = "yes"x; then
    AC_DEFINE(HAVE_ARMV7A_PMCCNTR,1,[Define if you have enabled the PMCCNTR cycle counter on ARMv7a])
fi

AC_ARG_ENABLE(generic-simd128, [AC_HELP_STRING([--enable-generic-simd128],[enable generic (gcc) 128-bit SIMD optimizations])], have_generic_simd128=$enableval, have_generic_simd128=no)
if test "$have_generic_simd128" = "yes"; then
        AC_DEFINE(HAVE_GENERIC_SIMD128,1,[Define to enable generic (gcc) 128-bit SIMD optimizations.])
fi
AM_CONDITIONAL(HAVE_GENERIC_SIMD128, test "$have_generic_simd128" = "yes")

AC_ARG_ENABLE(generic-simd256, [AC_HELP_STRING([--enable-generic-simd256],[enable generic (gcc) 256-bit SIMD optimizations])], have_generic_simd256=$enableval, have_generic_simd256=no)
if test "$have_generic_simd256" = "yes"; then
        AC_DEFINE(HAVE_GENERIC_SIMD256,1,[Define to enable generic (gcc) 256-bit SIMD optimizations.])
fi
AM_CONDITIONAL(HAVE_GENERIC_SIMD256, test "$have_generic_simd256" = "yes")


dnl FIXME:
dnl AC_ARG_ENABLE(mips-ps, [AC_HELP_STRING([--enable-mips-ps],[enable MIPS pair-single optimizations])], have_mips_ps=$enableval, have_mips_ps=no)
dnl if test "$have_mips_ps" = "yes"; then
dnl 	AC_DEFINE(HAVE_MIPS_PS,1,[Define to enable MIPS paired-single optimizations.])
dnl 	if test "$PRECISION" != "s"; then
dnl 		AC_MSG_ERROR([MIPS paired-single requires single precision])
dnl 	fi
dnl fi
dnl AM_CONDITIONAL(HAVE_MIPS_PS, test "$have_mips_ps" = "yes")

AC_ARG_WITH(slow-timer, [AC_HELP_STRING([--with-slow-timer],[use low-precision timers (SLOW)])], with_slow_timer=$withval, with_slow_timer=no)
if test "$with_slow_timer" = "yes"; then
    AC_DEFINE(WITH_SLOW_TIMER,1,[Use low-precision timers, making planner very slow])
fi

AC_ARG_ENABLE(mips_zbus_timer, [AC_HELP_STRING([--enable-mips-zbus-timer],[use MIPS ZBus cycle-counter])], have_mips_zbus_timer=$enableval, have_mips_zbus_timer=no)
if test "$have_mips_zbus_timer" = "yes"; then
    AC_DEFINE(HAVE_MIPS_ZBUS_TIMER,1,[Define to enable use of MIPS ZBus cycle-counter.])
fi

AC_ARG_WITH(our-malloc, [AC_HELP_STRING([--with-our-malloc],[use our aligned malloc (helpful for Win32)])], with_our_malloc=$withval, with_our_malloc=no)
AC_ARG_WITH(our-malloc16, [AC_HELP_STRING([--with-our-malloc16],[Obsolete alias for --with-our-malloc16])], with_our_malloc=$withval)
if test "$with_our_malloc" = "yes"; then
    AC_DEFINE(WITH_OUR_MALLOC,1,[Use our own aligned malloc routine; mainly helpful for Windows systems lacking aligned allocation system-library routines.])
fi

AC_ARG_WITH(windows-f77-mangling, [AC_HELP_STRING([--with-windows-f77-mangling],[use common Win32 Fortran interface styles])], with_windows_f77_mangling=$withval, with_windows_f77_mangling=no)
if test "$with_windows_f77_mangling" = "yes"; then
    AC_DEFINE(WINDOWS_F77_MANGLING,1,[Use common Windows Fortran mangling styles for the Fortran interfaces.])
fi

AC_ARG_WITH(incoming-stack-boundary, [AC_HELP_STRING([--with-incoming-stack-boundary=X],[Assume that stack is aligned to (1<<X) bytes])], with_incoming_stack_boundary=$withval, with_incoming_stack_boundary=no)


AC_ARG_ENABLE(fma, [AC_HELP_STRING([--enable-fma],[enable if the machine architecture "naturally" prefers fused multiply-add instructions])], arch_prefers_fma=$enableval)
if test "$arch_prefers_fma"x = "yes"x; then
        AC_DEFINE(ARCH_PREFERS_FMA,1,[Define if the machine architecture "naturally" prefers fused multiply-add instructions])
fi

dnl compute library suffix
case "$PRECISION" in
     s) PREC_SUFFIX=f;;
     d) PREC_SUFFIX=;;
     l) PREC_SUFFIX=l;;
     q) PREC_SUFFIX=q;;
esac
AC_SUBST(PREC_SUFFIX)

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AX_COMPILER_VENDOR
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_RANLIB

AC_CHECK_PROG(OCAMLBUILD, ocamlbuild, ocamlbuild)

dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(mpi, [AC_HELP_STRING([--enable-mpi],[compile FFTW MPI library])], enable_mpi=$enableval, enable_mpi=no)

if test "$enable_mpi" = "yes"; then
   if test $PRECISION = q; then
      AC_MSG_ERROR([quad precision is not supported in MPI])
   fi
   ACX_MPI([],[AC_MSG_ERROR([could not find mpi library for --enable-mpi])])
   AC_CHECK_PROG(MPIRUN, mpirun, mpirun)
   AC_SUBST(MPIRUN)

   save_CC=$CC
   CC=$MPICC
   AC_CHECK_SIZEOF(MPI_Fint, [], [#include <mpi.h>])
   CC=$save_CC
   if test 0 = $ac_cv_sizeof_MPI_Fint; then
      AC_MSG_WARN([sizeof(MPI_Fint) test failed]);
      dnl As a backup, assume Fortran integer == C int
      AC_CHECK_SIZEOF(int)
      if test 0 = $ac_cv_sizeof_int; then AC_MSG_ERROR([sizeof(int) test failed]); fi
      ac_cv_sizeof_MPI_Fint=$ac_cv_sizeof_int
   fi
   C_MPI_FINT=C_INT`expr $ac_cv_sizeof_MPI_Fint \* 8`_T
   AC_SUBST(C_MPI_FINT)
fi
AM_CONDITIONAL(MPI, test "$enable_mpi" = "yes")

dnl -----------------------------------------------------------------------

dnl determine CFLAGS first
AX_CC_MAXOPT

case "${ax_cv_c_compiler_vendor}" in
   hp) # must (sometimes) manually increase cpp limits to handle fftw3.h
        AX_CHECK_COMPILER_FLAGS([-Wp,-H128000],
                        [CC="$CC -Wp,-H128000"])
        ;;

   portland) # -Masmkeyword required for asm("") cycle counters
        AX_CHECK_COMPILER_FLAGS([-Masmkeyword],
                                [CC="$CC -Masmkeyword"])
        ;;
esac

dnl Determine SIMD CFLAGS at least for gcc and icc
case "${ax_cv_c_compiler_vendor}" in
    gnu|intel)
        # SSE/SSE2
        if test "$have_sse2" = "yes" -a "x$SSE2_CFLAGS" = x; then
            if test "$PRECISION" = d; then flag=msse2; else flag=msse; fi
            AX_CHECK_COMPILER_FLAGS(-$flag, [SSE2_CFLAGS="-$flag"],
            [AC_MSG_ERROR([Need a version of gcc with -$flag])])
        fi

        # AVX
        if test "$have_avx" = "yes" -a "x$AVX_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mavx, [AVX_CFLAGS="-mavx"],
            [AC_MSG_ERROR([Need a version of gcc with -mavx])])
        fi

        # AVX2
        if test "$have_avx2" = "yes" -a "x$AVX2_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mavx2, [AVX2_CFLAGS="-mavx2"],
                [AC_MSG_ERROR([Need a version of gcc with -mavx2])])
            AX_CHECK_COMPILER_FLAGS(-mfma, [AVX2_CFLAGS="$AVX2_CFLAGS -mfma"],
                [AC_MSG_WARN([Need a version of gcc with -mfma (harmless for icc)])])
        fi

        # AVX512
        if test "$have_avx512" = "yes" -a "x$AVX512_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mavx512f, [AVX512_CFLAGS="-mavx512f"],
            [AC_MSG_ERROR([Need a version of gcc with -mavx512f])])
        fi

        if test "$host_vendor" = "apple"; then
            # We need to tell gcc to use an external assembler to get AVX/AVX2 with gcc on OS X
            AX_CHECK_COMPILER_FLAGS([-Wa,-q], [CFLAGS="$CFLAGS -Wa,-q"])
            # Disable the new compact unwinding format so we avoid warnings/potential errors.
            AX_CHECK_COMPILER_FLAGS([-Wl,-no_compact_unwind], [CFLAGS="$CFLAGS -Wl,-no_compact_unwind"])
        fi

        # KCVI
        if test "$have_kcvi" = "yes" -a "x$KCVI_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mmic, [KCVI_CFLAGS="-mmic"],
            [AC_MSG_ERROR([Need a version of icc with -mmic])])
        fi

        if test "$have_altivec" = "yes" -a "x$ALTIVEC_CFLAGS" = x; then
            # -DFAKE__VEC__ is a workaround because gcc-3.3 does not
            # #define __VEC__ with -maltivec.
            AX_CHECK_COMPILER_FLAGS(-faltivec, [ALTIVEC_CFLAGS="-faltivec"],
            [AX_CHECK_COMPILER_FLAGS(-maltivec -mabi=altivec,
                [ALTIVEC_CFLAGS="-maltivec -mabi=altivec -DFAKE__VEC__"],
                [AX_CHECK_COMPILER_FLAGS(-fvec, [ALTIVEC_CFLAGS="-fvec"],
                [AC_MSG_ERROR([Need a version of gcc with -maltivec])])])])
        fi

        case "${host_cpu}" in
            aarch64)
                ;;
            *)
                if test "$have_neon" = "yes" -a "x$NEON_CFLAGS" = x; then
                    AX_CHECK_COMPILER_FLAGS(-mfpu=neon, [NEON_CFLAGS="-mfpu=neon"],
                [AC_MSG_ERROR([Need a version of gcc with -mfpu=neon])])
                fi
                ;;
        esac

        if test "$have_vsx" = "yes" -a "x$VSX_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mvsx, [VSX_CFLAGS="-mvsx"],
                [AC_MSG_ERROR([Need a version of gcc with -mvsx])])
        fi

    dnl FIXME:
    dnl elif test "$have_mips_ps" = "yes"; then
    dnl     # Just punt here and use only new 4.2 compiler :(
    dnl 	# Should add section for older compilers...
    dnl 	AX_CHECK_COMPILER_FLAGS(-mpaired-single,
    dnl 	    [SIMD_CFLAGS="-mpaired-single"],
    dnl 	    #[AC_MSG_ERROR([Need a version of gcc with -mpaired-single])])
    dnl 	    [AX_CHECK_COMPILER_FLAGS(-march=mips64,
    dnl 	      [SIMD_CFLAGS="-march=mips64"],
    dnl 	        [AC_MSG_ERROR(
    dnl 		 [Need a version of gcc with -mpaired-single or -march=mips64])
    dnl 		])])
    dnl fi
    ;;

    clang)

        if test "$have_avx" = "yes" -a "x$AVX_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mavx, [AVX_CFLAGS="-mavx"],
                [AC_MSG_ERROR([Need a version of clang with -mavx])])
        fi

        if test "$have_avx2" = "yes" -a "x$AVX2_CFLAGS" = x; then
                AX_CHECK_COMPILER_FLAGS(-mavx2, [AVX2_CFLAGS="-mavx2"],
                    [AC_MSG_ERROR([Need a version of clang with -mavx2])])
                AX_CHECK_COMPILER_FLAGS(-mfma, [AVX2_CFLAGS="$AVX2_CFLAGS -mfma"])
        fi

        # AVX512
        if test "$have_avx512" = "yes" -a "x$AVX512_CFLAGS" = x; then
            AX_CHECK_COMPILER_FLAGS(-mavx512f, [AVX512_CFLAGS="-mavx512f"],
            [AC_MSG_ERROR([Need a version of clang with -mavx512f])])
        fi

        if test "$have_vsx" = "yes" -a "x$VSX_CFLAGS" = x; then
            # clang appears to need both -mvsx and -maltivec for VSX
            AX_CHECK_COMPILER_FLAGS(-maltivec, [VSX_CFLAGS="-maltivec"],
                [AC_MSG_ERROR([Need a version of gcc with -maltivec])])
            AX_CHECK_COMPILER_FLAGS(-mvsx, [VSX_CFLAGS="-mvsx $VSX_CFLAGS"],
                [AC_MSG_ERROR([Need a version of gcc with -mvsx])])
        fi
        ;;

    ibm)
        if test "$have_vsx" = "yes" -a "x$VSX_CFLAGS" = x; then
            # Note that IBM xlC uses -qaltivec for VSX too.
            AX_CHECK_COMPILER_FLAGS(-qaltivec, [VSX_CFLAGS="-qaltivec"],
                [AC_MSG_ERROR([Need a version of gcc with -qaltivec])])
        fi
        ;;
esac

AC_SUBST(SSE2_CFLAGS)
AC_SUBST(AVX_CFLAGS)
AC_SUBST(AVX2_CFLAGS)
AC_SUBST(AVX512_CFLAGS)
AC_SUBST(KCVI_CFLAGS)
AC_SUBST(ALTIVEC_CFLAGS)
AC_SUBST(VSX_CFLAGS)
AC_SUBST(NEON_CFLAGS)

dnl add stack alignment CFLAGS if so requested
if test "$with_incoming_stack_boundary"x != "no"x; then
   case "${ax_cv_c_compiler_vendor}" in
      gnu)
        tentative_flags="-mincoming-stack-boundary=$with_incoming_stack_boundary";
        AX_CHECK_COMPILER_FLAGS($tentative_flags,
              [STACK_ALIGN_CFLAGS=$tentative_flags])
      ;;
   esac
fi
AC_SUBST(STACK_ALIGN_CFLAGS)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h fenv.h limits.h malloc.h stddef.h sys/time.h])
dnl c_asm.h: Header file for enabling asm() on Digital Unix
dnl intrinsics.h: cray unicos
dnl sys/sysctl.h: MacOS X altivec detection

dnl altivec.h requires $ALTIVEC_CFLAGS (we use this for VSX too, which uses the same header)
save_CFLAGS="$CFLAGS"
save_CPPFLAGS="$CPPFLAGS"
CFLAGS="$CFLAGS $ALTIVEC_CFLAGS $VSX_CFLAGS"
CPPFLAGS="$CPPFLAGS $ALTIVEC_CFLAGS $VSX_CFLAGS"
AC_CHECK_HEADERS([altivec.h])
CFLAGS="$save_CFLAGS"
CPPFLAGS="$save_CPPFLAGS"

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_HEADER_TIME
AC_CHECK_TYPE([long double],
              [AC_DEFINE(HAVE_LONG_DOUBLE, 1, [Define to 1 if the compiler supports `long double'])],
[
if test $PRECISION = l; then
    AC_MSG_ERROR([long double is not a supported type with your compiler.])
fi
])
AC_CHECK_TYPE([hrtime_t],[AC_DEFINE(HAVE_HRTIME_T, 1, [Define to 1 if hrtime_t is defined in <sys/time.h>])],,
[
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
])

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(ptrdiff_t)

AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPES(uintptr_t, [], [AC_CHECK_SIZEOF(void *)], [$ac_includes_default
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif])

AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

dnl Check sizeof fftw_r2r_kind for Fortran interface [it has == sizeof(int)
dnl for years, but being paranoid].  Note: the definition here must match
dnl the one in api/fftw3.h!
AC_CHECK_SIZEOF(fftw_r2r_kind, [], [typedef enum {
     FFTW_R2HC=0, FFTW_HC2R=1, FFTW_DHT=2,
     FFTW_REDFT00=3, FFTW_REDFT01=4, FFTW_REDFT10=5, FFTW_REDFT11=6,
     FFTW_RODFT00=7, FFTW_RODFT01=8, FFTW_RODFT10=9, FFTW_RODFT11=10
} fftw_r2r_kind;])
if test 0 = $ac_cv_sizeof_fftw_r2r_kind; then AC_MSG_ERROR([sizeof(fftw_r2r_kind) test failed]); fi
C_FFTW_R2R_KIND=C_INT`expr $ac_cv_sizeof_fftw_r2r_kind \* 8`_T
AC_SUBST(C_FFTW_R2R_KIND)

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_LIB(m, sin)

if test $PRECISION = q; then
   AX_GCC_VERSION(4,6,0,[],[AC_MSG_ERROR([gcc 4.6 or later required for quad precision support])])
   AC_CHECK_LIB(quadmath, sinq, [], [AC_MSG_ERROR([quad precision requires libquadmath for quad-precision trigonometric routines])])
   LIBQUADMATH=-lquadmath
fi
AC_SUBST(LIBQUADMATH)

AC_CHECK_FUNCS([BSDgettimeofday gettimeofday gethrtime read_real_time time_base_to_time drand48 sqrt memset posix_memalign memalign _mm_malloc _mm_free clock_gettime mach_absolute_time sysctl abort sinl cosl snprintf memmove strchr getpagesize])
AC_CHECK_DECLS([sinl, cosl, sinq, cosq],,,[#include <math.h>])
AC_CHECK_DECLS([memalign],,,[
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif])
AC_CHECK_DECLS([drand48, srand48, posix_memalign]) dnl in stdlib.h

dnl Cray UNICOS _rtc() (real-time clock) intrinsic
AC_MSG_CHECKING([for _rtc intrinsic])
rtc_ok=yes
AC_TRY_LINK([#ifdef HAVE_INTRINSICS_H
#include <intrinsics.h>
#endif], [_rtc()], [AC_DEFINE(HAVE__RTC,1,[Define if you have the UNICOS _rtc() intrinsic.])], [rtc_ok=no])
AC_MSG_RESULT($rtc_ok)

if test "$PRECISION" = "l"; then
    AC_CHECK_FUNCS([cosl sinl tanl], [], [AC_MSG_ERROR([long-double precision requires long-double trigonometric routines])])
fi

AC_MSG_CHECKING([for isnan])
AC_TRY_LINK([#include <math.h>
], if (!isnan(3.14159)) isnan(2.7183);, ok=yes, ok=no)
if test "$ok" = "yes"; then
    AC_DEFINE(HAVE_ISNAN,1,[Define if the isnan() function/macro is available.])
fi
AC_MSG_RESULT(${ok})

dnl TODO
AX_GCC_ALIGNS_STACK()

dnl override CFLAGS selection when debugging
if test "${enable_debug}" = "yes"; then
    CFLAGS="-g"
fi

dnl add gcc warnings, in debug/maintainer mode only
if test "$enable_debug" = yes || test "$USE_MAINTAINER_MODE" = yes; then
if test "$ac_test_CFLAGS" != "set"; then
    if test $ac_cv_prog_gcc = yes; then
        CFLAGS="$CFLAGS -Wall -W -Wcast-qual -Wpointer-arith -Wcast-align -pedantic -Wno-long-long -Wshadow -Wbad-function-cast -Wwrite-strings -Wstrict-prototypes -Wredundant-decls -Wnested-externs" # -Wundef -Wconversion -Wmissing-prototypes -Wmissing-declarations
    fi
fi
fi

dnl check for a proper indent in maintainer mode
if test "$USE_MAINTAINER_MODE" = yes; then
    AC_PATH_PROG(INDENT, indent, indent)
    # if INDENT is set to 'indent' then we didn't find indent
    if test "$INDENT" != indent ; then
        AC_MSG_CHECKING(if $INDENT is GNU indent)
        if $INDENT --version 2>/dev/null | head -n 1|grep "GNU indent" > /dev/null ; then
                AC_MSG_RESULT(yes)
                INDENT="$INDENT -kr -cs -i5 -l800 -fca -nfc1 -sc -sob -cli4 -TR -Tplanner -TV"
        else
                AC_MSG_RESULT(no)
                AC_MSG_WARN($INDENT does not appear to be GNU indent.)
        fi
    else
        AC_MSG_WARN(no indent program found: codelets will be ugly)
        INDENT=cat
    fi
fi

dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(fortran, [AC_HELP_STRING([--disable-fortran],[don't include Fortran-callable wrappers])], enable_fortran=$enableval, enable_fortran=yes)

if test "$enable_fortran" = "yes"; then
    AC_PROG_F77
    if test -z "$F77"; then
        enable_fortran=no
        AC_MSG_WARN([*** Couldn't find f77 compiler; using default Fortran wrappers.])
    else
        AC_F77_DUMMY_MAIN([], [enable_fortran=no
        AC_MSG_WARN([*** Couldn't figure out how to link C and Fortran; using default Fortran wrappers.])])
    fi
else
    AC_DEFINE([DISABLE_FORTRAN], 1, [Define to disable Fortran wrappers.])
fi

if test "x$enable_fortran" = xyes; then
    AC_F77_WRAPPERS
    AC_F77_FUNC(f77foo)
    AC_F77_FUNC(f77_foo)
    f77_foo2=`echo $f77foo | sed 's/77/77_/'`
    if test "$f77_foo" = "$f77_foo2"; then
        AC_DEFINE(F77_FUNC_EQUIV, 1, [Define if F77_FUNC and F77_FUNC_ are equivalent.])

        # Include g77 wrappers by default for GNU systems or gfortran
        with_g77_wrappers=$ac_cv_f77_compiler_gnu
        case $host_os in *gnu*) with_g77_wrappers=yes ;; esac
    fi
else
    with_g77_wrappers=no
fi

AC_ARG_WITH(g77-wrappers, [AC_HELP_STRING([--with-g77-wrappers],[force inclusion of g77-compatible wrappers in addition to any other Fortran compiler that is detected])], with_g77_wrappers=$withval)
if test "x$with_g77_wrappers" = "xyes"; then
    AC_DEFINE(WITH_G77_WRAPPERS,1,[Include g77-compatible wrappers in addition to any other Fortran wrappers.])
fi

dnl -----------------------------------------------------------------------
have_smp="no"
AC_ARG_ENABLE(openmp, [AC_HELP_STRING([--enable-openmp],[use OpenMP directives for parallelism])], enable_openmp=$enableval, enable_openmp=no)

if test "$enable_openmp" = "yes"; then
   AC_DEFINE(HAVE_OPENMP,1,[Define to enable OpenMP])
   AX_OPENMP([], [AC_MSG_ERROR([don't know how to enable OpenMP])])
fi

AC_ARG_ENABLE(threads, [AC_HELP_STRING([--enable-threads],[compile FFTW SMP threads library])], enable_threads=$enableval, enable_threads=no)

if test "$enable_threads" = "yes"; then
   AC_DEFINE(HAVE_THREADS,1,[Define to enable SMP threads])
fi

AC_ARG_WITH(combined-threads, [AC_HELP_STRING([--with-combined-threads],[combine threads into main libfftw3])], with_combined_threads=$withval, with_combined_threads=no)

if test "$with_combined_threads" = yes; then
    if test "$enable_openmp" = "yes"; then
        AC_MSG_ERROR([--with-combined-threads incompatible with --enable-openmp])
    fi
    if test "$enable_threads" != "yes"; then
        AC_MSG_ERROR([--with-combined-threads requires --enable-threads])
    fi
fi

dnl Check for threads library...
THREADLIBS=""
if test "$enable_threads" = "yes"; then
        # Win32 threads are the default on Windows:
    if test -z "$THREADLIBS"; then
        AC_MSG_CHECKING([for Win32 threads])
        AC_TRY_LINK([#include <windows.h>],
            [_beginthreadex(0,0,0,0,0,0);],
            [THREADLIBS=" "; AC_MSG_RESULT(yes)],
            [AC_MSG_RESULT(no)])
    fi

    # POSIX threads, the default choice everywhere else:
    if test -z "$THREADLIBS"; then
        ACX_PTHREAD([THREADLIBS="$PTHREAD_LIBS "
                         CC="$PTHREAD_CC"
                         AC_DEFINE(USING_POSIX_THREADS, 1, [Define if we have and are using POSIX threads.])])
    fi

    if test -z "$THREADLIBS"; then
        AC_MSG_ERROR([couldn't find threads library for --enable-threads])
    fi
    AC_DEFINE(HAVE_THREADS, 1, [Define if we have a threads library.])
fi
AC_SUBST(THREADLIBS)
AM_CONDITIONAL(THREADS, test "$enable_threads" = "yes")
AM_CONDITIONAL(OPENMP, test "$enable_openmp" = "yes")
AM_CONDITIONAL(SMP, test "$enable_threads" = "yes" -o "$enable_openmp" = "yes")
AM_CONDITIONAL(COMBINED_THREADS, test x"$with_combined_threads" = xyes)

dnl -----------------------------------------------------------------------

AC_MSG_CHECKING([whether a cycle counter is available])
save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I$srcdir/kernel"
AC_TRY_CPP([#include "cycle.h"
#ifndef HAVE_TICK_COUNTER
#  error No cycle counter
#endif], [ok=yes], [ok=no])
CPPFLAGS=$save_CPPFLAGS
AC_MSG_RESULT($ok)
if test $ok = no && test "x$with_slow_timer" = xno; then
    echo "***************************************************************"
    echo "WARNING: No cycle counter found.  FFTW will use ESTIMATE mode  "
    echo "         for all plans.  See the manual for more information."
    echo "***************************************************************"
fi

dnl -----------------------------------------------------------------------

AC_DEFINE_UNQUOTED(FFTW_CC, "$CC $CFLAGS", [C compiler name and flags])

AC_CONFIG_FILES([
   Makefile
   support/Makefile
   genfft/Makefile
   kernel/Makefile
   simd-support/Makefile

   dft/Makefile
   dft/scalar/Makefile
   dft/scalar/codelets/Makefile
   dft/simd/Makefile
   dft/simd/common/Makefile
   dft/simd/sse2/Makefile
   dft/simd/avx/Makefile
   dft/simd/avx-128-fma/Makefile
   dft/simd/avx2/Makefile
   dft/simd/avx2-128/Makefile
   dft/simd/avx512/Makefile
   dft/simd/kcvi/Makefile
   dft/simd/altivec/Makefile
   dft/simd/vsx/Makefile
   dft/simd/neon/Makefile
   dft/simd/generic-simd128/Makefile
   dft/simd/generic-simd256/Makefile

   rdft/Makefile
   rdft/scalar/Makefile
   rdft/scalar/r2cf/Makefile
   rdft/scalar/r2cb/Makefile
   rdft/scalar/r2r/Makefile
   rdft/simd/Makefile
   rdft/simd/common/Makefile
   rdft/simd/sse2/Makefile
   rdft/simd/avx/Makefile
   rdft/simd/avx-128-fma/Makefile
   rdft/simd/avx2/Makefile
   rdft/simd/avx2-128/Makefile
   rdft/simd/avx512/Makefile
   rdft/simd/kcvi/Makefile
   rdft/simd/altivec/Makefile
   rdft/simd/vsx/Makefile
   rdft/simd/neon/Makefile
   rdft/simd/generic-simd128/Makefile
   rdft/simd/generic-simd256/Makefile

   reodft/Makefile

   threads/Makefile

   api/Makefile

   mpi/Makefile

   libbench2/Makefile
   tests/Makefile
   doc/Makefile
   doc/FAQ/Makefile

   tools/Makefile
   tools/fftw_wisdom.1
   tools/fftw-wisdom-to-conf

   m4/Makefile

   fftw.pc
])

AC_OUTPUT
