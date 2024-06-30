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

/***************************************************************************/

/* Rader's algorithm requires lots of modular arithmetic, and if we
   aren't careful we can have errors due to integer overflows. */

/* Compute (x * y) mod p, but watch out for integer overflows; we must
   have 0 <= {x, y} < p.

   If overflow is common, this routine is somewhat slower than
   e.g. using 'long long' arithmetic.  However, it has the advantage
   of working when INT is 64 bits, and is also faster when overflow is
   rare.  FFTW calls this via the MULMOD macro, which further
   optimizes for the case of small integers. 
*/

#define ADD_MOD(x, y, p) ((x) >= (p) - (y)) ? ((x) + ((y) - (p))) : ((x) + (y))

INT X(safe_mulmod)(INT x, INT y, INT p)
{
     INT r;

     if (y > x) 
	  return X(safe_mulmod)(y, x, p);

     A(0 <= y && x < p);

     r = 0;
     while (y) {
	  r = ADD_MOD(r, x*(y&1), p); y >>= 1;
	  x = ADD_MOD(x, x, p);
     }

     return r;
}

/***************************************************************************/

/* Compute n^m mod p, where m >= 0 and p > 0.  If we really cared, we
   could make this tail-recursive. */

INT X(power_mod)(INT n, INT m, INT p)
{
     A(p > 0);
     if (m == 0)
	  return 1;
     else if (m % 2 == 0) {
	  INT x = X(power_mod)(n, m / 2, p);
	  return MULMOD(x, x, p);
     }
     else
	  return MULMOD(n, X(power_mod)(n, m - 1, p), p);
}

/* the following two routines were contributed by Greg Dionne. */
static INT get_prime_factors(INT n, INT *primef)
{
     INT i;
     INT size = 0;

     A(n % 2 == 0); /* this routine is designed only for even n */
     primef[size++] = (INT)2;
     do {
	  n >>= 1;
     } while ((n & 1) == 0);

     if (n == 1)
	  return size;

     for (i = 3; i * i <= n; i += 2)
	  if (!(n % i)) {
	       primef[size++] = i;
	       do {
		    n /= i;
	       } while (!(n % i));
	  }
     if (n == 1)
	  return size;
     primef[size++] = n;
     return size;
}

INT X(find_generator)(INT p)
{
    INT n, i, size;
    INT primef[16];     /* smallest number = 32589158477190044730 > 2^64 */
    INT pm1 = p - 1;

    if (p == 2)
	 return 1;

    size = get_prime_factors(pm1, primef);
    n = 2;
    for (i = 0; i < size; i++)
        if (X(power_mod)(n, pm1 / primef[i], p) == 1) {
            i = -1;
            n++;
        }
    return n;
}

/* Return first prime divisor of n  (It would be at best slightly faster to
   search a static table of primes; there are 6542 primes < 2^16.)  */
INT X(first_divisor)(INT n)
{
     INT i;
     if (n <= 1)
	  return n;
     if (n % 2 == 0)
	  return 2;
     for (i = 3; i*i <= n; i += 2)
	  if (n % i == 0)
	       return i;
     return n;
}

int X(is_prime)(INT n)
{
     return(n > 1 && X(first_divisor)(n) == n);
}

INT X(next_prime)(INT n)
{
     while (!X(is_prime)(n)) ++n;
     return n;
}

int X(factors_into)(INT n, const INT *primes)
{
     for (; *primes != 0; ++primes) 
	  while ((n % *primes) == 0) 
	       n /= *primes;
     return (n == 1);
}

/* integer square root.  Return floor(sqrt(N)) */
INT X(isqrt)(INT n)
{
     INT guess, iguess;

     A(n >= 0);
     if (n == 0) return 0;

     guess = n; iguess = 1;

     do {
          guess = (guess + iguess) / 2;
	  iguess = n / guess;
     } while (guess > iguess);

     return guess;
}

static INT isqrt_maybe(INT n)
{
     INT guess = X(isqrt)(n);
     return guess * guess == n ? guess : 0;
}

#define divides(a, b) (((b) % (a)) == 0)
INT X(choose_radix)(INT r, INT n)
{
     if (r > 0) {
	  if (divides(r, n)) return r;
	  return 0;
     } else if (r == 0) {
	  return X(first_divisor)(n);
     } else {
	  /* r is negative.  If n = (-r) * q^2, take q as the radix */
	  r = 0 - r;
	  return (n > r && divides(r, n)) ? isqrt_maybe(n / r) : 0;
     }
}

/* return A mod N, works for all A including A < 0 */
INT X(modulo)(INT a, INT n)
{
     A(n > 0);
     if (a >= 0)
	  return a % n;
     else
	  return (n - 1) - ((-(a + (INT)1)) % n);
}

/* TRUE if N factors into small primes */
int X(factors_into_small_primes)(INT n)
{
     static const INT primes[] = { 2, 3, 5, 0 };
     return X(factors_into)(n, primes);
}
