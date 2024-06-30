dnl @synopsis AX_GCC_ALIGNS_STACK([ACTION-IF-YES], [ACTION-IF-NO])
dnl @summary check whether gcc can align stack to 8-byte boundary
dnl @category Misc
dnl
dnl Check to see if we are using a version of gcc that aligns the stack
dnl (true in gcc-2.95+, which have the -mpreferred-stack-boundary flag).
dnl Also, however, checks whether main() is correctly aligned by the
dnl OS/libc/..., as well as for a bug in the stack alignment of gcc-2.95.x
dnl (see http://gcc.gnu.org/ml/gcc-bugs/1999-11/msg00259.html).
dnl
dnl ACTION-IF-YES/ACTION-IF-NO are shell commands to execute if we are
dnl using gcc and the stack is/isn't aligned, respectively.
dnl
dnl Requires macro: AX_CHECK_COMPILER_FLAGS, AX_GCC_VERSION
dnl
dnl @version 2005-05-30
dnl @license GPLWithACException
dnl @author Steven G. Johnson <stevenj@alum.mit.edu>
AC_DEFUN([AX_GCC_ALIGNS_STACK],
[
AC_REQUIRE([AC_PROG_CC])
ax_gcc_aligns_stack=no
if test "$GCC" = "yes"; then
AX_CHECK_COMPILER_FLAGS(-mpreferred-stack-boundary=4, [
	AC_MSG_CHECKING([whether the stack is at least 8-byte aligned by gcc])
	save_CFLAGS="$CFLAGS"
	CFLAGS="-O"
	AX_CHECK_COMPILER_FLAGS(-malign-double, CFLAGS="$CFLAGS -malign-double")
	AC_TRY_RUN([#include <stdlib.h>
#       include <stdio.h>
	struct yuck { int blechh; };
	int one(void) { return 1; }
	struct yuck ick(void) { struct yuck y; y.blechh = 3; return y; }
#       define CHK_ALIGN(x) if ((((long) &(x)) & 0x7)) { fprintf(stderr, "bad alignment of " #x "\n"); exit(1); }
	void blah(int foo) { double foobar; CHK_ALIGN(foobar); }
	int main2(void) {double ok1; struct yuck y; double ok2; CHK_ALIGN(ok1);
                         CHK_ALIGN(ok2); y = ick(); blah(one()); return 0;}
	int main(void) { if ((((long) (__builtin_alloca(0))) & 0x7)) __builtin_alloca(4); return main2(); }
	], [ax_gcc_aligns_stack=yes; ax_gcc_stack_align_bug=no], 
	ax_gcc_stack_align_bug=yes, [AX_GCC_VERSION(3,0,0, ax_gcc_stack_align_bug=no, ax_gcc_stack_align_bug=yes)])
	CFLAGS="$save_CFLAGS"
	AC_MSG_RESULT($ax_gcc_aligns_stack)
])
fi
if test "$ax_gcc_aligns_stack" = yes; then
	m4_default([$1], :)
else
	m4_default([$2], :)
fi
])
