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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/* This file was automatically generated --- DO NOT EDIT */
/* Generated on Tue Sep 14 10:46:59 EDT 2021 */

#include "rdft/codelet-rdft.h"

#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)

/* Generated by: ../../../genfft/gen_r2cb.native -fma -compact -variables 4
 * -pipeline-latency 4 -sign 1 -n 3 -name r2cbIII_3 -dft-III -include
 * rdft/scalar/r2cbIII.h */

/*
 * This function contains 4 FP additions, 3 FP multiplications,
 * (or, 1 additions, 0 multiplications, 3 fused multiply/add),
 * 7 stack variables, 2 constants, and 6 memory accesses
 */
#include "rdft/scalar/r2cbIII.h"

static void r2cbIII_3(R *R0, R *R1, R *Cr, R *Ci, stride rs, stride csr,
                      stride csi, INT v, INT ivs, INT ovs) {
  DK(KP1_732050807, +1.732050807568877293527446341505872366942805254);
  DK(KP2_000000000, +2.000000000000000000000000000000000000000000000);
  {
    INT i;
    for (i = v; i > 0; i = i - 1, R0 = R0 + ovs, R1 = R1 + ovs, Cr = Cr + ivs,
        Ci = Ci + ivs, MAKE_VOLATILE_STRIDE(12, rs),
        MAKE_VOLATILE_STRIDE(12, csr), MAKE_VOLATILE_STRIDE(12, csi)) {
      E T4, T1, T2, T3;
      T4 = Ci[0];
      T1 = Cr[WS(csr, 1)];
      T2 = Cr[0];
      T3 = T2 - T1;
      R0[0] = FMA(KP2_000000000, T2, T1);
      R0[WS(rs, 1)] = -(FMA(KP1_732050807, T4, T3));
      R1[0] = FNMS(KP1_732050807, T4, T3);
    }
  }
}

static const kr2c_desc desc = {3, "r2cbIII_3", {1, 0, 3, 0}, &GENUS};

void X(codelet_r2cbIII_3)(planner *p) { X(kr2c_register)(p, r2cbIII_3, &desc); }

#else

/* Generated by: ../../../genfft/gen_r2cb.native -compact -variables 4
 * -pipeline-latency 4 -sign 1 -n 3 -name r2cbIII_3 -dft-III -include
 * rdft/scalar/r2cbIII.h */

/*
 * This function contains 4 FP additions, 2 FP multiplications,
 * (or, 3 additions, 1 multiplications, 1 fused multiply/add),
 * 8 stack variables, 2 constants, and 6 memory accesses
 */
#include "rdft/scalar/r2cbIII.h"

static void r2cbIII_3(R *R0, R *R1, R *Cr, R *Ci, stride rs, stride csr,
                      stride csi, INT v, INT ivs, INT ovs) {
  DK(KP2_000000000, +2.000000000000000000000000000000000000000000000);
  DK(KP1_732050807, +1.732050807568877293527446341505872366942805254);
  {
    INT i;
    for (i = v; i > 0; i = i - 1, R0 = R0 + ovs, R1 = R1 + ovs, Cr = Cr + ivs,
        Ci = Ci + ivs, MAKE_VOLATILE_STRIDE(12, rs),
        MAKE_VOLATILE_STRIDE(12, csr), MAKE_VOLATILE_STRIDE(12, csi)) {
      E T5, T1, T2, T3, T4;
      T4 = Ci[0];
      T5 = KP1_732050807 * T4;
      T1 = Cr[WS(csr, 1)];
      T2 = Cr[0];
      T3 = T2 - T1;
      R0[0] = FMA(KP2_000000000, T2, T1);
      R0[WS(rs, 1)] = -(T3 + T5);
      R1[0] = T3 - T5;
    }
  }
}

static const kr2c_desc desc = {3, "r2cbIII_3", {3, 1, 1, 0}, &GENUS};

void X(codelet_r2cbIII_3)(planner *p) { X(kr2c_register)(p, r2cbIII_3, &desc); }

#endif
