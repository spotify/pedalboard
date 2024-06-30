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

void X(ops_zero)(opcnt *dst)
{
     dst->add = dst->mul = dst->fma = dst->other = 0;
}

void X(ops_cpy)(const opcnt *src, opcnt *dst)
{
     *dst = *src;
}

void X(ops_other)(INT o, opcnt *dst)
{
     X(ops_zero)(dst);
     dst->other = o;
}

void X(ops_madd)(INT m, const opcnt *a, const opcnt *b, opcnt *dst)
{
     dst->add = m * a->add + b->add;
     dst->mul = m * a->mul + b->mul;
     dst->fma = m * a->fma + b->fma;
     dst->other = m * a->other + b->other;
}

void X(ops_add)(const opcnt *a, const opcnt *b, opcnt *dst)
{
     X(ops_madd)(1, a, b, dst);
}

void X(ops_add2)(const opcnt *a, opcnt *dst)
{
     X(ops_add)(a, dst, dst);
}

void X(ops_madd2)(INT m, const opcnt *a, opcnt *dst)
{
     X(ops_madd)(m, a, dst, dst);
}

