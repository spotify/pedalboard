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
/* Generated on Tue Sep 14 10:45:48 EDT 2021 */

#include "dft/codelet-dft.h"

#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)

/* Generated by: ../../../genfft/gen_twiddle_c.native -fma -simd -compact
 * -variables 4 -pipeline-latency 8 -twiddle-log3 -precompute-twiddles
 * -no-generate-bytw -n 10 -name t3fv_10 -include dft/simd/t3f.h */

/*
 * This function contains 57 FP additions, 52 FP multiplications,
 * (or, 39 additions, 34 multiplications, 18 fused multiply/add),
 * 41 stack variables, 4 constants, and 20 memory accesses
 */
#include "dft/simd/t3f.h"

static void t3fv_10(R *ri, R *ii, const R *W, stride rs, INT mb, INT me,
                    INT ms) {
  DVK(KP559016994, +0.559016994374947424102293417182819058860154590);
  DVK(KP618033988, +0.618033988749894848204586834365638117720309180);
  DVK(KP951056516, +0.951056516295153572116439333379382143405698634);
  DVK(KP250000000, +0.250000000000000000000000000000000000000000000);
  {
    INT m;
    R *x;
    x = ri;
    for (m = mb, W = W + (mb * ((TWVL / VL) * 6)); m < me; m = m + VL,
        x = x + (VL * ms), W = W + (TWVL * 6), MAKE_VOLATILE_STRIDE(10, rs)) {
      V T2, T3, T4, Ta, T5, T6, Tt, Td, Th;
      T2 = LDW(&(W[0]));
      T3 = LDW(&(W[TWVL * 2]));
      T4 = VZMUL(T2, T3);
      Ta = VZMULJ(T2, T3);
      T5 = LDW(&(W[TWVL * 4]));
      T6 = VZMULJ(T4, T5);
      Tt = VZMULJ(T3, T5);
      Td = VZMULJ(Ta, T5);
      Th = VZMULJ(T2, T5);
      {
        V T9, TJ, Ts, Ty, Tz, TN, TO, TP, Tg, Tm, Tn, TK, TL, TM, T1;
        V T8, T7;
        T1 = LD(&(x[0]), ms, &(x[0]));
        T7 = LD(&(x[WS(rs, 5)]), ms, &(x[WS(rs, 1)]));
        T8 = VZMULJ(T6, T7);
        T9 = VSUB(T1, T8);
        TJ = VADD(T1, T8);
        {
          V Tp, Tx, Tr, Tv;
          {
            V To, Tw, Tq, Tu;
            To = LD(&(x[WS(rs, 4)]), ms, &(x[0]));
            Tp = VZMULJ(T4, To);
            Tw = LD(&(x[WS(rs, 1)]), ms, &(x[WS(rs, 1)]));
            Tx = VZMULJ(T2, Tw);
            Tq = LD(&(x[WS(rs, 9)]), ms, &(x[WS(rs, 1)]));
            Tr = VZMULJ(T5, Tq);
            Tu = LD(&(x[WS(rs, 6)]), ms, &(x[0]));
            Tv = VZMULJ(Tt, Tu);
          }
          Ts = VSUB(Tp, Tr);
          Ty = VSUB(Tv, Tx);
          Tz = VADD(Ts, Ty);
          TN = VADD(Tp, Tr);
          TO = VADD(Tv, Tx);
          TP = VADD(TN, TO);
        }
        {
          V Tc, Tl, Tf, Tj;
          {
            V Tb, Tk, Te, Ti;
            Tb = LD(&(x[WS(rs, 2)]), ms, &(x[0]));
            Tc = VZMULJ(Ta, Tb);
            Tk = LD(&(x[WS(rs, 3)]), ms, &(x[WS(rs, 1)]));
            Tl = VZMULJ(T3, Tk);
            Te = LD(&(x[WS(rs, 7)]), ms, &(x[WS(rs, 1)]));
            Tf = VZMULJ(Td, Te);
            Ti = LD(&(x[WS(rs, 8)]), ms, &(x[0]));
            Tj = VZMULJ(Th, Ti);
          }
          Tg = VSUB(Tc, Tf);
          Tm = VSUB(Tj, Tl);
          Tn = VADD(Tg, Tm);
          TK = VADD(Tc, Tf);
          TL = VADD(Tj, Tl);
          TM = VADD(TK, TL);
        }
        {
          V TC, TA, TB, TG, TI, TE, TF, TH, TD;
          TC = VSUB(Tn, Tz);
          TA = VADD(Tn, Tz);
          TB = VFNMS(LDK(KP250000000), TA, T9);
          TE = VSUB(Tg, Tm);
          TF = VSUB(Ts, Ty);
          TG = VMUL(LDK(KP951056516), VFMA(LDK(KP618033988), TF, TE));
          TI = VMUL(LDK(KP951056516), VFNMS(LDK(KP618033988), TE, TF));
          ST(&(x[WS(rs, 5)]), VADD(T9, TA), ms, &(x[WS(rs, 1)]));
          TH = VFNMS(LDK(KP559016994), TC, TB);
          ST(&(x[WS(rs, 3)]), VFNMSI(TI, TH), ms, &(x[WS(rs, 1)]));
          ST(&(x[WS(rs, 7)]), VFMAI(TI, TH), ms, &(x[WS(rs, 1)]));
          TD = VFMA(LDK(KP559016994), TC, TB);
          ST(&(x[WS(rs, 1)]), VFNMSI(TG, TD), ms, &(x[WS(rs, 1)]));
          ST(&(x[WS(rs, 9)]), VFMAI(TG, TD), ms, &(x[WS(rs, 1)]));
        }
        {
          V TS, TQ, TR, TW, TY, TU, TV, TX, TT;
          TS = VSUB(TM, TP);
          TQ = VADD(TM, TP);
          TR = VFNMS(LDK(KP250000000), TQ, TJ);
          TU = VSUB(TN, TO);
          TV = VSUB(TK, TL);
          TW = VMUL(LDK(KP951056516), VFNMS(LDK(KP618033988), TV, TU));
          TY = VMUL(LDK(KP951056516), VFMA(LDK(KP618033988), TU, TV));
          ST(&(x[0]), VADD(TJ, TQ), ms, &(x[0]));
          TX = VFMA(LDK(KP559016994), TS, TR);
          ST(&(x[WS(rs, 4)]), VFMAI(TY, TX), ms, &(x[0]));
          ST(&(x[WS(rs, 6)]), VFNMSI(TY, TX), ms, &(x[0]));
          TT = VFNMS(LDK(KP559016994), TS, TR);
          ST(&(x[WS(rs, 2)]), VFMAI(TW, TT), ms, &(x[0]));
          ST(&(x[WS(rs, 8)]), VFNMSI(TW, TT), ms, &(x[0]));
        }
      }
    }
  }
  VLEAVE();
}

static const tw_instr twinstr[] = {
    VTW(0, 1), VTW(0, 3), VTW(0, 9), {TW_NEXT, VL, 0}};

static const ct_desc desc = {
    10, XSIMD_STRING("t3fv_10"), twinstr, &GENUS, {39, 34, 18, 0}, 0, 0, 0};

void XSIMD(codelet_t3fv_10)(planner *p) {
  X(kdft_dit_register)(p, t3fv_10, &desc);
}
#else

/* Generated by: ../../../genfft/gen_twiddle_c.native -simd -compact -variables
 * 4 -pipeline-latency 8 -twiddle-log3 -precompute-twiddles -no-generate-bytw -n
 * 10 -name t3fv_10 -include dft/simd/t3f.h */

/*
 * This function contains 57 FP additions, 42 FP multiplications,
 * (or, 51 additions, 36 multiplications, 6 fused multiply/add),
 * 41 stack variables, 4 constants, and 20 memory accesses
 */
#include "dft/simd/t3f.h"

static void t3fv_10(R *ri, R *ii, const R *W, stride rs, INT mb, INT me,
                    INT ms) {
  DVK(KP587785252, +0.587785252292473129168705954639072768597652438);
  DVK(KP951056516, +0.951056516295153572116439333379382143405698634);
  DVK(KP250000000, +0.250000000000000000000000000000000000000000000);
  DVK(KP559016994, +0.559016994374947424102293417182819058860154590);
  {
    INT m;
    R *x;
    x = ri;
    for (m = mb, W = W + (mb * ((TWVL / VL) * 6)); m < me; m = m + VL,
        x = x + (VL * ms), W = W + (TWVL * 6), MAKE_VOLATILE_STRIDE(10, rs)) {
      V T1, T2, T3, Ti, T6, T7, Tx, Tb, To;
      T1 = LDW(&(W[0]));
      T2 = LDW(&(W[TWVL * 2]));
      T3 = VZMULJ(T1, T2);
      Ti = VZMUL(T1, T2);
      T6 = LDW(&(W[TWVL * 4]));
      T7 = VZMULJ(T3, T6);
      Tx = VZMULJ(Ti, T6);
      Tb = VZMULJ(T1, T6);
      To = VZMULJ(T2, T6);
      {
        V TA, TQ, Tn, Tt, Tu, TJ, TK, TS, Ta, Tg, Th, TM, TN, TR, Tw;
        V Tz, Ty;
        Tw = LD(&(x[0]), ms, &(x[0]));
        Ty = LD(&(x[WS(rs, 5)]), ms, &(x[WS(rs, 1)]));
        Tz = VZMULJ(Tx, Ty);
        TA = VSUB(Tw, Tz);
        TQ = VADD(Tw, Tz);
        {
          V Tk, Ts, Tm, Tq;
          {
            V Tj, Tr, Tl, Tp;
            Tj = LD(&(x[WS(rs, 4)]), ms, &(x[0]));
            Tk = VZMULJ(Ti, Tj);
            Tr = LD(&(x[WS(rs, 1)]), ms, &(x[WS(rs, 1)]));
            Ts = VZMULJ(T1, Tr);
            Tl = LD(&(x[WS(rs, 9)]), ms, &(x[WS(rs, 1)]));
            Tm = VZMULJ(T6, Tl);
            Tp = LD(&(x[WS(rs, 6)]), ms, &(x[0]));
            Tq = VZMULJ(To, Tp);
          }
          Tn = VSUB(Tk, Tm);
          Tt = VSUB(Tq, Ts);
          Tu = VADD(Tn, Tt);
          TJ = VADD(Tk, Tm);
          TK = VADD(Tq, Ts);
          TS = VADD(TJ, TK);
        }
        {
          V T5, Tf, T9, Td;
          {
            V T4, Te, T8, Tc;
            T4 = LD(&(x[WS(rs, 2)]), ms, &(x[0]));
            T5 = VZMULJ(T3, T4);
            Te = LD(&(x[WS(rs, 3)]), ms, &(x[WS(rs, 1)]));
            Tf = VZMULJ(T2, Te);
            T8 = LD(&(x[WS(rs, 7)]), ms, &(x[WS(rs, 1)]));
            T9 = VZMULJ(T7, T8);
            Tc = LD(&(x[WS(rs, 8)]), ms, &(x[0]));
            Td = VZMULJ(Tb, Tc);
          }
          Ta = VSUB(T5, T9);
          Tg = VSUB(Td, Tf);
          Th = VADD(Ta, Tg);
          TM = VADD(T5, T9);
          TN = VADD(Td, Tf);
          TR = VADD(TM, TN);
        }
        {
          V Tv, TB, TC, TG, TI, TE, TF, TH, TD;
          Tv = VMUL(LDK(KP559016994), VSUB(Th, Tu));
          TB = VADD(Th, Tu);
          TC = VFNMS(LDK(KP250000000), TB, TA);
          TE = VSUB(Ta, Tg);
          TF = VSUB(Tn, Tt);
          TG = VBYI(VFMA(LDK(KP951056516), TE, VMUL(LDK(KP587785252), TF)));
          TI = VBYI(VFNMS(LDK(KP587785252), TE, VMUL(LDK(KP951056516), TF)));
          ST(&(x[WS(rs, 5)]), VADD(TA, TB), ms, &(x[WS(rs, 1)]));
          TH = VSUB(TC, Tv);
          ST(&(x[WS(rs, 3)]), VSUB(TH, TI), ms, &(x[WS(rs, 1)]));
          ST(&(x[WS(rs, 7)]), VADD(TI, TH), ms, &(x[WS(rs, 1)]));
          TD = VADD(Tv, TC);
          ST(&(x[WS(rs, 1)]), VSUB(TD, TG), ms, &(x[WS(rs, 1)]));
          ST(&(x[WS(rs, 9)]), VADD(TG, TD), ms, &(x[WS(rs, 1)]));
        }
        {
          V TV, TT, TU, TP, TX, TL, TO, TY, TW;
          TV = VMUL(LDK(KP559016994), VSUB(TR, TS));
          TT = VADD(TR, TS);
          TU = VFNMS(LDK(KP250000000), TT, TQ);
          TL = VSUB(TJ, TK);
          TO = VSUB(TM, TN);
          TP = VBYI(VFNMS(LDK(KP587785252), TO, VMUL(LDK(KP951056516), TL)));
          TX = VBYI(VFMA(LDK(KP951056516), TO, VMUL(LDK(KP587785252), TL)));
          ST(&(x[0]), VADD(TQ, TT), ms, &(x[0]));
          TY = VADD(TV, TU);
          ST(&(x[WS(rs, 4)]), VADD(TX, TY), ms, &(x[0]));
          ST(&(x[WS(rs, 6)]), VSUB(TY, TX), ms, &(x[0]));
          TW = VSUB(TU, TV);
          ST(&(x[WS(rs, 2)]), VADD(TP, TW), ms, &(x[0]));
          ST(&(x[WS(rs, 8)]), VSUB(TW, TP), ms, &(x[0]));
        }
      }
    }
  }
  VLEAVE();
}

static const tw_instr twinstr[] = {
    VTW(0, 1), VTW(0, 3), VTW(0, 9), {TW_NEXT, VL, 0}};

static const ct_desc desc = {
    10, XSIMD_STRING("t3fv_10"), twinstr, &GENUS, {51, 36, 6, 0}, 0, 0, 0};

void XSIMD(codelet_t3fv_10)(planner *p) {
  X(kdft_dit_register)(p, t3fv_10, &desc);
}
#endif
