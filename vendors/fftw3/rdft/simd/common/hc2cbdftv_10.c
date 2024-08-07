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
/* Generated on Tue Sep 14 10:47:22 EDT 2021 */

#include "rdft/codelet-rdft.h"

#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)

/* Generated by: ../../../genfft/gen_hc2cdft_c.native -fma -simd -compact
 * -variables 4 -pipeline-latency 8 -trivial-stores -variables 32
 * -no-generate-bytw -n 10 -dif -sign 1 -name hc2cbdftv_10 -include
 * rdft/simd/hc2cbv.h */

/*
 * This function contains 61 FP additions, 50 FP multiplications,
 * (or, 33 additions, 22 multiplications, 28 fused multiply/add),
 * 76 stack variables, 4 constants, and 20 memory accesses
 */
#include "rdft/simd/hc2cbv.h"

static void hc2cbdftv_10(R *Rp, R *Ip, R *Rm, R *Im, const R *W, stride rs,
                         INT mb, INT me, INT ms) {
  DVK(KP559016994, +0.559016994374947424102293417182819058860154590);
  DVK(KP618033988, +0.618033988749894848204586834365638117720309180);
  DVK(KP951056516, +0.951056516295153572116439333379382143405698634);
  DVK(KP250000000, +0.250000000000000000000000000000000000000000000);
  {
    INT m;
    for (m = mb, W = W + ((mb - 1) * ((TWVL / VL) * 18)); m < me; m = m + VL,
        Rp = Rp + (VL * ms), Ip = Ip + (VL * ms), Rm = Rm - (VL * ms),
        Im = Im - (VL * ms), W = W + (TWVL * 18),
        MAKE_VOLATILE_STRIDE(40, rs)) {
      V T4, Ts, Tl, TB, Tj, Tk, Tz, TA, TF, TV, Tp, TL, Te, Tw, Th;
      V Tx, Ti, Ty, T7, Tt, Ta, Tu, Tb, Tv, T2, T3, Tc, Td, Tf, Tg;
      V T5, T6, T8, T9, TD, TE, Tn, To;
      T2 = LD(&(Rp[0]), ms, &(Rp[0]));
      T3 = LD(&(Rm[WS(rs, 4)]), -ms, &(Rm[0]));
      T4 = VFNMSCONJ(T3, T2);
      Ts = VFMACONJ(T3, T2);
      Tc = LD(&(Rp[WS(rs, 4)]), ms, &(Rp[0]));
      Td = LD(&(Rm[0]), -ms, &(Rm[0]));
      Te = VFNMSCONJ(Td, Tc);
      Tw = VFMACONJ(Td, Tc);
      Tf = LD(&(Rp[WS(rs, 1)]), ms, &(Rp[WS(rs, 1)]));
      Tg = LD(&(Rm[WS(rs, 3)]), -ms, &(Rm[WS(rs, 1)]));
      Th = VFMSCONJ(Tg, Tf);
      Tx = VFMACONJ(Tg, Tf);
      Ti = VADD(Te, Th);
      Ty = VADD(Tw, Tx);
      T5 = LD(&(Rp[WS(rs, 2)]), ms, &(Rp[0]));
      T6 = LD(&(Rm[WS(rs, 2)]), -ms, &(Rm[0]));
      T7 = VFNMSCONJ(T6, T5);
      Tt = VFMACONJ(T6, T5);
      T8 = LD(&(Rp[WS(rs, 3)]), ms, &(Rp[WS(rs, 1)]));
      T9 = LD(&(Rm[WS(rs, 1)]), -ms, &(Rm[WS(rs, 1)]));
      Ta = VFMSCONJ(T9, T8);
      Tu = VFMACONJ(T9, T8);
      Tb = VADD(T7, Ta);
      Tv = VADD(Tt, Tu);
      Tl = VSUB(Tb, Ti);
      TB = VSUB(Tv, Ty);
      Tj = VADD(Tb, Ti);
      Tk = VFNMS(LDK(KP250000000), Tj, T4);
      Tz = VADD(Tv, Ty);
      TA = VFNMS(LDK(KP250000000), Tz, Ts);
      TD = VSUB(Tw, Tx);
      TE = VSUB(Tt, Tu);
      TF = VMUL(LDK(KP951056516), VFNMS(LDK(KP618033988), TE, TD));
      TV = VMUL(LDK(KP951056516), VFMA(LDK(KP618033988), TD, TE));
      Tn = VSUB(Te, Th);
      To = VSUB(T7, Ta);
      Tp = VMUL(LDK(KP951056516), VFNMS(LDK(KP618033988), To, Tn));
      TL = VMUL(LDK(KP951056516), VFMA(LDK(KP618033988), Tn, To));
      {
        V T17, TS, Tq, T10, TW, T12, TM, T16, TG, TO, TR, Tm, T1, TZ, TU;
        V TT, T11, TK, TJ, T15, TC, Tr, TN, TH, TP, T19, TI, T18, T14, TY;
        V TQ, T13, TX;
        T17 = VADD(Ts, Tz);
        TR = LDW(&(W[TWVL * 8]));
        TS = VZMULI(TR, VADD(T4, Tj));
        Tm = VFNMS(LDK(KP559016994), Tl, Tk);
        T1 = LDW(&(W[TWVL * 4]));
        Tq = VZMULI(T1, VFMAI(Tp, Tm));
        TZ = LDW(&(W[TWVL * 12]));
        T10 = VZMULI(TZ, VFNMSI(Tp, Tm));
        TU = VFMA(LDK(KP559016994), TB, TA);
        TT = LDW(&(W[TWVL * 6]));
        TW = VZMUL(TT, VFNMSI(TV, TU));
        T11 = LDW(&(W[TWVL * 10]));
        T12 = VZMUL(T11, VFMAI(TV, TU));
        TK = VFMA(LDK(KP559016994), Tl, Tk);
        TJ = LDW(&(W[TWVL * 16]));
        TM = VZMULI(TJ, VFNMSI(TL, TK));
        T15 = LDW(&(W[0]));
        T16 = VZMULI(T15, VFMAI(TL, TK));
        TC = VFNMS(LDK(KP559016994), TB, TA);
        Tr = LDW(&(W[TWVL * 2]));
        TG = VZMUL(Tr, VFNMSI(TF, TC));
        TN = LDW(&(W[TWVL * 14]));
        TO = VZMUL(TN, VFMAI(TF, TC));
        TH = VADD(Tq, TG);
        ST(&(Rp[WS(rs, 1)]), TH, ms, &(Rp[WS(rs, 1)]));
        TP = VADD(TM, TO);
        ST(&(Rp[WS(rs, 4)]), TP, ms, &(Rp[0]));
        T19 = VCONJ(VSUB(T17, T16));
        ST(&(Rm[0]), T19, -ms, &(Rm[0]));
        TI = VCONJ(VSUB(TG, Tq));
        ST(&(Rm[WS(rs, 1)]), TI, -ms, &(Rm[WS(rs, 1)]));
        T18 = VADD(T16, T17);
        ST(&(Rp[0]), T18, ms, &(Rp[0]));
        T14 = VCONJ(VSUB(T12, T10));
        ST(&(Rm[WS(rs, 3)]), T14, -ms, &(Rm[WS(rs, 1)]));
        TY = VCONJ(VSUB(TW, TS));
        ST(&(Rm[WS(rs, 2)]), TY, -ms, &(Rm[0]));
        TQ = VCONJ(VSUB(TO, TM));
        ST(&(Rm[WS(rs, 4)]), TQ, -ms, &(Rm[0]));
        T13 = VADD(T10, T12);
        ST(&(Rp[WS(rs, 3)]), T13, ms, &(Rp[WS(rs, 1)]));
        TX = VADD(TS, TW);
        ST(&(Rp[WS(rs, 2)]), TX, ms, &(Rp[0]));
      }
    }
  }
  VLEAVE();
}

static const tw_instr twinstr[] = {
    VTW(1, 1), VTW(1, 2), VTW(1, 3), VTW(1, 4), VTW(1, 5),
    VTW(1, 6), VTW(1, 7), VTW(1, 8), VTW(1, 9), {TW_NEXT, VL, 0}};

static const hc2c_desc desc = {
    10, XSIMD_STRING("hc2cbdftv_10"), twinstr, &GENUS, {33, 22, 28, 0}};

void XSIMD(codelet_hc2cbdftv_10)(planner *p) {
  X(khc2c_register)(p, hc2cbdftv_10, &desc, HC2C_VIA_DFT);
}
#else

/* Generated by: ../../../genfft/gen_hc2cdft_c.native -simd -compact -variables
 * 4 -pipeline-latency 8 -trivial-stores -variables 32 -no-generate-bytw -n 10
 * -dif -sign 1 -name hc2cbdftv_10 -include rdft/simd/hc2cbv.h */

/*
 * This function contains 61 FP additions, 30 FP multiplications,
 * (or, 55 additions, 24 multiplications, 6 fused multiply/add),
 * 81 stack variables, 4 constants, and 20 memory accesses
 */
#include "rdft/simd/hc2cbv.h"

static void hc2cbdftv_10(R *Rp, R *Ip, R *Rm, R *Im, const R *W, stride rs,
                         INT mb, INT me, INT ms) {
  DVK(KP250000000, +0.250000000000000000000000000000000000000000000);
  DVK(KP951056516, +0.951056516295153572116439333379382143405698634);
  DVK(KP587785252, +0.587785252292473129168705954639072768597652438);
  DVK(KP559016994, +0.559016994374947424102293417182819058860154590);
  {
    INT m;
    for (m = mb, W = W + ((mb - 1) * ((TWVL / VL) * 18)); m < me; m = m + VL,
        Rp = Rp + (VL * ms), Ip = Ip + (VL * ms), Rm = Rm - (VL * ms),
        Im = Im - (VL * ms), W = W + (TWVL * 18),
        MAKE_VOLATILE_STRIDE(40, rs)) {
      V T5, TE, Ts, Tt, TC, Tz, TH, TJ, To, Tq, T2, T4, T3, T9, Tx;
      V Tm, TB, Td, Ty, Ti, TA, T6, T8, T7, Tl, Tk, Tj, Tc, Tb, Ta;
      V Tf, Th, Tg, TF, TG, Te, Tn;
      T2 = LD(&(Rp[0]), ms, &(Rp[0]));
      T3 = LD(&(Rm[WS(rs, 4)]), -ms, &(Rm[0]));
      T4 = VCONJ(T3);
      T5 = VSUB(T2, T4);
      TE = VADD(T2, T4);
      T6 = LD(&(Rp[WS(rs, 2)]), ms, &(Rp[0]));
      T7 = LD(&(Rm[WS(rs, 2)]), -ms, &(Rm[0]));
      T8 = VCONJ(T7);
      T9 = VSUB(T6, T8);
      Tx = VADD(T6, T8);
      Tl = LD(&(Rp[WS(rs, 1)]), ms, &(Rp[WS(rs, 1)]));
      Tj = LD(&(Rm[WS(rs, 3)]), -ms, &(Rm[WS(rs, 1)]));
      Tk = VCONJ(Tj);
      Tm = VSUB(Tk, Tl);
      TB = VADD(Tk, Tl);
      Tc = LD(&(Rp[WS(rs, 3)]), ms, &(Rp[WS(rs, 1)]));
      Ta = LD(&(Rm[WS(rs, 1)]), -ms, &(Rm[WS(rs, 1)]));
      Tb = VCONJ(Ta);
      Td = VSUB(Tb, Tc);
      Ty = VADD(Tb, Tc);
      Tf = LD(&(Rp[WS(rs, 4)]), ms, &(Rp[0]));
      Tg = LD(&(Rm[0]), -ms, &(Rm[0]));
      Th = VCONJ(Tg);
      Ti = VSUB(Tf, Th);
      TA = VADD(Tf, Th);
      Ts = VSUB(T9, Td);
      Tt = VSUB(Ti, Tm);
      TC = VSUB(TA, TB);
      Tz = VSUB(Tx, Ty);
      TF = VADD(Tx, Ty);
      TG = VADD(TA, TB);
      TH = VADD(TF, TG);
      TJ = VMUL(LDK(KP559016994), VSUB(TF, TG));
      Te = VADD(T9, Td);
      Tn = VADD(Ti, Tm);
      To = VADD(Te, Tn);
      Tq = VMUL(LDK(KP559016994), VSUB(Te, Tn));
      {
        V T1c, TX, Tv, T1b, TR, T15, TL, T17, TT, T11, TW, Tu, TQ, Tr, TP;
        V Tp, T1, T1a, TO, T14, TD, T10, TK, TZ, TI, Tw, T16, TS, TY, TM;
        V TU, T1e, TN, T1d, T19, T13, TV, T18, T12;
        T1c = VADD(TE, TH);
        TW = LDW(&(W[TWVL * 8]));
        TX = VZMULI(TW, VADD(T5, To));
        Tu = VBYI(VFNMS(LDK(KP951056516), Tt, VMUL(LDK(KP587785252), Ts)));
        TQ = VBYI(VFMA(LDK(KP951056516), Ts, VMUL(LDK(KP587785252), Tt)));
        Tp = VFNMS(LDK(KP250000000), To, T5);
        Tr = VSUB(Tp, Tq);
        TP = VADD(Tq, Tp);
        T1 = LDW(&(W[TWVL * 4]));
        Tv = VZMULI(T1, VSUB(Tr, Tu));
        T1a = LDW(&(W[0]));
        T1b = VZMULI(T1a, VADD(TQ, TP));
        TO = LDW(&(W[TWVL * 16]));
        TR = VZMULI(TO, VSUB(TP, TQ));
        T14 = LDW(&(W[TWVL * 12]));
        T15 = VZMULI(T14, VADD(Tu, Tr));
        TD = VBYI(VFNMS(LDK(KP951056516), TC, VMUL(LDK(KP587785252), Tz)));
        T10 = VBYI(VFMA(LDK(KP951056516), Tz, VMUL(LDK(KP587785252), TC)));
        TI = VFNMS(LDK(KP250000000), TH, TE);
        TK = VSUB(TI, TJ);
        TZ = VADD(TJ, TI);
        Tw = LDW(&(W[TWVL * 2]));
        TL = VZMUL(Tw, VADD(TD, TK));
        T16 = LDW(&(W[TWVL * 10]));
        T17 = VZMUL(T16, VADD(T10, TZ));
        TS = LDW(&(W[TWVL * 14]));
        TT = VZMUL(TS, VSUB(TK, TD));
        TY = LDW(&(W[TWVL * 6]));
        T11 = VZMUL(TY, VSUB(TZ, T10));
        TM = VADD(Tv, TL);
        ST(&(Rp[WS(rs, 1)]), TM, ms, &(Rp[WS(rs, 1)]));
        TU = VADD(TR, TT);
        ST(&(Rp[WS(rs, 4)]), TU, ms, &(Rp[0]));
        T1e = VCONJ(VSUB(T1c, T1b));
        ST(&(Rm[0]), T1e, -ms, &(Rm[0]));
        TN = VCONJ(VSUB(TL, Tv));
        ST(&(Rm[WS(rs, 1)]), TN, -ms, &(Rm[WS(rs, 1)]));
        T1d = VADD(T1b, T1c);
        ST(&(Rp[0]), T1d, ms, &(Rp[0]));
        T19 = VCONJ(VSUB(T17, T15));
        ST(&(Rm[WS(rs, 3)]), T19, -ms, &(Rm[WS(rs, 1)]));
        T13 = VCONJ(VSUB(T11, TX));
        ST(&(Rm[WS(rs, 2)]), T13, -ms, &(Rm[0]));
        TV = VCONJ(VSUB(TT, TR));
        ST(&(Rm[WS(rs, 4)]), TV, -ms, &(Rm[0]));
        T18 = VADD(T15, T17);
        ST(&(Rp[WS(rs, 3)]), T18, ms, &(Rp[WS(rs, 1)]));
        T12 = VADD(TX, T11);
        ST(&(Rp[WS(rs, 2)]), T12, ms, &(Rp[0]));
      }
    }
  }
  VLEAVE();
}

static const tw_instr twinstr[] = {
    VTW(1, 1), VTW(1, 2), VTW(1, 3), VTW(1, 4), VTW(1, 5),
    VTW(1, 6), VTW(1, 7), VTW(1, 8), VTW(1, 9), {TW_NEXT, VL, 0}};

static const hc2c_desc desc = {
    10, XSIMD_STRING("hc2cbdftv_10"), twinstr, &GENUS, {55, 24, 6, 0}};

void XSIMD(codelet_hc2cbdftv_10)(planner *p) {
  X(khc2c_register)(p, hc2cbdftv_10, &desc, HC2C_VIA_DFT);
}
#endif
