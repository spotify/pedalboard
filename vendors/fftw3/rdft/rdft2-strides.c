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

#include "rdft/rdft.h"

/* Deal with annoyance because the tensor (is,os) applies to
   (r,rio/iio) for R2HC and vice-versa for HC2R.  We originally had
   (is,os) always apply to (r,rio/iio), but this causes other
   headaches with the tensor functions. */
void X(rdft2_strides)(rdft_kind kind, const iodim *d, INT *rs, INT *cs)
{
     if (kind == R2HC) {
	  *rs = d->is;
	  *cs = d->os;
     }
     else {
	  A(kind == HC2R);
	  *rs = d->os;
	  *cs = d->is;
     }
}
