dnl @synopsis AX_CC_MAXOPT
dnl @summary turn on optimization flags for the C compiler
dnl @category C
dnl
dnl Try to turn on "good" C optimization flags for various compilers
dnl and architectures, for some definition of "good".  (In our case,
dnl good for FFTW and hopefully for other scientific codes.  Modify 
dnl as needed.)
dnl
dnl The user can override the flags by setting the CFLAGS environment
dnl variable.  
dnl
dnl Note also that the flags assume that ANSI C aliasing rules are
dnl followed by the code (e.g. for gcc's -fstrict-aliasing), and that
dnl floating-point computations can be re-ordered as needed.
dnl
dnl Requires macros: AX_CHECK_COMPILER_FLAGS, AX_COMPILER_VENDOR,
dnl
dnl @version 2011-06-22
dnl @license GPLWithACException
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Matteo Frigo.
AC_DEFUN([AX_CC_MAXOPT],
[
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AX_COMPILER_VENDOR])
AC_REQUIRE([AC_CANONICAL_HOST])

# Try to determine "good" native compiler flags if none specified via CFLAGS
if test "$ac_test_CFLAGS" != "set"; then
  CFLAGS=""
  case $ax_cv_c_compiler_vendor in
    dec) CFLAGS="-newc -w0 -O5 -ansi_alias -ansi_args -fp_reorder -tune host"
    	 ;;

    sun) CFLAGS="-native -fast -xO5 -dalign"
    	 ;;

    hp)  CFLAGS="+Oall +Optrs_ansi +DSnative"
    	 ;;

    ibm) xlc_opt="-qarch=auto -qtune=auto"
         AX_CHECK_COMPILER_FLAGS($xlc_opt,
         	CFLAGS="-O3 -qalias=ansi -w $xlc_opt",
               [CFLAGS="-O3 -qalias=ansi -w"])
         ;;

    intel) CFLAGS="-O3"
        # Intel seems to have changed the spelling of this flag recently
        icc_ansi_alias="unknown"
	for flag in -ansi-alias -ansi_alias; do
	  AX_CHECK_COMPILER_FLAGS($flag, [icc_ansi_alias=$flag; break])
	done
 	if test "x$icc_ansi_alias" != xunknown; then
            CFLAGS="$CFLAGS $icc_ansi_alias"
        fi
	AX_CHECK_COMPILER_FLAGS(-malign-double, CFLAGS="$CFLAGS -malign-double")
	# We used to check for architecture flags here, e.g. -xHost etc.,
	# but these flags are problematic.  On icc-12.0.0, "-mavx -xHost"
	# overrides -mavx with -xHost, generating SSE2 code instead of AVX
	# code.  ICC does not seem to support -mtune=host or equivalent
	# non-ABI changing flag.
	;;
    
    clang)
        CFLAGS="-O3 -fomit-frame-pointer"
        AX_CHECK_COMPILER_FLAGS(-mtune=native, CFLAGS="$CFLAGS -mtune=native")
        AX_CHECK_COMPILER_FLAGS(-fstrict-aliasing,CFLAGS="$CFLAGS -fstrict-aliasing")
        ;;

    gnu) 
     # Default optimization flags for gcc on all systems.
     # Somehow -O3 does not imply -fomit-frame-pointer on ia32
     CFLAGS="-O3 -fomit-frame-pointer"

     # tune for the host by default
     AX_CHECK_COMPILER_FLAGS(-mtune=native, CFLAGS="$CFLAGS -mtune=native")

     # -malign-double for x86 systems
     AX_CHECK_COMPILER_FLAGS(-malign-double, CFLAGS="$CFLAGS -malign-double")

     #  -fstrict-aliasing for gcc-2.95+
     AX_CHECK_COMPILER_FLAGS(-fstrict-aliasing,
	CFLAGS="$CFLAGS -fstrict-aliasing")

     # -fno-schedule-insns is pretty much required on all risc
     # processors.
     # 
     # gcc performs one pass of instruction scheduling, then a pass of
     # register allocation, then another pass of instruction
     # scheduling.  The first pass reorders instructions in a way that
     # is pretty much the worst possible for the purposes of register
     # allocation.  We disable the first pass.
     AX_CHECK_COMPILER_FLAGS(-fno-schedule-insns, CFLAGS="$CFLAGS -fno-schedule-insns")

     # flags to enable power ISA 2.07 instructions with gcc (always true with vsx)
     if test "$have_vsx" = "yes"; then
         AX_CHECK_COMPILER_FLAGS(-mcpu=power8, CFLAGS="$CFLAGS -mcpu=power8")
         AX_CHECK_COMPILER_FLAGS(-mpower8-fusion, CFLAGS="$CFLAGS -mpower8-fusion")
         AX_CHECK_COMPILER_FLAGS(-mpower8-vector, CFLAGS="$CFLAGS -mpower8-vector")
         AX_CHECK_COMPILER_FLAGS(-mdirect-move, CFLAGS="$CFLAGS -mdirect-move")
     fi
     ;;
  esac

  if test -z "$CFLAGS"; then
	echo ""
	echo "********************************************************"
        echo "* WARNING: Don't know the best CFLAGS for this system  *"
        echo "* Use ./configure CFLAGS=... to specify your own flags *"
	echo "* (otherwise, a default of CFLAGS=-O3 will be used)    *"
	echo "********************************************************"
	echo ""
        CFLAGS="-O3"
  fi

  AX_CHECK_COMPILER_FLAGS($CFLAGS, [], [
	echo ""
        echo "********************************************************"
        echo "* WARNING: The guessed CFLAGS don't seem to work with  *"
        echo "* your compiler.                                       *"
        echo "* Use ./configure CFLAGS=... to specify your own flags *"
        echo "********************************************************"
        echo ""
        CFLAGS=""
  ])

fi
])
