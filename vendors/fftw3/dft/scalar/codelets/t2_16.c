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
/* Generated on Tue Sep 14 10:44:32 EDT 2021 */

#include "dft/codelet-dft.h"

#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)

/* Generated by: ../../../genfft/gen_twiddle.native -fma -compact -variables 4
 * -pipeline-latency 4 -twiddle-log3 -precompute-twiddles -n 16 -name t2_16
 * -include dft/scalar/t.h */

/*
 * This function contains 196 FP additions, 134 FP multiplications,
 * (or, 104 additions, 42 multiplications, 92 fused multiply/add),
 * 90 stack variables, 3 constants, and 64 memory accesses
 */
#include "dft/scalar/t.h"

static void t2_16(R *ri, R *ii, const R *W, stride rs, INT mb, INT me, INT ms) {
  DK(KP923879532, +0.923879532511286756128183189396788286822416626);
  DK(KP414213562, +0.414213562373095048801688724209698078569671875);
  DK(KP707106781, +0.707106781186547524400844362104849039284835938);
  {
    INT m;
    for (m = mb, W = W + (mb * 8); m < me; m = m + 1, ri = ri + ms,
        ii = ii + ms, W = W + 8, MAKE_VOLATILE_STRIDE(32, rs)) {
      E T2, Tf, TM, TO, T3, T6, T5, Th, Tz, Ti, T7, TZ, TT, Tq, TW;
      E Tb, Tu, TP, TI, TF, TC, T1z, T1O, T1D, T1L, Tm, T1f, T1p, T1j, T1m;
      {
        E TN, TS, T4, Tp, Ta, Tt, Tl, Tg;
        T2 = W[0];
        Tf = W[2];
        Tg = T2 * Tf;
        TM = W[6];
        TN = T2 * TM;
        TO = W[7];
        TS = T2 * TO;
        T3 = W[4];
        T4 = T2 * T3;
        Tp = Tf * T3;
        T6 = W[5];
        Ta = T2 * T6;
        Tt = Tf * T6;
        T5 = W[1];
        Th = W[3];
        Tl = T2 * Th;
        Tz = FMA(T5, Th, Tg);
        Ti = FNMS(T5, Th, Tg);
        T7 = FMA(T5, T6, T4);
        TZ = FNMS(Th, T3, Tt);
        TT = FNMS(T5, TM, TS);
        Tq = FNMS(Th, T6, Tp);
        TW = FMA(Th, T6, Tp);
        Tb = FNMS(T5, T3, Ta);
        Tu = FMA(Th, T3, Tt);
        TP = FMA(T5, TO, TN);
        TI = FMA(T5, T3, Ta);
        TF = FNMS(T5, T6, T4);
        {
          E T1y, T1C, T1e, T1i;
          T1y = Tz * T3;
          T1C = Tz * T6;
          TC = FNMS(T5, Tf, Tl);
          T1z = FMA(TC, T6, T1y);
          T1O = FMA(TC, T3, T1C);
          T1D = FNMS(TC, T3, T1C);
          T1L = FNMS(TC, T6, T1y);
          T1e = Ti * T3;
          T1i = Ti * T6;
          Tm = FMA(T5, Tf, Tl);
          T1f = FMA(Tm, T6, T1e);
          T1p = FMA(Tm, T3, T1i);
          T1j = FNMS(Tm, T3, T1i);
          T1m = FNMS(Tm, T6, T1e);
        }
      }
      {
        E Te, T1U, T3A, T3L, T1G, T2D, T2A, T3h, T1R, T2B, T2I, T3i, Tx, T3M,
            T1Z;
        E T3w, TL, T26, T25, T37, T1d, T2o, T2l, T3c, T1s, T2m, T2t, T3d, T12,
            T28;
        E T2d, T38;
        {
          E T1, T3z, T8, T9, Tc, T3x, Td, T3y;
          T1 = ri[0];
          T3z = ii[0];
          T8 = ri[WS(rs, 8)];
          T9 = T7 * T8;
          Tc = ii[WS(rs, 8)];
          T3x = T7 * Tc;
          Td = FMA(Tb, Tc, T9);
          Te = T1 + Td;
          T1U = T1 - Td;
          T3y = FNMS(Tb, T8, T3x);
          T3A = T3y + T3z;
          T3L = T3z - T3y;
        }
        {
          E T1u, T1v, T1w, T2w, T1A, T1B, T1E, T2y;
          T1u = ri[WS(rs, 15)];
          T1v = TM * T1u;
          T1w = ii[WS(rs, 15)];
          T2w = TM * T1w;
          T1A = ri[WS(rs, 7)];
          T1B = T1z * T1A;
          T1E = ii[WS(rs, 7)];
          T2y = T1z * T1E;
          {
            E T1x, T1F, T2x, T2z;
            T1x = FMA(TO, T1w, T1v);
            T1F = FMA(T1D, T1E, T1B);
            T1G = T1x + T1F;
            T2D = T1x - T1F;
            T2x = FNMS(TO, T1u, T2w);
            T2z = FNMS(T1D, T1A, T2y);
            T2A = T2x - T2z;
            T3h = T2x + T2z;
          }
        }
        {
          E T1H, T1I, T1J, T2E, T1M, T1N, T1P, T2G;
          T1H = ri[WS(rs, 3)];
          T1I = Tf * T1H;
          T1J = ii[WS(rs, 3)];
          T2E = Tf * T1J;
          T1M = ri[WS(rs, 11)];
          T1N = T1L * T1M;
          T1P = ii[WS(rs, 11)];
          T2G = T1L * T1P;
          {
            E T1K, T1Q, T2F, T2H;
            T1K = FMA(Th, T1J, T1I);
            T1Q = FMA(T1O, T1P, T1N);
            T1R = T1K + T1Q;
            T2B = T1K - T1Q;
            T2F = FNMS(Th, T1H, T2E);
            T2H = FNMS(T1O, T1M, T2G);
            T2I = T2F - T2H;
            T3i = T2F + T2H;
          }
        }
        {
          E Tj, Tk, Tn, T1V, Tr, Ts, Tv, T1X;
          Tj = ri[WS(rs, 4)];
          Tk = Ti * Tj;
          Tn = ii[WS(rs, 4)];
          T1V = Ti * Tn;
          Tr = ri[WS(rs, 12)];
          Ts = Tq * Tr;
          Tv = ii[WS(rs, 12)];
          T1X = Tq * Tv;
          {
            E To, Tw, T1W, T1Y;
            To = FMA(Tm, Tn, Tk);
            Tw = FMA(Tu, Tv, Ts);
            Tx = To + Tw;
            T3M = To - Tw;
            T1W = FNMS(Tm, Tj, T1V);
            T1Y = FNMS(Tu, Tr, T1X);
            T1Z = T1W - T1Y;
            T3w = T1W + T1Y;
          }
        }
        {
          E TA, TB, TD, T21, TG, TH, TJ, T23;
          TA = ri[WS(rs, 2)];
          TB = Tz * TA;
          TD = ii[WS(rs, 2)];
          T21 = Tz * TD;
          TG = ri[WS(rs, 10)];
          TH = TF * TG;
          TJ = ii[WS(rs, 10)];
          T23 = TF * TJ;
          {
            E TE, TK, T22, T24;
            TE = FMA(TC, TD, TB);
            TK = FMA(TI, TJ, TH);
            TL = TE + TK;
            T26 = TE - TK;
            T22 = FNMS(TC, TA, T21);
            T24 = FNMS(TI, TG, T23);
            T25 = T22 - T24;
            T37 = T22 + T24;
          }
        }
        {
          E T15, T16, T17, T2h, T19, T1a, T1b, T2j;
          T15 = ri[WS(rs, 1)];
          T16 = T2 * T15;
          T17 = ii[WS(rs, 1)];
          T2h = T2 * T17;
          T19 = ri[WS(rs, 9)];
          T1a = T3 * T19;
          T1b = ii[WS(rs, 9)];
          T2j = T3 * T1b;
          {
            E T18, T1c, T2i, T2k;
            T18 = FMA(T5, T17, T16);
            T1c = FMA(T6, T1b, T1a);
            T1d = T18 + T1c;
            T2o = T18 - T1c;
            T2i = FNMS(T5, T15, T2h);
            T2k = FNMS(T6, T19, T2j);
            T2l = T2i - T2k;
            T3c = T2i + T2k;
          }
        }
        {
          E T1g, T1h, T1k, T2p, T1n, T1o, T1q, T2r;
          T1g = ri[WS(rs, 5)];
          T1h = T1f * T1g;
          T1k = ii[WS(rs, 5)];
          T2p = T1f * T1k;
          T1n = ri[WS(rs, 13)];
          T1o = T1m * T1n;
          T1q = ii[WS(rs, 13)];
          T2r = T1m * T1q;
          {
            E T1l, T1r, T2q, T2s;
            T1l = FMA(T1j, T1k, T1h);
            T1r = FMA(T1p, T1q, T1o);
            T1s = T1l + T1r;
            T2m = T1l - T1r;
            T2q = FNMS(T1j, T1g, T2p);
            T2s = FNMS(T1p, T1n, T2r);
            T2t = T2q - T2s;
            T3d = T2q + T2s;
          }
        }
        {
          E TQ, TR, TU, T29, TX, TY, T10, T2b;
          TQ = ri[WS(rs, 14)];
          TR = TP * TQ;
          TU = ii[WS(rs, 14)];
          T29 = TP * TU;
          TX = ri[WS(rs, 6)];
          TY = TW * TX;
          T10 = ii[WS(rs, 6)];
          T2b = TW * T10;
          {
            E TV, T11, T2a, T2c;
            TV = FMA(TT, TU, TR);
            T11 = FMA(TZ, T10, TY);
            T12 = TV + T11;
            T28 = TV - T11;
            T2a = FNMS(TT, TQ, T29);
            T2c = FNMS(TZ, TX, T2b);
            T2d = T2a - T2c;
            T38 = T2a + T2c;
          }
        }
        {
          E T14, T3q, T3C, T3E, T1T, T3D, T3t, T3u;
          {
            E Ty, T13, T3v, T3B;
            Ty = Te + Tx;
            T13 = TL + T12;
            T14 = Ty + T13;
            T3q = Ty - T13;
            T3v = T37 + T38;
            T3B = T3w + T3A;
            T3C = T3v + T3B;
            T3E = T3B - T3v;
          }
          {
            E T1t, T1S, T3r, T3s;
            T1t = T1d + T1s;
            T1S = T1G + T1R;
            T1T = T1t + T1S;
            T3D = T1S - T1t;
            T3r = T3c + T3d;
            T3s = T3h + T3i;
            T3t = T3r - T3s;
            T3u = T3r + T3s;
          }
          ri[WS(rs, 8)] = T14 - T1T;
          ii[WS(rs, 8)] = T3C - T3u;
          ri[0] = T14 + T1T;
          ii[0] = T3u + T3C;
          ri[WS(rs, 12)] = T3q - T3t;
          ii[WS(rs, 12)] = T3E - T3D;
          ri[WS(rs, 4)] = T3q + T3t;
          ii[WS(rs, 4)] = T3D + T3E;
        }
        {
          E T3a, T3m, T3H, T3J, T3f, T3n, T3k, T3o;
          {
            E T36, T39, T3F, T3G;
            T36 = Te - Tx;
            T39 = T37 - T38;
            T3a = T36 + T39;
            T3m = T36 - T39;
            T3F = T12 - TL;
            T3G = T3A - T3w;
            T3H = T3F + T3G;
            T3J = T3G - T3F;
          }
          {
            E T3b, T3e, T3g, T3j;
            T3b = T1d - T1s;
            T3e = T3c - T3d;
            T3f = T3b + T3e;
            T3n = T3e - T3b;
            T3g = T1G - T1R;
            T3j = T3h - T3i;
            T3k = T3g - T3j;
            T3o = T3g + T3j;
          }
          {
            E T3l, T3I, T3p, T3K;
            T3l = T3f + T3k;
            ri[WS(rs, 10)] = FNMS(KP707106781, T3l, T3a);
            ri[WS(rs, 2)] = FMA(KP707106781, T3l, T3a);
            T3I = T3n + T3o;
            ii[WS(rs, 2)] = FMA(KP707106781, T3I, T3H);
            ii[WS(rs, 10)] = FNMS(KP707106781, T3I, T3H);
            T3p = T3n - T3o;
            ri[WS(rs, 14)] = FNMS(KP707106781, T3p, T3m);
            ri[WS(rs, 6)] = FMA(KP707106781, T3p, T3m);
            T3K = T3k - T3f;
            ii[WS(rs, 6)] = FMA(KP707106781, T3K, T3J);
            ii[WS(rs, 14)] = FNMS(KP707106781, T3K, T3J);
          }
        }
        {
          E T20, T3N, T3T, T2Q, T2f, T3O, T30, T34, T2T, T3U, T2v, T2N, T2X,
              T33, T2K;
          E T2O;
          {
            E T27, T2e, T2n, T2u;
            T20 = T1U - T1Z;
            T3N = T3L - T3M;
            T3T = T3M + T3L;
            T2Q = T1U + T1Z;
            T27 = T25 - T26;
            T2e = T28 + T2d;
            T2f = T27 - T2e;
            T3O = T27 + T2e;
            {
              E T2Y, T2Z, T2R, T2S;
              T2Y = T2D + T2I;
              T2Z = T2A - T2B;
              T30 = FNMS(KP414213562, T2Z, T2Y);
              T34 = FMA(KP414213562, T2Y, T2Z);
              T2R = T26 + T25;
              T2S = T28 - T2d;
              T2T = T2R + T2S;
              T3U = T2S - T2R;
            }
            T2n = T2l + T2m;
            T2u = T2o - T2t;
            T2v = FMA(KP414213562, T2u, T2n);
            T2N = FNMS(KP414213562, T2n, T2u);
            {
              E T2V, T2W, T2C, T2J;
              T2V = T2o + T2t;
              T2W = T2l - T2m;
              T2X = FMA(KP414213562, T2W, T2V);
              T33 = FNMS(KP414213562, T2V, T2W);
              T2C = T2A + T2B;
              T2J = T2D - T2I;
              T2K = FNMS(KP414213562, T2J, T2C);
              T2O = FMA(KP414213562, T2C, T2J);
            }
          }
          {
            E T2g, T2L, T3V, T3W;
            T2g = FMA(KP707106781, T2f, T20);
            T2L = T2v - T2K;
            ri[WS(rs, 11)] = FNMS(KP923879532, T2L, T2g);
            ri[WS(rs, 3)] = FMA(KP923879532, T2L, T2g);
            T3V = FMA(KP707106781, T3U, T3T);
            T3W = T2O - T2N;
            ii[WS(rs, 3)] = FMA(KP923879532, T3W, T3V);
            ii[WS(rs, 11)] = FNMS(KP923879532, T3W, T3V);
          }
          {
            E T2M, T2P, T3X, T3Y;
            T2M = FNMS(KP707106781, T2f, T20);
            T2P = T2N + T2O;
            ri[WS(rs, 7)] = FNMS(KP923879532, T2P, T2M);
            ri[WS(rs, 15)] = FMA(KP923879532, T2P, T2M);
            T3X = FNMS(KP707106781, T3U, T3T);
            T3Y = T2v + T2K;
            ii[WS(rs, 7)] = FNMS(KP923879532, T3Y, T3X);
            ii[WS(rs, 15)] = FMA(KP923879532, T3Y, T3X);
          }
          {
            E T2U, T31, T3P, T3Q;
            T2U = FMA(KP707106781, T2T, T2Q);
            T31 = T2X + T30;
            ri[WS(rs, 9)] = FNMS(KP923879532, T31, T2U);
            ri[WS(rs, 1)] = FMA(KP923879532, T31, T2U);
            T3P = FMA(KP707106781, T3O, T3N);
            T3Q = T33 + T34;
            ii[WS(rs, 1)] = FMA(KP923879532, T3Q, T3P);
            ii[WS(rs, 9)] = FNMS(KP923879532, T3Q, T3P);
          }
          {
            E T32, T35, T3R, T3S;
            T32 = FNMS(KP707106781, T2T, T2Q);
            T35 = T33 - T34;
            ri[WS(rs, 13)] = FNMS(KP923879532, T35, T32);
            ri[WS(rs, 5)] = FMA(KP923879532, T35, T32);
            T3R = FNMS(KP707106781, T3O, T3N);
            T3S = T30 - T2X;
            ii[WS(rs, 5)] = FMA(KP923879532, T3S, T3R);
            ii[WS(rs, 13)] = FNMS(KP923879532, T3S, T3R);
          }
        }
      }
    }
  }
}

static const tw_instr twinstr[] = {{TW_CEXP, 0, 1},
                                   {TW_CEXP, 0, 3},
                                   {TW_CEXP, 0, 9},
                                   {TW_CEXP, 0, 15},
                                   {TW_NEXT, 1, 0}};

static const ct_desc desc = {16, "t2_16", twinstr, &GENUS, {104, 42, 92, 0},
                             0,  0,       0};

void X(codelet_t2_16)(planner *p) { X(kdft_dit_register)(p, t2_16, &desc); }
#else

/* Generated by: ../../../genfft/gen_twiddle.native -compact -variables 4
 * -pipeline-latency 4 -twiddle-log3 -precompute-twiddles -n 16 -name t2_16
 * -include dft/scalar/t.h */

/*
 * This function contains 196 FP additions, 108 FP multiplications,
 * (or, 156 additions, 68 multiplications, 40 fused multiply/add),
 * 82 stack variables, 3 constants, and 64 memory accesses
 */
#include "dft/scalar/t.h"

static void t2_16(R *ri, R *ii, const R *W, stride rs, INT mb, INT me, INT ms) {
  DK(KP382683432, +0.382683432365089771728459984030398866761344562);
  DK(KP923879532, +0.923879532511286756128183189396788286822416626);
  DK(KP707106781, +0.707106781186547524400844362104849039284835938);
  {
    INT m;
    for (m = mb, W = W + (mb * 8); m < me; m = m + 1, ri = ri + ms,
        ii = ii + ms, W = W + 8, MAKE_VOLATILE_STRIDE(32, rs)) {
      E T2, T5, Tg, Ti, Tk, To, TE, TC, T6, T3, T8, TW, TJ, Tt, TU;
      E Tc, Tx, TH, TN, TO, TP, TR, T1f, T1k, T1b, T1i, T1y, T1H, T1u, T1F;
      {
        E T7, Tv, Ta, Ts, T4, Tw, Tb, Tr;
        {
          E Th, Tn, Tj, Tm;
          T2 = W[0];
          T5 = W[1];
          Tg = W[2];
          Ti = W[3];
          Th = T2 * Tg;
          Tn = T5 * Tg;
          Tj = T5 * Ti;
          Tm = T2 * Ti;
          Tk = Th - Tj;
          To = Tm + Tn;
          TE = Tm - Tn;
          TC = Th + Tj;
          T6 = W[5];
          T7 = T5 * T6;
          Tv = Tg * T6;
          Ta = T2 * T6;
          Ts = Ti * T6;
          T3 = W[4];
          T4 = T2 * T3;
          Tw = Ti * T3;
          Tb = T5 * T3;
          Tr = Tg * T3;
        }
        T8 = T4 + T7;
        TW = Tv - Tw;
        TJ = Ta + Tb;
        Tt = Tr - Ts;
        TU = Tr + Ts;
        Tc = Ta - Tb;
        Tx = Tv + Tw;
        TH = T4 - T7;
        TN = W[6];
        TO = W[7];
        TP = FMA(T2, TN, T5 * TO);
        TR = FNMS(T5, TN, T2 * TO);
        {
          E T1d, T1e, T19, T1a;
          T1d = Tk * T6;
          T1e = To * T3;
          T1f = T1d - T1e;
          T1k = T1d + T1e;
          T19 = Tk * T3;
          T1a = To * T6;
          T1b = T19 + T1a;
          T1i = T19 - T1a;
        }
        {
          E T1w, T1x, T1s, T1t;
          T1w = TC * T6;
          T1x = TE * T3;
          T1y = T1w - T1x;
          T1H = T1w + T1x;
          T1s = TC * T3;
          T1t = TE * T6;
          T1u = T1s + T1t;
          T1F = T1s - T1t;
        }
      }
      {
        E Tf, T3r, T1N, T3e, TA, T3s, T1Q, T3b, TM, T2M, T1W, T2w, TZ, T2N, T21;
        E T2x, T1B, T1K, T2V, T2W, T2X, T2Y, T2j, T2D, T2o, T2E, T18, T1n, T2Q,
            T2R;
        E T2S, T2T, T28, T2A, T2d, T2B;
        {
          E T1, T3d, Te, T3c, T9, Td;
          T1 = ri[0];
          T3d = ii[0];
          T9 = ri[WS(rs, 8)];
          Td = ii[WS(rs, 8)];
          Te = FMA(T8, T9, Tc * Td);
          T3c = FNMS(Tc, T9, T8 * Td);
          Tf = T1 + Te;
          T3r = T3d - T3c;
          T1N = T1 - Te;
          T3e = T3c + T3d;
        }
        {
          E Tq, T1O, Tz, T1P;
          {
            E Tl, Tp, Tu, Ty;
            Tl = ri[WS(rs, 4)];
            Tp = ii[WS(rs, 4)];
            Tq = FMA(Tk, Tl, To * Tp);
            T1O = FNMS(To, Tl, Tk * Tp);
            Tu = ri[WS(rs, 12)];
            Ty = ii[WS(rs, 12)];
            Tz = FMA(Tt, Tu, Tx * Ty);
            T1P = FNMS(Tx, Tu, Tt * Ty);
          }
          TA = Tq + Tz;
          T3s = Tq - Tz;
          T1Q = T1O - T1P;
          T3b = T1O + T1P;
        }
        {
          E TG, T1S, TL, T1T, T1U, T1V;
          {
            E TD, TF, TI, TK;
            TD = ri[WS(rs, 2)];
            TF = ii[WS(rs, 2)];
            TG = FMA(TC, TD, TE * TF);
            T1S = FNMS(TE, TD, TC * TF);
            TI = ri[WS(rs, 10)];
            TK = ii[WS(rs, 10)];
            TL = FMA(TH, TI, TJ * TK);
            T1T = FNMS(TJ, TI, TH * TK);
          }
          TM = TG + TL;
          T2M = T1S + T1T;
          T1U = T1S - T1T;
          T1V = TG - TL;
          T1W = T1U - T1V;
          T2w = T1V + T1U;
        }
        {
          E TT, T1Y, TY, T1Z, T1X, T20;
          {
            E TQ, TS, TV, TX;
            TQ = ri[WS(rs, 14)];
            TS = ii[WS(rs, 14)];
            TT = FMA(TP, TQ, TR * TS);
            T1Y = FNMS(TR, TQ, TP * TS);
            TV = ri[WS(rs, 6)];
            TX = ii[WS(rs, 6)];
            TY = FMA(TU, TV, TW * TX);
            T1Z = FNMS(TW, TV, TU * TX);
          }
          TZ = TT + TY;
          T2N = T1Y + T1Z;
          T1X = TT - TY;
          T20 = T1Y - T1Z;
          T21 = T1X + T20;
          T2x = T1X - T20;
        }
        {
          E T1r, T2k, T1J, T2h, T1A, T2l, T1E, T2g;
          {
            E T1p, T1q, T1G, T1I;
            T1p = ri[WS(rs, 15)];
            T1q = ii[WS(rs, 15)];
            T1r = FMA(TN, T1p, TO * T1q);
            T2k = FNMS(TO, T1p, TN * T1q);
            T1G = ri[WS(rs, 11)];
            T1I = ii[WS(rs, 11)];
            T1J = FMA(T1F, T1G, T1H * T1I);
            T2h = FNMS(T1H, T1G, T1F * T1I);
          }
          {
            E T1v, T1z, T1C, T1D;
            T1v = ri[WS(rs, 7)];
            T1z = ii[WS(rs, 7)];
            T1A = FMA(T1u, T1v, T1y * T1z);
            T2l = FNMS(T1y, T1v, T1u * T1z);
            T1C = ri[WS(rs, 3)];
            T1D = ii[WS(rs, 3)];
            T1E = FMA(Tg, T1C, Ti * T1D);
            T2g = FNMS(Ti, T1C, Tg * T1D);
          }
          T1B = T1r + T1A;
          T1K = T1E + T1J;
          T2V = T1B - T1K;
          T2W = T2k + T2l;
          T2X = T2g + T2h;
          T2Y = T2W - T2X;
          {
            E T2f, T2i, T2m, T2n;
            T2f = T1r - T1A;
            T2i = T2g - T2h;
            T2j = T2f - T2i;
            T2D = T2f + T2i;
            T2m = T2k - T2l;
            T2n = T1E - T1J;
            T2o = T2m + T2n;
            T2E = T2m - T2n;
          }
        }
        {
          E T14, T24, T1m, T2b, T17, T25, T1h, T2a;
          {
            E T12, T13, T1j, T1l;
            T12 = ri[WS(rs, 1)];
            T13 = ii[WS(rs, 1)];
            T14 = FMA(T2, T12, T5 * T13);
            T24 = FNMS(T5, T12, T2 * T13);
            T1j = ri[WS(rs, 13)];
            T1l = ii[WS(rs, 13)];
            T1m = FMA(T1i, T1j, T1k * T1l);
            T2b = FNMS(T1k, T1j, T1i * T1l);
          }
          {
            E T15, T16, T1c, T1g;
            T15 = ri[WS(rs, 9)];
            T16 = ii[WS(rs, 9)];
            T17 = FMA(T3, T15, T6 * T16);
            T25 = FNMS(T6, T15, T3 * T16);
            T1c = ri[WS(rs, 5)];
            T1g = ii[WS(rs, 5)];
            T1h = FMA(T1b, T1c, T1f * T1g);
            T2a = FNMS(T1f, T1c, T1b * T1g);
          }
          T18 = T14 + T17;
          T1n = T1h + T1m;
          T2Q = T18 - T1n;
          T2R = T24 + T25;
          T2S = T2a + T2b;
          T2T = T2R - T2S;
          {
            E T26, T27, T29, T2c;
            T26 = T24 - T25;
            T27 = T1h - T1m;
            T28 = T26 + T27;
            T2A = T26 - T27;
            T29 = T14 - T17;
            T2c = T2a - T2b;
            T2d = T29 - T2c;
            T2B = T29 + T2c;
          }
        }
        {
          E T23, T2r, T3A, T3C, T2q, T3B, T2u, T3x;
          {
            E T1R, T22, T3y, T3z;
            T1R = T1N - T1Q;
            T22 = KP707106781 * (T1W - T21);
            T23 = T1R + T22;
            T2r = T1R - T22;
            T3y = KP707106781 * (T2x - T2w);
            T3z = T3s + T3r;
            T3A = T3y + T3z;
            T3C = T3z - T3y;
          }
          {
            E T2e, T2p, T2s, T2t;
            T2e = FMA(KP923879532, T28, KP382683432 * T2d);
            T2p = FNMS(KP923879532, T2o, KP382683432 * T2j);
            T2q = T2e + T2p;
            T3B = T2p - T2e;
            T2s = FNMS(KP923879532, T2d, KP382683432 * T28);
            T2t = FMA(KP382683432, T2o, KP923879532 * T2j);
            T2u = T2s - T2t;
            T3x = T2s + T2t;
          }
          ri[WS(rs, 11)] = T23 - T2q;
          ii[WS(rs, 11)] = T3A - T3x;
          ri[WS(rs, 3)] = T23 + T2q;
          ii[WS(rs, 3)] = T3x + T3A;
          ri[WS(rs, 15)] = T2r - T2u;
          ii[WS(rs, 15)] = T3C - T3B;
          ri[WS(rs, 7)] = T2r + T2u;
          ii[WS(rs, 7)] = T3B + T3C;
        }
        {
          E T2P, T31, T3m, T3o, T30, T3n, T34, T3j;
          {
            E T2L, T2O, T3k, T3l;
            T2L = Tf - TA;
            T2O = T2M - T2N;
            T2P = T2L + T2O;
            T31 = T2L - T2O;
            T3k = TZ - TM;
            T3l = T3e - T3b;
            T3m = T3k + T3l;
            T3o = T3l - T3k;
          }
          {
            E T2U, T2Z, T32, T33;
            T2U = T2Q + T2T;
            T2Z = T2V - T2Y;
            T30 = KP707106781 * (T2U + T2Z);
            T3n = KP707106781 * (T2Z - T2U);
            T32 = T2T - T2Q;
            T33 = T2V + T2Y;
            T34 = KP707106781 * (T32 - T33);
            T3j = KP707106781 * (T32 + T33);
          }
          ri[WS(rs, 10)] = T2P - T30;
          ii[WS(rs, 10)] = T3m - T3j;
          ri[WS(rs, 2)] = T2P + T30;
          ii[WS(rs, 2)] = T3j + T3m;
          ri[WS(rs, 14)] = T31 - T34;
          ii[WS(rs, 14)] = T3o - T3n;
          ri[WS(rs, 6)] = T31 + T34;
          ii[WS(rs, 6)] = T3n + T3o;
        }
        {
          E T2z, T2H, T3u, T3w, T2G, T3v, T2K, T3p;
          {
            E T2v, T2y, T3q, T3t;
            T2v = T1N + T1Q;
            T2y = KP707106781 * (T2w + T2x);
            T2z = T2v + T2y;
            T2H = T2v - T2y;
            T3q = KP707106781 * (T1W + T21);
            T3t = T3r - T3s;
            T3u = T3q + T3t;
            T3w = T3t - T3q;
          }
          {
            E T2C, T2F, T2I, T2J;
            T2C = FMA(KP382683432, T2A, KP923879532 * T2B);
            T2F = FNMS(KP382683432, T2E, KP923879532 * T2D);
            T2G = T2C + T2F;
            T3v = T2F - T2C;
            T2I = FNMS(KP382683432, T2B, KP923879532 * T2A);
            T2J = FMA(KP923879532, T2E, KP382683432 * T2D);
            T2K = T2I - T2J;
            T3p = T2I + T2J;
          }
          ri[WS(rs, 9)] = T2z - T2G;
          ii[WS(rs, 9)] = T3u - T3p;
          ri[WS(rs, 1)] = T2z + T2G;
          ii[WS(rs, 1)] = T3p + T3u;
          ri[WS(rs, 13)] = T2H - T2K;
          ii[WS(rs, 13)] = T3w - T3v;
          ri[WS(rs, 5)] = T2H + T2K;
          ii[WS(rs, 5)] = T3v + T3w;
        }
        {
          E T11, T35, T3g, T3i, T1M, T3h, T38, T39;
          {
            E TB, T10, T3a, T3f;
            TB = Tf + TA;
            T10 = TM + TZ;
            T11 = TB + T10;
            T35 = TB - T10;
            T3a = T2M + T2N;
            T3f = T3b + T3e;
            T3g = T3a + T3f;
            T3i = T3f - T3a;
          }
          {
            E T1o, T1L, T36, T37;
            T1o = T18 + T1n;
            T1L = T1B + T1K;
            T1M = T1o + T1L;
            T3h = T1L - T1o;
            T36 = T2R + T2S;
            T37 = T2W + T2X;
            T38 = T36 - T37;
            T39 = T36 + T37;
          }
          ri[WS(rs, 8)] = T11 - T1M;
          ii[WS(rs, 8)] = T3g - T39;
          ri[0] = T11 + T1M;
          ii[0] = T39 + T3g;
          ri[WS(rs, 12)] = T35 - T38;
          ii[WS(rs, 12)] = T3i - T3h;
          ri[WS(rs, 4)] = T35 + T38;
          ii[WS(rs, 4)] = T3h + T3i;
        }
      }
    }
  }
}

static const tw_instr twinstr[] = {{TW_CEXP, 0, 1},
                                   {TW_CEXP, 0, 3},
                                   {TW_CEXP, 0, 9},
                                   {TW_CEXP, 0, 15},
                                   {TW_NEXT, 1, 0}};

static const ct_desc desc = {16, "t2_16", twinstr, &GENUS, {156, 68, 40, 0},
                             0,  0,       0};

void X(codelet_t2_16)(planner *p) { X(kdft_dit_register)(p, t2_16, &desc); }
#endif
