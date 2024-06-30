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
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#define BSZ 64

static void myputs(printer *p, const char *s)
{
     char c;
     while ((c = *s++))
          p->putchr(p, c);
}

static void newline(printer *p)
{
     int i;

     p->putchr(p, '\n');
     for (i = 0; i < p->indent; ++i)
	  p->putchr(p, ' ');
}

static const char *digits = "0123456789abcdef";

static void putint(printer *p, INT i)
{
     char buf[BSZ];
     char *f = buf;

     if (i < 0) {
	  p->putchr(p, '-');
	  i = -i;
     }
     
     do {
	  *f++ = digits[i % 10];
	  i /= 10;
     } while (i);
     
     do {
	  p->putchr(p, *--f);
     } while (f != buf);
}

static void putulong(printer *p, unsigned long i, unsigned base, int width)
{
     char buf[BSZ];
     char *f = buf;

     do {
	  *f++ = digits[i % base];
	  i /= base;
     } while (i);

     while (width > f - buf) {
	  p->putchr(p, '0');
	  --width;
     }

     do {
	  p->putchr(p, *--f);
     } while (f != buf);
}

static void vprint(printer *p, const char *format, va_list ap)
{
     const char *s = format;
     char c;
     INT ival;

     while ((c = *s++)) {
          switch (c) {
	      case '%':
		   switch ((c = *s++)) {
		       case 'M': {
			    /* md5 value */
			    md5uint x = va_arg(ap, md5uint);
			    putulong(p, (unsigned long)(0xffffffffUL & x),
				     16u, 8);
			    break;
		       }
		       case 'c': {
			    int x = va_arg(ap, int);
			    p->putchr(p, (char)x);
			    break;
		       }
		       case 's': {
			    char *x = va_arg(ap, char *);
			    if (x)
				 myputs(p, x);
			    else
				 goto putnull;
			    break;
		       }
		       case 'd': {
			    int x = va_arg(ap, int);
			    ival = (INT)x;
			    goto putival;
		       }
		       case 'D': {
			    ival = va_arg(ap, INT);
			    goto putival;
		       }
		       case 'v': {
			    /* print optional vector length */
			    ival = va_arg(ap, INT);
			    if (ival > 1) {
				 myputs(p, "-x");
				 goto putival;
			    }
			    break;
		       }
		       case 'o': {
			    /* integer option.  Usage: %oNAME= */
			    ival = va_arg(ap, INT);
			    if (ival)
				 p->putchr(p, '/');
			    while ((c = *s++) != '=')
				 if (ival)
				      p->putchr(p, c);
			    if (ival) {
				 p->putchr(p, '=');
				 goto putival;
			    }
			    break;
		       }
		       case 'u': {
			    unsigned x = va_arg(ap, unsigned);
			    putulong(p, (unsigned long)x, 10u, 0);
			    break;
		       }
		       case 'x': {
			    unsigned x = va_arg(ap, unsigned);
			    putulong(p, (unsigned long)x, 16u, 0);
			    break;
		       }
		       case '(': {
			    /* newline, augment indent level */
			    p->indent += p->indent_incr;
			    newline(p);
			    break;
		       }
		       case ')': {
			    /* decrement indent level */
			    p->indent -= p->indent_incr;
			    break;
		       }
		       case 'p': {  /* note difference from C's %p */
			    /* print plan */
			    plan *x = va_arg(ap, plan *);
			    if (x) 
				 x->adt->print(x, p);
			    else 
				 goto putnull;
			    break;
		       }
		       case 'P': {
			    /* print problem */
			    problem *x = va_arg(ap, problem *);
			    if (x)
				 x->adt->print(x, p);
			    else
				 goto putnull;
			    break;
		       }
		       case 'T': {
			    /* print tensor */
			    tensor *x = va_arg(ap, tensor *);
			    if (x)
				 X(tensor_print)(x, p);
			    else
				 goto putnull;
			    break;
		       }
		       default:
			    A(0 /* unknown format */);
			    break;

		   putnull:
			    myputs(p, "(null)");
			    break;

		   putival:
			    putint(p, ival);
			    break;
		   }
		   break;
	      default:
		   p->putchr(p, c);
		   break;
          }
     }
}

static void print(printer *p, const char *format, ...)
{
     va_list ap;
     va_start(ap, format);
     vprint(p, format, ap);
     va_end(ap);
}

printer *X(mkprinter)(size_t size, 
		      void (*putchr)(printer *p, char c),
		      void (*cleanup)(printer *p))
{
     printer *s = (printer *)MALLOC(size, OTHER);
     s->print = print;
     s->vprint = vprint;
     s->putchr = putchr;
     s->cleanup = cleanup;
     s->indent = 0;
     s->indent_incr = 2;
     return s;
}

void X(printer_destroy)(printer *p)
{
     if (p->cleanup)
	  p->cleanup(p);
     X(ifree)(p);
}
