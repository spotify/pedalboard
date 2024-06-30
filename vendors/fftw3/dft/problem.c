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


#include "dft/dft.h"
#include <stddef.h>

static void destroy(problem *ego_)
{
     problem_dft *ego = (problem_dft *) ego_;
     X(tensor_destroy2)(ego->vecsz, ego->sz);
     X(ifree)(ego_);
}

static void hash(const problem *p_, md5 *m)
{
     const problem_dft *p = (const problem_dft *) p_;
     X(md5puts)(m, "dft");
     X(md5int)(m, p->ri == p->ro);
     X(md5INT)(m, p->ii - p->ri);
     X(md5INT)(m, p->io - p->ro);
     X(md5int)(m, X(ialignment_of)(p->ri));
     X(md5int)(m, X(ialignment_of)(p->ii));
     X(md5int)(m, X(ialignment_of)(p->ro));
     X(md5int)(m, X(ialignment_of)(p->io));
     X(tensor_md5)(m, p->sz);
     X(tensor_md5)(m, p->vecsz);
}

static void print(const problem *ego_, printer *p)
{
     const problem_dft *ego = (const problem_dft *) ego_;
     p->print(p, "(dft %d %d %d %D %D %T %T)", 
	      ego->ri == ego->ro,
	      X(ialignment_of)(ego->ri),
	      X(ialignment_of)(ego->ro),
	      (INT)(ego->ii - ego->ri), 
	      (INT)(ego->io - ego->ro),
	      ego->sz,
	      ego->vecsz);
}

static void zero(const problem *ego_)
{
     const problem_dft *ego = (const problem_dft *) ego_;
     tensor *sz = X(tensor_append)(ego->vecsz, ego->sz);
     X(dft_zerotens)(sz, UNTAINT(ego->ri), UNTAINT(ego->ii));
     X(tensor_destroy)(sz);
}

static const problem_adt padt =
{
     PROBLEM_DFT,
     hash,
     zero,
     print,
     destroy
};

problem *X(mkproblem_dft)(const tensor *sz, const tensor *vecsz,
			  R *ri, R *ii, R *ro, R *io)
{
     problem_dft *ego;

     /* enforce pointer equality if untainted pointers are equal */
     if (UNTAINT(ri) == UNTAINT(ro))
	  ri = ro = JOIN_TAINT(ri, ro);
     if (UNTAINT(ii) == UNTAINT(io))
	  ii = io = JOIN_TAINT(ii, io);

     /* more correctness conditions: */
     A(TAINTOF(ri) == TAINTOF(ii));
     A(TAINTOF(ro) == TAINTOF(io));

     A(X(tensor_kosherp)(sz));
     A(X(tensor_kosherp)(vecsz));

     if (ri == ro || ii == io) {
	  /* If either real or imag pointers are in place, both must be. */
	  if (ri != ro || ii != io || !X(tensor_inplace_locations)(sz, vecsz))
	       return X(mkproblem_unsolvable)();
     }

     ego = (problem_dft *)X(mkproblem)(sizeof(problem_dft), &padt);

     ego->sz = X(tensor_compress)(sz);
     ego->vecsz = X(tensor_compress_contiguous)(vecsz);
     ego->ri = ri;
     ego->ii = ii;
     ego->ro = ro;
     ego->io = io;

     A(FINITE_RNK(ego->sz->rnk));
     return &(ego->super);
}

/* Same as X(mkproblem_dft), but also destroy input tensors. */
problem *X(mkproblem_dft_d)(tensor *sz, tensor *vecsz,
			    R *ri, R *ii, R *ro, R *io)
{
     problem *p = X(mkproblem_dft)(sz, vecsz, ri, ii, ro, io);
     X(tensor_destroy2)(vecsz, sz);
     return p;
}
