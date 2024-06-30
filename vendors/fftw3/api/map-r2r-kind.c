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
#include "rdft/rdft.h"

rdft_kind *X(map_r2r_kind)(int rank, const X(r2r_kind) * kind)
{
     int i;
     rdft_kind *k;

     A(FINITE_RNK(rank));
     k = (rdft_kind *) MALLOC((unsigned)rank * sizeof(rdft_kind), PROBLEMS);
     for (i = 0; i < rank; ++i) {
	  rdft_kind m;
          switch (kind[i]) {
	      case FFTW_R2HC: m = R2HC; break;
	      case FFTW_HC2R: m = HC2R; break;
	      case FFTW_DHT: m = DHT; break;
	      case FFTW_REDFT00: m = REDFT00; break;
	      case FFTW_REDFT01: m = REDFT01; break;
	      case FFTW_REDFT10: m = REDFT10; break;
	      case FFTW_REDFT11: m = REDFT11; break;
	      case FFTW_RODFT00: m = RODFT00; break;
	      case FFTW_RODFT01: m = RODFT01; break;
	      case FFTW_RODFT10: m = RODFT10; break;
	      case FFTW_RODFT11: m = RODFT11; break;
	      default: m = R2HC; A(0);
          }
	  k[i] = m;
     }
     return k;
}
