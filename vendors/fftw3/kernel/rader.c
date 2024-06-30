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

/*
  common routines for Rader solvers 
*/


/* shared twiddle and omega lists, keyed by two/three integers. */
struct rader_tls {
     INT k1, k2, k3;
     R *W;
     int refcnt;
     rader_tl *cdr; 
};

void X(rader_tl_insert)(INT k1, INT k2, INT k3, R *W, rader_tl **tl)
{
     rader_tl *t = (rader_tl *) MALLOC(sizeof(rader_tl), TWIDDLES);
     t->k1 = k1; t->k2 = k2; t->k3 = k3; t->W = W;
     t->refcnt = 1; t->cdr = *tl; *tl = t;
}

R *X(rader_tl_find)(INT k1, INT k2, INT k3, rader_tl *t)
{
     while (t && (t->k1 != k1 || t->k2 != k2 || t->k3 != k3))
	  t = t->cdr;
     if (t) {
	  ++t->refcnt;
	  return t->W;
     } else 
	  return 0;
}

void X(rader_tl_delete)(R *W, rader_tl **tl)
{
     if (W) {
	  rader_tl **tp, *t;

	  for (tp = tl; (t = *tp) && t->W != W; tp = &t->cdr)
	       ;

	  if (t && --t->refcnt <= 0) {
	       *tp = t->cdr;
	       X(ifree)(t->W);
	       X(ifree)(t);
	  }
     }
}
