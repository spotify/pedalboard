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

typedef struct {
     printer super;
     size_t *cnt;
} P_cnt;

static void putchr_cnt(printer * p_, char c)
{
     P_cnt *p = (P_cnt *) p_;
     UNUSED(c);
     ++*p->cnt;
}

printer *X(mkprinter_cnt)(size_t *cnt)
{
     P_cnt *p = (P_cnt *) X(mkprinter)(sizeof(P_cnt), putchr_cnt, 0);
     p->cnt = cnt;
     *cnt = 0;
     return &p->super;
}

typedef struct {
     printer super;
     char *s;
} P_str;

static void putchr_str(printer * p_, char c)
{
     P_str *p = (P_str *) p_;
     *p->s++ = c;
     *p->s = 0;
}

printer *X(mkprinter_str)(char *s)
{
     P_str *p = (P_str *) X(mkprinter)(sizeof(P_str), putchr_str, 0);
     p->s = s;
     *s = 0;
     return &p->super;
}
