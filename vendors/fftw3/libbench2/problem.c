/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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


#include "config.h"
#include "libbench2/bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int always_pad_real = 0; /* by default, only pad in-place case */

typedef enum {
     SAME, PADDED, HALFISH
} n_transform;

/* funny transformations for last dimension of PROBLEM_REAL */
static int transform_n(int n, n_transform nt)
{
     switch (nt) {
	 case SAME: return n;
	 case PADDED: return 2*(n/2+1);
	 case HALFISH: return (n/2+1);
	 default: BENCH_ASSERT(0); return 0;
     }
}

/* do what I mean */
static bench_tensor *dwim(bench_tensor *t, bench_iodim **last_iodim,
			  n_transform nti, n_transform nto,
			  bench_iodim *dt)
{
     int i;
     bench_iodim *d, *d1;

     if (!BENCH_FINITE_RNK(t->rnk) || t->rnk < 1)
	  return t;

     i = t->rnk;
     d1 = *last_iodim;

     while (--i >= 0) {
	  d = t->dims + i;
	  if (!d->is) 
	       d->is = d1->is * transform_n(d1->n, d1==dt ? nti : SAME); 
	  if (!d->os) 
	       d->os = d1->os * transform_n(d1->n, d1==dt ? nto : SAME); 
	  d1 = d;
     }

     *last_iodim = d1;
     return t;
}

static void transpose_tensor(bench_tensor *t)
{
     if (!BENCH_FINITE_RNK(t->rnk) || t->rnk < 2)
          return;

     t->dims[0].os = t->dims[1].os;
     t->dims[1].os = t->dims[0].os * t->dims[0].n;
}

static const char *parseint(const char *s, int *n)
{
     int sign = 1;

     *n = 0;

     if (*s == '-') { 
	  sign = -1;
	  ++s;
     } else if (*s == '+') { 
	  sign = +1; 
	  ++s; 
     }

     BENCH_ASSERT(isdigit(*s));
     while (isdigit(*s)) {
	  *n = *n * 10 + (*s - '0');
	  ++s;
     }
     
     *n *= sign;

     if (*s == 'k' || *s == 'K') {
	  *n *= 1024;
	  ++s;
     }

     if (*s == 'm' || *s == 'M') {
	  *n *= 1024 * 1024;
	  ++s;
     }

     return s;
}

struct dimlist { bench_iodim car; r2r_kind_t k; struct dimlist *cdr; };

static const char *parsetensor(const char *s, bench_tensor **tp,
			       r2r_kind_t **k)
{
     struct dimlist *l = 0, *m;
     bench_tensor *t;
     int rnk = 0;

 L1:
     m = (struct dimlist *)bench_malloc(sizeof(struct dimlist));
     /* nconc onto l */
     m->cdr = l; l = m;
     ++rnk; 

     s = parseint(s, &m->car.n);

     if (*s == ':') {
	  /* read input stride */
	  ++s;
	  s = parseint(s, &m->car.is);
	  if (*s == ':') {
	       /* read output stride */
	       ++s;
	       s = parseint(s, &m->car.os);
	  } else {
	       /* default */
	       m->car.os = m->car.is;
	  }
     } else {
	  m->car.is = 0;
	  m->car.os = 0;
     }

     if (*s == 'f' || *s == 'F') {
	  m->k = R2R_R2HC;
	  ++s;
     }
     else if (*s == 'b' || *s == 'B') {
	  m->k = R2R_HC2R;
	  ++s;
     }
     else if (*s == 'h' || *s == 'H') {
	  m->k = R2R_DHT;
	  ++s;
     }
     else if (*s == 'e' || *s == 'E' || *s == 'o' || *s == 'O') {
	  char c = *(s++);
	  int ab;

	  s = parseint(s, &ab);

	  if (c == 'e' || c == 'E') {
	       if (ab == 0)
		    m->k = R2R_REDFT00;
	       else if (ab == 1)
		    m->k = R2R_REDFT01;
	       else if (ab == 10)
		    m->k = R2R_REDFT10;
	       else if (ab == 11)
		    m->k = R2R_REDFT11;
	       else
		    BENCH_ASSERT(0);
	  }
	  else {
	       if (ab == 0)
		    m->k = R2R_RODFT00;
	       else if (ab == 1)
		    m->k = R2R_RODFT01;
	       else if (ab == 10)
		    m->k = R2R_RODFT10;
	       else if (ab == 11)
		    m->k = R2R_RODFT11;
	       else
		    BENCH_ASSERT(0);
	  }
     }
     else
	  m->k = R2R_R2HC;

     if (*s == 'x' || *s == 'X') {
	  ++s;
	  goto L1;
     }
     
     /* now we have a dimlist.  Build bench_tensor, etc. */

     if (k && rnk > 0) {
	  int i;
	  *k = (r2r_kind_t *) bench_malloc(sizeof(r2r_kind_t) * rnk);
	  for (m = l, i = rnk - 1; i >= 0; --i, m = m->cdr) {
	       BENCH_ASSERT(m);
	       (*k)[i] = m->k;
	  }
     }

     t = mktensor(rnk);
     while (--rnk >= 0) {
	  bench_iodim *d = t->dims + rnk;
	  BENCH_ASSERT(l);
	  m = l; l = m->cdr;
	  d->n = m->car.n;
	  d->is = m->car.is;
	  d->os = m->car.os;
	  bench_free(m);
     }

     *tp = t;
     return s;
}

/* parse a problem description, return a problem */
bench_problem *problem_parse(const char *s)
{
     bench_problem *p;
     bench_iodim last_iodim0 = {1,1,1}, *last_iodim = &last_iodim0;
     bench_iodim *sz_last_iodim;
     bench_tensor *sz;
     n_transform nti = SAME, nto = SAME;
     int transpose = 0;

     p = (bench_problem *) bench_malloc(sizeof(bench_problem));
     p->kind = PROBLEM_COMPLEX;
     p->k = 0;
     p->sign = -1;
     p->in = p->out = 0;
     p->inphys = p->outphys = 0;
     p->iphyssz = p->ophyssz = 0;
     p->in_place = 0;
     p->destroy_input = 0;
     p->split = 0;
     p->userinfo = 0;
     p->scrambled_in = p->scrambled_out = 0;
     p->sz = p->vecsz = 0;
     p->ini = p->outi = 0;
     p->pstring = (char *) bench_malloc(sizeof(char) * (strlen(s) + 1));
     strcpy(p->pstring, s);

 L1:
     switch (tolower(*s)) {
	 case 'i': p->in_place = 1; ++s; goto L1;
	 case 'o': p->in_place = 0; ++s; goto L1;
	 case 'd': p->destroy_input = 1; ++s; goto L1;
	 case '/': p->split = 1; ++s; goto L1;
	 case 'f': 
	 case '-': p->sign = -1; ++s; goto L1;
	 case 'b': 
	 case '+': p->sign = 1; ++s; goto L1;
	 case 'r': p->kind = PROBLEM_REAL; ++s; goto L1;
	 case 'c': p->kind = PROBLEM_COMPLEX; ++s; goto L1;
	 case 'k': p->kind = PROBLEM_R2R; ++s; goto L1;
	 case 't': transpose = 1; ++s; goto L1;
	      
	 /* hack for MPI: */
	 case '[': p->scrambled_in = 1; ++s; goto L1;
	 case ']': p->scrambled_out = 1; ++s; goto L1;

	 default : ;
     }

     s = parsetensor(s, &sz, p->kind == PROBLEM_R2R ? &p->k : 0);

     if (p->kind == PROBLEM_REAL) {
	  if (p->sign < 0) {
	       nti = p->in_place || always_pad_real ? PADDED : SAME;
	       nto = HALFISH;
	  }
	  else {
	       nti = HALFISH;
	       nto = p->in_place || always_pad_real ? PADDED : SAME;
	  }
     }

     sz_last_iodim = sz->dims + sz->rnk - 1;
     if (*s == '*') { /* "external" vector */
	  ++s;
	  p->sz = dwim(sz, &last_iodim, nti, nto, sz_last_iodim);
	  s = parsetensor(s, &sz, 0);
	  p->vecsz = dwim(sz, &last_iodim, nti, nto, sz_last_iodim);
     } else if (*s == 'v' || *s == 'V') { /* "internal" vector */
	  bench_tensor *vecsz;
	  ++s;
	  s = parsetensor(s, &vecsz, 0);
	  p->vecsz = dwim(vecsz, &last_iodim, nti, nto, sz_last_iodim);
	  p->sz = dwim(sz, &last_iodim, nti, nto, sz_last_iodim);
     } else {
	  p->sz = dwim(sz, &last_iodim, nti, nto, sz_last_iodim);
	  p->vecsz = mktensor(0);
     }

     if (transpose) {
	  transpose_tensor(p->sz);
	  transpose_tensor(p->vecsz);
     }

     if (!p->in_place)
	  p->out = ((bench_real *) p->in) + (1 << 20);  /* whatever */

     BENCH_ASSERT(p->sz && p->vecsz);
     BENCH_ASSERT(!*s);
     return p;
}

void problem_destroy(bench_problem *p)
{
     BENCH_ASSERT(p);
     problem_free(p);
     bench_free0(p->k);
     bench_free0(p->pstring);
     bench_free(p);
}

