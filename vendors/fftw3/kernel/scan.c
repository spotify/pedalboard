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


#include "kernel/ifftw.h"
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef USE_CTYPE
#include <ctype.h>
#else
/* Screw ctype. On linux, the is* functions call a routine that gets
   the ctype map in the current locale.  Because this operation is
   expensive, the map is cached on a per-thread basis.  I am not
   willing to link this crap with FFTW.  Not over my dead body.

   Sic transit gloria mundi.
*/
#undef isspace
#define isspace(x) ((x) >= 0 && (x) <= ' ')
#undef isdigit
#define isdigit(x) ((x) >= '0' && (x) <= '9')
#undef isupper
#define isupper(x) ((x) >= 'A' && (x) <= 'Z')
#undef islower
#define islower(x) ((x) >= 'a' && (x) <= 'z')
#endif

static int mygetc(scanner *sc)
{
     if (sc->ungotc != EOF) {
	  int c = sc->ungotc;
	  sc->ungotc = EOF;
	  return c;
     }
     return(sc->getchr(sc));
}

#define GETCHR(sc) mygetc(sc)

static void myungetc(scanner *sc, int c)
{
     sc->ungotc = c;
}

#define UNGETCHR(sc, c) myungetc(sc, c)

static void eat_blanks(scanner *sc)
{
     int ch;
     while (ch = GETCHR(sc), isspace(ch))
          ;
     UNGETCHR(sc, ch);
}

static void mygets(scanner *sc, char *s, int maxlen)
{
     char *s0 = s;
     int ch;

     A(maxlen > 0);
     while ((ch = GETCHR(sc)) != EOF && !isspace(ch)
	    && ch != ')' && ch != '(' && s < s0 + maxlen)
	  *s++ = (char)(ch & 0xFF);
     *s = 0;
     UNGETCHR(sc, ch);
}

static long getlong(scanner *sc, int base, int *ret)
{
     int sign = 1, ch, count;
     long x = 0;     

     ch = GETCHR(sc);
     if (ch == '-' || ch == '+') {
	  sign = ch == '-' ? -1 : 1;
	  ch = GETCHR(sc);
     }
     for (count = 0; ; ++count) {
	  if (isdigit(ch)) 
	       ch -= '0';
	  else if (isupper(ch))
	       ch -= 'A' - 10;
	  else if (islower(ch))
	       ch -= 'a' - 10;
	  else
	       break;
	  x = x * base + ch;
	  ch = GETCHR(sc);
     }
     x *= sign;
     UNGETCHR(sc, ch);
     *ret = count > 0;
     return x;
}

/* vscan is mostly scanf-like, with our additional format specifiers,
   but with a few twists.  It returns simply 0 or 1 indicating whether
   the match was successful. '(' and ')' in the format string match
   those characters preceded by any whitespace.  Finally, if a
   character match fails, it will ungetchr() the last character back
   onto the stream. */
static int vscan(scanner *sc, const char *format, va_list ap)
{
     const char *s = format;
     char c;
     int ch = 0;
     int fmt_len;

     while ((c = *s++)) {
	  fmt_len = 0;
          switch (c) {
	      case '%':
	  getformat:
		   switch ((c = *s++)) {
		       case 's': {
			    char *x = va_arg(ap, char *);
			    mygets(sc, x, fmt_len);
			    break;
		       }
		       case 'd': {
			    int *x = va_arg(ap, int *);
			    *x = (int) getlong(sc, 10, &ch);
			    if (!ch) return 0;
			    break;
		       }
		       case 'x': {
			    int *x = va_arg(ap, int *);
			    *x = (int) getlong(sc, 16, &ch);
			    if (!ch) return 0;
			    break;
		       }
		       case 'M': {
			    md5uint *x = va_arg(ap, md5uint *);
			    *x = (md5uint)
				    (0xFFFFFFFF & getlong(sc, 16, &ch));
			    if (!ch) return 0;
			    break;
		       }
		       case '*': {
			    if ((fmt_len = va_arg(ap, int)) <= 0) return 0;
			    goto getformat;
		       }
		       default:
			    A(0 /* unknown format */);
			    break;
		   }
		   break;
	      default:
		   if (isspace(c) || c == '(' || c == ')')
			eat_blanks(sc);
		   if (!isspace(c) && (ch = GETCHR(sc)) != c) {
			UNGETCHR(sc, ch);
			return 0;
		   }
		   break;
          }
     }
     return 1;
}

static int scan(scanner *sc, const char *format, ...)
{
     int ret;
     va_list ap;
     va_start(ap, format);
     ret = vscan(sc, format, ap);
     va_end(ap);
     return ret;
}

scanner *X(mkscanner)(size_t size, int (*getchr)(scanner *sc))
{
     scanner *s = (scanner *)MALLOC(size, OTHER);
     s->scan = scan;
     s->vscan = vscan;
     s->getchr = getchr;
     s->ungotc = EOF;
     return s;
}

void X(scanner_destroy)(scanner *sc)
{
     X(ifree)(sc);
}
