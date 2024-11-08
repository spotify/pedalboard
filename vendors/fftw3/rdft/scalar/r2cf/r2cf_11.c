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
/* Generated on Tue Sep 14 10:46:10 EDT 2021 */

#include "rdft/codelet-rdft.h"

#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)

/* Generated by: ../../../genfft/gen_r2cf.native -fma -compact -variables 4
 * -pipeline-latency 4 -n 11 -name r2cf_11 -include rdft/scalar/r2cf.h */

/*
 * This function contains 60 FP additions, 50 FP multiplications,
 * (or, 15 additions, 5 multiplications, 45 fused multiply/add),
 * 42 stack variables, 10 constants, and 22 memory accesses
 */
#include "rdft/scalar/r2cf.h"

static void r2cf_11(R *R0, R *R1, R *Cr, R *Ci, stride rs, stride csr,
                    stride csi, INT v, INT ivs, INT ovs) {
  DK(KP918985947, +0.918985947228994779780736114132655398124909697);
  DK(KP989821441, +0.989821441880932732376092037776718787376519372);
  DK(KP830830026, +0.830830026003772851058548298459246407048009821);
  DK(KP715370323, +0.715370323453429719112414662767260662417897278);
  DK(KP959492973, +0.959492973614497389890368057066327699062454848);
  DK(KP876768831, +0.876768831002589333891339807079336796764054852);
  DK(KP778434453, +0.778434453334651800608337670740821884709317477);
  DK(KP634356270, +0.634356270682424498893150776899916060542806975);
  DK(KP342584725, +0.342584725681637509502641509861112333758894680);
  DK(KP521108558, +0.521108558113202722944698153526659300680427422);
  {
    INT i;
    for (i = v; i > 0; i = i - 1, R0 = R0 + ivs, R1 = R1 + ivs, Cr = Cr + ovs,
        Ci = Ci + ovs, MAKE_VOLATILE_STRIDE(44, rs),
        MAKE_VOLATILE_STRIDE(44, csr), MAKE_VOLATILE_STRIDE(44, csi)) {
      E T1, T4, TC, Tg, TE, T7, TD, Ta, TF, Td, TB, TG, TM, TS, TJ;
      E TP, Ty, Tq, Ti, Tu, Tm, T5, T6;
      T1 = R0[0];
      {
        E T2, T3, Te, Tf;
        T2 = R1[0];
        T3 = R0[WS(rs, 5)];
        T4 = T2 + T3;
        TC = T3 - T2;
        Te = R1[WS(rs, 2)];
        Tf = R0[WS(rs, 3)];
        Tg = Te + Tf;
        TE = Tf - Te;
      }
      T5 = R0[WS(rs, 1)];
      T6 = R1[WS(rs, 4)];
      T7 = T5 + T6;
      TD = T5 - T6;
      {
        E T8, T9, Tb, Tc;
        T8 = R1[WS(rs, 1)];
        T9 = R0[WS(rs, 4)];
        Ta = T8 + T9;
        TF = T9 - T8;
        Tb = R0[WS(rs, 2)];
        Tc = R1[WS(rs, 3)];
        Td = Tb + Tc;
        TB = Tb - Tc;
      }
      TG = FMA(KP521108558, TF, TE);
      TM = FNMS(KP521108558, TD, TB);
      TS = FMA(KP521108558, TC, TD);
      TJ = FMA(KP521108558, TE, TC);
      TP = FNMS(KP521108558, TB, TF);
      {
        E Tx, Tp, Th, Tt, Tl;
        Tx = FNMS(KP342584725, Ta, T7);
        Ty = FNMS(KP634356270, Tx, Td);
        Tp = FNMS(KP342584725, T4, Ta);
        Tq = FNMS(KP634356270, Tp, Tg);
        Th = FNMS(KP342584725, Tg, Td);
        Ti = FNMS(KP634356270, Th, Ta);
        Tt = FNMS(KP342584725, Td, T4);
        Tu = FNMS(KP634356270, Tt, T7);
        Tl = FNMS(KP342584725, T7, Tg);
        Tm = FNMS(KP634356270, Tl, T4);
      }
      {
        E To, Tn, TI, TH;
        {
          E Tk, Tj, TU, TT;
          Tj = FNMS(KP778434453, Ti, T7);
          Tk = FNMS(KP876768831, Tj, T4);
          Cr[WS(csr, 5)] = FNMS(KP959492973, Tk, T1);
          TT = FMA(KP715370323, TS, TF);
          TU = FMA(KP830830026, TT, TB);
          Ci[WS(csi, 5)] = KP989821441 * (FMA(KP918985947, TU, TE));
        }
        Tn = FNMS(KP778434453, Tm, Ta);
        To = FNMS(KP876768831, Tn, Td);
        Cr[WS(csr, 4)] = FNMS(KP959492973, To, T1);
        {
          E TR, TQ, Ts, Tr;
          TQ = FMA(KP715370323, TP, TC);
          TR = FNMS(KP830830026, TQ, TE);
          Ci[WS(csi, 4)] = KP989821441 * (FNMS(KP918985947, TR, TD));
          Tr = FNMS(KP778434453, Tq, Td);
          Ts = FNMS(KP876768831, Tr, T7);
          Cr[WS(csr, 3)] = FNMS(KP959492973, Ts, T1);
        }
        {
          E TO, TN, Tw, Tv;
          TN = FNMS(KP715370323, TM, TE);
          TO = FNMS(KP830830026, TN, TF);
          Ci[WS(csi, 3)] = KP989821441 * (FNMS(KP918985947, TO, TC));
          Tv = FNMS(KP778434453, Tu, Tg);
          Tw = FNMS(KP876768831, Tv, Ta);
          Cr[WS(csr, 2)] = FNMS(KP959492973, Tw, T1);
          Cr[0] = T1 + T4 + T7 + Ta + Td + Tg;
        }
        TH = FMA(KP715370323, TG, TD);
        TI = FNMS(KP830830026, TH, TC);
        Ci[WS(csi, 2)] = KP989821441 * (FMA(KP918985947, TI, TB));
        {
          E TL, TK, TA, Tz;
          TK = FNMS(KP715370323, TJ, TB);
          TL = FMA(KP830830026, TK, TD);
          Ci[WS(csi, 1)] = KP989821441 * (FNMS(KP918985947, TL, TF));
          Tz = FNMS(KP778434453, Ty, T4);
          TA = FNMS(KP876768831, Tz, Tg);
          Cr[WS(csr, 1)] = FNMS(KP959492973, TA, T1);
        }
      }
    }
  }
}

static const kr2c_desc desc = {11, "r2cf_11", {15, 5, 45, 0}, &GENUS};

void X(codelet_r2cf_11)(planner *p) { X(kr2c_register)(p, r2cf_11, &desc); }

#else

/* Generated by: ../../../genfft/gen_r2cf.native -compact -variables 4
 * -pipeline-latency 4 -n 11 -name r2cf_11 -include rdft/scalar/r2cf.h */

/*
 * This function contains 60 FP additions, 50 FP multiplications,
 * (or, 20 additions, 10 multiplications, 40 fused multiply/add),
 * 28 stack variables, 10 constants, and 22 memory accesses
 */
#include "rdft/scalar/r2cf.h"

static void r2cf_11(R *R0, R *R1, R *Cr, R *Ci, stride rs, stride csr,
                    stride csi, INT v, INT ivs, INT ovs) {
  DK(KP654860733, +0.654860733945285064056925072466293553183791199);
  DK(KP142314838, +0.142314838273285140443792668616369668791051361);
  DK(KP959492973, +0.959492973614497389890368057066327699062454848);
  DK(KP415415013, +0.415415013001886425529274149229623203524004910);
  DK(KP841253532, +0.841253532831181168861811648919367717513292498);
  DK(KP989821441, +0.989821441880932732376092037776718787376519372);
  DK(KP909631995, +0.909631995354518371411715383079028460060241051);
  DK(KP281732556, +0.281732556841429697711417915346616899035777899);
  DK(KP540640817, +0.540640817455597582107635954318691695431770608);
  DK(KP755749574, +0.755749574354258283774035843972344420179717445);
  {
    INT i;
    for (i = v; i > 0; i = i - 1, R0 = R0 + ivs, R1 = R1 + ivs, Cr = Cr + ovs,
        Ci = Ci + ovs, MAKE_VOLATILE_STRIDE(44, rs),
        MAKE_VOLATILE_STRIDE(44, csr), MAKE_VOLATILE_STRIDE(44, csi)) {
      E T1, T4, Tl, Tg, Th, Td, Ti, Ta, Tk, T7, Tj, Tb, Tc;
      T1 = R0[0];
      {
        E T2, T3, Te, Tf;
        T2 = R0[WS(rs, 1)];
        T3 = R1[WS(rs, 4)];
        T4 = T2 + T3;
        Tl = T3 - T2;
        Te = R1[0];
        Tf = R0[WS(rs, 5)];
        Tg = Te + Tf;
        Th = Tf - Te;
      }
      Tb = R1[WS(rs, 1)];
      Tc = R0[WS(rs, 4)];
      Td = Tb + Tc;
      Ti = Tc - Tb;
      {
        E T8, T9, T5, T6;
        T8 = R1[WS(rs, 2)];
        T9 = R0[WS(rs, 3)];
        Ta = T8 + T9;
        Tk = T9 - T8;
        T5 = R0[WS(rs, 2)];
        T6 = R1[WS(rs, 3)];
        T7 = T5 + T6;
        Tj = T6 - T5;
      }
      Ci[WS(csi, 4)] = FMA(KP755749574, Th, KP540640817 * Ti) +
                       FNMS(KP909631995, Tk, KP281732556 * Tj) -
                       (KP989821441 * Tl);
      Cr[WS(csr, 4)] = FMA(KP841253532, Td, T1) +
                       FNMS(KP959492973, T7, KP415415013 * Ta) +
                       FNMA(KP142314838, T4, KP654860733 * Tg);
      Ci[WS(csi, 2)] = FMA(KP909631995, Th, KP755749574 * Tl) +
                       FNMA(KP540640817, Tk, KP989821441 * Tj) -
                       (KP281732556 * Ti);
      Ci[WS(csi, 5)] = FMA(KP281732556, Th, KP755749574 * Ti) +
                       FNMS(KP909631995, Tj, KP989821441 * Tk) -
                       (KP540640817 * Tl);
      Ci[WS(csi, 1)] = FMA(KP540640817, Th, KP909631995 * Tl) +
                       FMA(KP989821441, Ti, KP755749574 * Tj) +
                       (KP281732556 * Tk);
      Ci[WS(csi, 3)] = FMA(KP989821441, Th, KP540640817 * Tj) +
                       FNMS(KP909631995, Ti, KP755749574 * Tk) -
                       (KP281732556 * Tl);
      Cr[WS(csr, 3)] = FMA(KP415415013, Td, T1) +
                       FNMS(KP654860733, Ta, KP841253532 * T7) +
                       FNMA(KP959492973, T4, KP142314838 * Tg);
      Cr[WS(csr, 1)] = FMA(KP841253532, Tg, T1) +
                       FNMS(KP959492973, Ta, KP415415013 * T4) +
                       FNMA(KP654860733, T7, KP142314838 * Td);
      Cr[0] = T1 + Tg + T4 + Td + T7 + Ta;
      Cr[WS(csr, 2)] = FMA(KP415415013, Tg, T1) +
                       FNMS(KP142314838, T7, KP841253532 * Ta) +
                       FNMA(KP959492973, Td, KP654860733 * T4);
      Cr[WS(csr, 5)] = FMA(KP841253532, T4, T1) +
                       FNMS(KP142314838, Ta, KP415415013 * T7) +
                       FNMA(KP654860733, Td, KP959492973 * Tg);
    }
  }
}

static const kr2c_desc desc = {11, "r2cf_11", {20, 10, 40, 0}, &GENUS};

void X(codelet_r2cf_11)(planner *p) { X(kr2c_register)(p, r2cf_11, &desc); }

#endif
