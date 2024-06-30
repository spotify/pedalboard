/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "api/api.h"
#include "dft/dft.h"
#include "rdft/rdft.h"

#include "api/x77.h"

/* if F77_FUNC is not defined, then we don't know how to mangle identifiers
   for the Fortran linker, and we must omit the f77 API. */
#if defined(F77_FUNC) || defined(WINDOWS_F77_MANGLING)

/*-----------------------------------------------------------------------*/
/* some internal functions used by the f77 api */

/* in fortran, the natural array ordering is column-major, which
   corresponds to reversing the dimensions relative to C's row-major */
static int *reverse_n(int rnk, const int *n)
{
     int *nrev;
     int i;
     A(FINITE_RNK(rnk));
     nrev = (int *) MALLOC(sizeof(int) * (unsigned)rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i)
          nrev[rnk - i - 1] = n[i];
     return nrev;
}

/* f77 doesn't have data structures, so we have to pass iodims as
   parallel arrays */
static X(iodim) *make_dims(int rnk, const int *n,
			   const int *is, const int *os)
{
     X(iodim) *dims;
     int i;
     A(FINITE_RNK(rnk));
     dims = (X(iodim) *) MALLOC(sizeof(X(iodim)) * (unsigned)rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i) {
          dims[i].n = n[i];
          dims[i].is = is[i];
          dims[i].os = os[i];
     }
     return dims;
}

typedef struct {
     void (*f77_write_char)(char *, void *);
     void *data;
} write_char_data;

static void write_char(char c, void *d)
{
     write_char_data *ad = (write_char_data *) d;
     ad->f77_write_char(&c, ad->data);
}

typedef struct {
     void (*f77_read_char)(int *, void *);
     void *data;
} read_char_data;

static int read_char(void *d)
{
     read_char_data *ed = (read_char_data *) d;
     int c;
     ed->f77_read_char(&c, ed->data);
     return (c < 0 ? EOF : c);
}

static X(r2r_kind) *ints2kinds(int rnk, const int *ik)
{
     if (!FINITE_RNK(rnk) || rnk == 0)
	  return 0;
     else {
	  int i;
	  X(r2r_kind) *k;

	  k = (X(r2r_kind) *) MALLOC(sizeof(X(r2r_kind)) * (unsigned)rnk, PROBLEMS);
	  /* reverse order for Fortran -> C */
	  for (i = 0; i < rnk; ++i)
	       k[i] = (X(r2r_kind)) ik[rnk - 1 - i];
	  return k;
     }
}

/*-----------------------------------------------------------------------*/

#define F77(a, A) F77x(x77(a), X77(A))

#ifndef WINDOWS_F77_MANGLING

#if defined(F77_FUNC)
#  define F77x(a, A) F77_FUNC(a, A)
#  include "f77funcs.h"
#endif

/* If identifiers with underscores are mangled differently than those
   without underscores, then we include *both* mangling versions.  The
   reason is that the only Fortran compiler that does such differing
   mangling is currently g77 (which adds an extra underscore to names
   with underscores), whereas other compilers running on the same
   machine are likely to use non-underscored mangling.  (I'm sick
   of users complaining that FFTW works with g77 but not with e.g.
   pgf77 or ifc on the same machine.)  Note that all FFTW identifiers
   contain underscores, and configure picks g77 by default. */
#if defined(F77_FUNC_) && !defined(F77_FUNC_EQUIV)
#  undef F77x
#  define F77x(a, A) F77_FUNC_(a, A)
#  include "f77funcs.h"
#endif

#else /* WINDOWS_F77_MANGLING */

/* Various mangling conventions common (?) under Windows. */

/* g77 */
#  define WINDOWS_F77_FUNC(a, A) a ## __
#  define F77x(a, A) WINDOWS_F77_FUNC(a, A)
#  include "f77funcs.h"

/* Intel, etc. */
#  undef WINDOWS_F77_FUNC
#  define WINDOWS_F77_FUNC(a, A) a ## _
#  include "f77funcs.h"

/* Digital/Compaq/HP Visual Fortran, Intel Fortran.  stdcall attribute
   is apparently required to adjust for calling conventions (callee
   pops stack in stdcall).  See also:
       http://msdn.microsoft.com/library/en-us/vccore98/html/_core_mixed.2d.language_programming.3a_.overview.asp
*/
#  undef WINDOWS_F77_FUNC
#  if defined(__GNUC__)
#    define WINDOWS_F77_FUNC(a, A) __attribute__((stdcall)) A
#  elif defined(_MSC_VER) || defined(_ICC) || defined(_STDCALL_SUPPORTED)
#    define WINDOWS_F77_FUNC(a, A) __stdcall A
#  else
#    define WINDOWS_F77_FUNC(a, A) A /* oh well */
#  endif
#  include "f77funcs.h"

#endif /* WINDOWS_F77_MANGLING */

#endif				/* F77_FUNC */
