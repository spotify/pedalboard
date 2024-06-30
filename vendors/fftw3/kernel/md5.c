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

/* 
   independent implementation of Ron Rivest's MD5 message-digest
   algorithm, based on rfc 1321.

   Optimized for small code size, not speed.  Works as long as
   sizeof(md5uint) >= 4.
*/

#include "kernel/ifftw.h"

/* sintab[i] = 4294967296.0 * abs(sin((double)(i + 1))) */
static const md5uint sintab[64] = {
     0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
     0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
     0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
     0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
     0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
     0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
     0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
     0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
     0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
     0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
     0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
     0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
     0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
     0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
     0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
     0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
}; 

/* see rfc 1321 section 3.4 */
static const struct roundtab {
     char k; 
     char s;
} roundtab[64] = {
     {  0,  7}, {  1, 12}, {  2, 17}, {  3, 22},
     {  4,  7}, {  5, 12}, {  6, 17}, {  7, 22},
     {  8,  7}, {  9, 12}, { 10, 17}, { 11, 22},
     { 12,  7}, { 13, 12}, { 14, 17}, { 15, 22},
     {  1,  5}, {  6,  9}, { 11, 14}, {  0, 20},
     {  5,  5}, { 10,  9}, { 15, 14}, {  4, 20},
     {  9,  5}, { 14,  9}, {  3, 14}, {  8, 20},
     { 13,  5}, {  2,  9}, {  7, 14}, { 12, 20},
     {  5,  4}, {  8, 11}, { 11, 16}, { 14, 23},
     {  1,  4}, {  4, 11}, {  7, 16}, { 10, 23},
     { 13,  4}, {  0, 11}, {  3, 16}, {  6, 23},
     {  9,  4}, { 12, 11}, { 15, 16}, {  2, 23},
     {  0,  6}, {  7, 10}, { 14, 15}, {  5, 21},
     { 12,  6}, {  3, 10}, { 10, 15}, {  1, 21},
     {  8,  6}, { 15, 10}, {  6, 15}, { 13, 21},
     {  4,  6}, { 11, 10}, {  2, 15}, {  9, 21}
};

#define rol(a, s) ((a << (int)(s)) | (a >> (32 - (int)(s))))

static void doblock(md5sig state, const unsigned char *data)
{
     md5uint a, b, c, d, t, x[16];
     const md5uint msk = (md5uint)0xffffffffUL;
     int i;

     /* encode input bytes into md5uint */
     for (i = 0; i < 16; ++i) {
	  const unsigned char *p = data + 4 * i;
	  x[i] = (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
     }

     a = state[0]; b = state[1]; c = state[2]; d = state[3];
     for (i = 0; i < 64; ++i) {
	  const struct roundtab *p = roundtab + i;
	  switch (i >> 4) {
	      case 0: a += (b & c) | (~b & d); break;
	      case 1: a += (b & d) | (c & ~d); break;
	      case 2: a += b ^ c ^ d; break;
	      case 3: a += c ^ (b | ~d); break;
	  }
	  a += sintab[i];
	  a += x[(int)(p->k)];
	  a &= msk;
	  t = b + rol(a, p->s);
	  a = d; d = c; c = b; b = t;
     }
     state[0] = (state[0] + a) & msk;
     state[1] = (state[1] + b) & msk;
     state[2] = (state[2] + c) & msk;
     state[3] = (state[3] + d) & msk;
}


void X(md5begin)(md5 *p)
{
     p->s[0] = 0x67452301;
     p->s[1] = 0xefcdab89;
     p->s[2] = 0x98badcfe;
     p->s[3] = 0x10325476;
     p->l = 0;
}

void X(md5putc)(md5 *p, unsigned char c)
{
     p->c[p->l % 64] = c;
     if (((++p->l) % 64) == 0) doblock(p->s, p->c);
}

void X(md5end)(md5 *p)
{
     unsigned l, i;

     l = 8 * p->l; /* length before padding, in bits */

     /* rfc 1321 section 3.1: padding */
     X(md5putc)(p, 0x80);
     while ((p->l % 64) != 56) X(md5putc)(p, 0x00);

     /* rfc 1321 section 3.2: length (little endian) */
     for (i = 0; i < 8; ++i) {
	  X(md5putc)(p, (unsigned char)(l & 0xFF));
	  l = l >> 8;
     }

     /* Now p->l % 64 == 0 and signature is in p->s */
}
