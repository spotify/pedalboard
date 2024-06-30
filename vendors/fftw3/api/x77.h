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

/* Fortran-like (e.g. as in BLAS) type prefixes for F77 interface */
#if defined(FFTW_SINGLE)
#  define x77(name) CONCAT(sfftw_, name)
#  define X77(NAME) CONCAT(SFFTW_, NAME)
#elif defined(FFTW_LDOUBLE)
/* FIXME: what is best?  BLAS uses D..._X, apparently.  Ugh. */
#  define x77(name) CONCAT(lfftw_, name)
#  define X77(NAME) CONCAT(LFFTW_, NAME)
#elif defined(FFTW_QUAD)
#  define x77(name) CONCAT(qfftw_, name)
#  define X77(NAME) CONCAT(QFFTW_, NAME)
#else
#  define x77(name) CONCAT(dfftw_, name)
#  define X77(NAME) CONCAT(DFFTW_, NAME)
#endif

/* If F77_FUNC is not defined and the user didn't explicitly specify
   --disable-fortran, then make our best guess at default wrappers
   (since F77_FUNC_EQUIV should not be defined in this case, we
    will use both double-underscored g77 wrappers and single- or
    non-underscored wrappers).  This saves us from dealing with
    complaints in the cases where the user failed to specify
    an F77 compiler or wrapper detection failed for some reason. */
#if !defined(F77_FUNC) && !defined(DISABLE_FORTRAN)
#  if (defined(_WIN32) || defined(__WIN32__)) && !defined(WINDOWS_F77_MANGLING)
#    define WINDOWS_F77_MANGLING 1
#  endif
#  if defined(_AIX) || defined(__hpux) || defined(hpux)
#    define F77_FUNC(a, A) a
#  elif defined(CRAY) || defined(_CRAY) || defined(_UNICOS)
#    define F77_FUNC(a, A) A
#  else
#    define F77_FUNC(a, A) a ## _
#  endif
#  define F77_FUNC_(a, A) a ## __
#endif

#if defined(WITH_G77_WRAPPERS) && !defined(DISABLE_FORTRAN)
#  undef F77_FUNC_
#  define F77_FUNC_(a, A) a ## __
#  undef F77_FUNC_EQUIV
#endif

/* annoying Windows syntax for shared-library declarations */
#if defined(FFTW_DLL) && (defined(_WIN32) || defined(__WIN32__))
#  define FFTW_VOIDFUNC __declspec(dllexport) void
#else
#  define FFTW_VOIDFUNC void
#endif
