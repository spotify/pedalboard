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


/* trigonometric functions */
#include "kernel/ifftw.h"
#include <math.h>

#if defined(TRIGREAL_IS_LONG_DOUBLE)
#  define COS cosl
#  define SIN sinl
#  define KTRIG(x) (x##L)
#  if defined(HAVE_DECL_SINL) && !HAVE_DECL_SINL
     extern long double sinl(long double x);
#  endif
#  if defined(HAVE_DECL_COSL) && !HAVE_DECL_COSL
     extern long double cosl(long double x);
#  endif
#elif defined(TRIGREAL_IS_QUAD)
#  define COS cosq
#  define SIN sinq
#  define KTRIG(x) (x##Q)
   extern __float128 sinq(__float128 x);
   extern __float128 cosq(__float128 x);
#else
#  define COS cos
#  define SIN sin
#  define KTRIG(x) (x)
#endif

static const trigreal K2PI =
    KTRIG(6.2831853071795864769252867665590057683943388);
#define by2pi(m, n) ((K2PI * (m)) / (n))

/*
 * Improve accuracy by reducing x to range [0..1/8]
 * before multiplication by 2 * PI.
 */

static void real_cexp(INT m, INT n, trigreal *out)
{
     trigreal theta, c, s, t;
     unsigned octant = 0;
     INT quarter_n = n;

     n += n; n += n;
     m += m; m += m;

     if (m < 0) m += n;
     if (m > n - m) { m = n - m; octant |= 4; }
     if (m - quarter_n > 0) { m = m - quarter_n; octant |= 2; }
     if (m > quarter_n - m) { m = quarter_n - m; octant |= 1; }

     theta = by2pi(m, n);
     c = COS(theta); s = SIN(theta);

     if (octant & 1) { t = c; c = s; s = t; }
     if (octant & 2) { t = c; c = -s; s = t; }
     if (octant & 4) { s = -s; }

     out[0] = c; 
     out[1] = s; 
}

static INT choose_twshft(INT n)
{
     INT log2r = 0;
     while (n > 0) {
	  ++log2r;
	  n /= 4;
     }
     return log2r;
}

static void cexpl_sqrtn_table(triggen *p, INT m, trigreal *res)
{
     m += p->n * (m < 0);

     {
	  INT m0 = m & p->twmsk;
	  INT m1 = m >> p->twshft;
	  trigreal wr0 = p->W0[2 * m0];
	  trigreal wi0 = p->W0[2 * m0 + 1];
	  trigreal wr1 = p->W1[2 * m1];
	  trigreal wi1 = p->W1[2 * m1 + 1];

	  res[0] = wr1 * wr0 - wi1 * wi0;
	  res[1] = wi1 * wr0 + wr1 * wi0;
     }
}

/* multiply (xr, xi) by exp(FFT_SIGN * 2*pi*i*m/n) */
static void rotate_sqrtn_table(triggen *p, INT m, R xr, R xi, R *res)
{
     m += p->n * (m < 0);

     {
	  INT m0 = m & p->twmsk;
	  INT m1 = m >> p->twshft;
	  trigreal wr0 = p->W0[2 * m0];
	  trigreal wi0 = p->W0[2 * m0 + 1];
	  trigreal wr1 = p->W1[2 * m1];
	  trigreal wi1 = p->W1[2 * m1 + 1];
	  trigreal wr = wr1 * wr0 - wi1 * wi0;
	  trigreal wi = wi1 * wr0 + wr1 * wi0;

#if FFT_SIGN == -1
	  res[0] = xr * wr + xi * wi;
	  res[1] = xi * wr - xr * wi;
#else
	  res[0] = xr * wr - xi * wi;
	  res[1] = xi * wr + xr * wi;
#endif
     }
}

static void cexpl_sincos(triggen *p, INT m, trigreal *res)
{
     real_cexp(m, p->n, res);
}

static void cexp_zero(triggen *p, INT m, R *res)
{
     UNUSED(p); UNUSED(m);
     res[0] = 0;
     res[1] = 0;
}

static void cexpl_zero(triggen *p, INT m, trigreal *res)
{
     UNUSED(p); UNUSED(m);
     res[0] = 0;
     res[1] = 0;
}

static void cexp_generic(triggen *p, INT m, R *res)
{
     trigreal resl[2];
     p->cexpl(p, m, resl);
     res[0] = (R)resl[0];
     res[1] = (R)resl[1];
}

static void rotate_generic(triggen *p, INT m, R xr, R xi, R *res)
{
     trigreal w[2];
     p->cexpl(p, m, w);
     res[0] = xr * w[0] - xi * (FFT_SIGN * w[1]);
     res[1] = xi * w[0] + xr * (FFT_SIGN * w[1]);
}

triggen *X(mktriggen)(enum wakefulness wakefulness, INT n)
{
     INT i, n0, n1;
     triggen *p = (triggen *)MALLOC(sizeof(*p), TWIDDLES);

     p->n = n;
     p->W0 = p->W1 = 0;
     p->cexp = 0;
     p->rotate = 0;

     switch (wakefulness) {
	 case SLEEPY:
	      A(0 /* can't happen */);
	      break;

	 case AWAKE_SQRTN_TABLE: {
	      INT twshft = choose_twshft(n);

	      p->twshft = twshft;
	      p->twradix = ((INT)1) << twshft;
	      p->twmsk = p->twradix - 1;

	      n0 = p->twradix;
	      n1 = (n + n0 - 1) / n0;

	      p->W0 = (trigreal *)MALLOC(n0 * 2 * sizeof(trigreal), TWIDDLES);
	      p->W1 = (trigreal *)MALLOC(n1 * 2 * sizeof(trigreal), TWIDDLES);

	      for (i = 0; i < n0; ++i) 
		   real_cexp(i, n, p->W0 + 2 * i);

	      for (i = 0; i < n1; ++i) 
		   real_cexp(i * p->twradix, n, p->W1 + 2 * i);

	      p->cexpl = cexpl_sqrtn_table;
	      p->rotate = rotate_sqrtn_table;
	      break;
	 }

	 case AWAKE_SINCOS: 
	      p->cexpl = cexpl_sincos;
	      break;

	 case AWAKE_ZERO: 
	      p->cexp = cexp_zero;
	      p->cexpl = cexpl_zero;
	      break;
     }

     if (!p->cexp) {
	  if (sizeof(trigreal) == sizeof(R))
	       p->cexp = (void (*)(triggen *, INT, R *))p->cexpl;
	  else
	       p->cexp = cexp_generic;
     }
     if (!p->rotate)     
	       p->rotate = rotate_generic;
     return p;
}

void X(triggen_destroy)(triggen *p)
{
     X(ifree0)(p->W0);
     X(ifree0)(p->W1);
     X(ifree)(p);
}
