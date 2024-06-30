#include "kernel/ifftw.h"
#include SIMD_HEADER

extern void XSIMD(codelet_n1fv_2)(planner *);
extern void XSIMD(codelet_n1fv_3)(planner *);
extern void XSIMD(codelet_n1fv_4)(planner *);
extern void XSIMD(codelet_n1fv_5)(planner *);
extern void XSIMD(codelet_n1fv_6)(planner *);
extern void XSIMD(codelet_n1fv_7)(planner *);
extern void XSIMD(codelet_n1fv_8)(planner *);
extern void XSIMD(codelet_n1fv_9)(planner *);
extern void XSIMD(codelet_n1fv_10)(planner *);
extern void XSIMD(codelet_n1fv_11)(planner *);
extern void XSIMD(codelet_n1fv_12)(planner *);
extern void XSIMD(codelet_n1fv_13)(planner *);
extern void XSIMD(codelet_n1fv_14)(planner *);
extern void XSIMD(codelet_n1fv_15)(planner *);
extern void XSIMD(codelet_n1fv_16)(planner *);
extern void XSIMD(codelet_n1fv_32)(planner *);
extern void XSIMD(codelet_n1fv_64)(planner *);
extern void XSIMD(codelet_n1fv_128)(planner *);
extern void XSIMD(codelet_n1fv_20)(planner *);
extern void XSIMD(codelet_n1fv_25)(planner *);
extern void XSIMD(codelet_n1bv_2)(planner *);
extern void XSIMD(codelet_n1bv_3)(planner *);
extern void XSIMD(codelet_n1bv_4)(planner *);
extern void XSIMD(codelet_n1bv_5)(planner *);
extern void XSIMD(codelet_n1bv_6)(planner *);
extern void XSIMD(codelet_n1bv_7)(planner *);
extern void XSIMD(codelet_n1bv_8)(planner *);
extern void XSIMD(codelet_n1bv_9)(planner *);
extern void XSIMD(codelet_n1bv_10)(planner *);
extern void XSIMD(codelet_n1bv_11)(planner *);
extern void XSIMD(codelet_n1bv_12)(planner *);
extern void XSIMD(codelet_n1bv_13)(planner *);
extern void XSIMD(codelet_n1bv_14)(planner *);
extern void XSIMD(codelet_n1bv_15)(planner *);
extern void XSIMD(codelet_n1bv_16)(planner *);
extern void XSIMD(codelet_n1bv_32)(planner *);
extern void XSIMD(codelet_n1bv_64)(planner *);
extern void XSIMD(codelet_n1bv_128)(planner *);
extern void XSIMD(codelet_n1bv_20)(planner *);
extern void XSIMD(codelet_n1bv_25)(planner *);
extern void XSIMD(codelet_n2fv_2)(planner *);
extern void XSIMD(codelet_n2fv_4)(planner *);
extern void XSIMD(codelet_n2fv_6)(planner *);
extern void XSIMD(codelet_n2fv_8)(planner *);
extern void XSIMD(codelet_n2fv_10)(planner *);
extern void XSIMD(codelet_n2fv_12)(planner *);
extern void XSIMD(codelet_n2fv_14)(planner *);
extern void XSIMD(codelet_n2fv_16)(planner *);
extern void XSIMD(codelet_n2fv_32)(planner *);
extern void XSIMD(codelet_n2fv_64)(planner *);
extern void XSIMD(codelet_n2fv_20)(planner *);
extern void XSIMD(codelet_n2bv_2)(planner *);
extern void XSIMD(codelet_n2bv_4)(planner *);
extern void XSIMD(codelet_n2bv_6)(planner *);
extern void XSIMD(codelet_n2bv_8)(planner *);
extern void XSIMD(codelet_n2bv_10)(planner *);
extern void XSIMD(codelet_n2bv_12)(planner *);
extern void XSIMD(codelet_n2bv_14)(planner *);
extern void XSIMD(codelet_n2bv_16)(planner *);
extern void XSIMD(codelet_n2bv_32)(planner *);
extern void XSIMD(codelet_n2bv_64)(planner *);
extern void XSIMD(codelet_n2bv_20)(planner *);
extern void XSIMD(codelet_n2sv_4)(planner *);
extern void XSIMD(codelet_n2sv_8)(planner *);
extern void XSIMD(codelet_n2sv_16)(planner *);
extern void XSIMD(codelet_n2sv_32)(planner *);
extern void XSIMD(codelet_n2sv_64)(planner *);
extern void XSIMD(codelet_t1fuv_2)(planner *);
extern void XSIMD(codelet_t1fuv_3)(planner *);
extern void XSIMD(codelet_t1fuv_4)(planner *);
extern void XSIMD(codelet_t1fuv_5)(planner *);
extern void XSIMD(codelet_t1fuv_6)(planner *);
extern void XSIMD(codelet_t1fuv_7)(planner *);
extern void XSIMD(codelet_t1fuv_8)(planner *);
extern void XSIMD(codelet_t1fuv_9)(planner *);
extern void XSIMD(codelet_t1fuv_10)(planner *);
extern void XSIMD(codelet_t1fv_2)(planner *);
extern void XSIMD(codelet_t1fv_3)(planner *);
extern void XSIMD(codelet_t1fv_4)(planner *);
extern void XSIMD(codelet_t1fv_5)(planner *);
extern void XSIMD(codelet_t1fv_6)(planner *);
extern void XSIMD(codelet_t1fv_7)(planner *);
extern void XSIMD(codelet_t1fv_8)(planner *);
extern void XSIMD(codelet_t1fv_9)(planner *);
extern void XSIMD(codelet_t1fv_10)(planner *);
extern void XSIMD(codelet_t1fv_12)(planner *);
extern void XSIMD(codelet_t1fv_15)(planner *);
extern void XSIMD(codelet_t1fv_16)(planner *);
extern void XSIMD(codelet_t1fv_32)(planner *);
extern void XSIMD(codelet_t1fv_64)(planner *);
extern void XSIMD(codelet_t1fv_20)(planner *);
extern void XSIMD(codelet_t1fv_25)(planner *);
extern void XSIMD(codelet_t2fv_2)(planner *);
extern void XSIMD(codelet_t2fv_4)(planner *);
extern void XSIMD(codelet_t2fv_8)(planner *);
extern void XSIMD(codelet_t2fv_16)(planner *);
extern void XSIMD(codelet_t2fv_32)(planner *);
extern void XSIMD(codelet_t2fv_64)(planner *);
extern void XSIMD(codelet_t2fv_5)(planner *);
extern void XSIMD(codelet_t2fv_10)(planner *);
extern void XSIMD(codelet_t2fv_20)(planner *);
extern void XSIMD(codelet_t2fv_25)(planner *);
extern void XSIMD(codelet_t3fv_4)(planner *);
extern void XSIMD(codelet_t3fv_8)(planner *);
extern void XSIMD(codelet_t3fv_16)(planner *);
extern void XSIMD(codelet_t3fv_32)(planner *);
extern void XSIMD(codelet_t3fv_5)(planner *);
extern void XSIMD(codelet_t3fv_10)(planner *);
extern void XSIMD(codelet_t3fv_20)(planner *);
extern void XSIMD(codelet_t3fv_25)(planner *);
extern void XSIMD(codelet_t1buv_2)(planner *);
extern void XSIMD(codelet_t1buv_3)(planner *);
extern void XSIMD(codelet_t1buv_4)(planner *);
extern void XSIMD(codelet_t1buv_5)(planner *);
extern void XSIMD(codelet_t1buv_6)(planner *);
extern void XSIMD(codelet_t1buv_7)(planner *);
extern void XSIMD(codelet_t1buv_8)(planner *);
extern void XSIMD(codelet_t1buv_9)(planner *);
extern void XSIMD(codelet_t1buv_10)(planner *);
extern void XSIMD(codelet_t1bv_2)(planner *);
extern void XSIMD(codelet_t1bv_3)(planner *);
extern void XSIMD(codelet_t1bv_4)(planner *);
extern void XSIMD(codelet_t1bv_5)(planner *);
extern void XSIMD(codelet_t1bv_6)(planner *);
extern void XSIMD(codelet_t1bv_7)(planner *);
extern void XSIMD(codelet_t1bv_8)(planner *);
extern void XSIMD(codelet_t1bv_9)(planner *);
extern void XSIMD(codelet_t1bv_10)(planner *);
extern void XSIMD(codelet_t1bv_12)(planner *);
extern void XSIMD(codelet_t1bv_15)(planner *);
extern void XSIMD(codelet_t1bv_16)(planner *);
extern void XSIMD(codelet_t1bv_32)(planner *);
extern void XSIMD(codelet_t1bv_64)(planner *);
extern void XSIMD(codelet_t1bv_20)(planner *);
extern void XSIMD(codelet_t1bv_25)(planner *);
extern void XSIMD(codelet_t2bv_2)(planner *);
extern void XSIMD(codelet_t2bv_4)(planner *);
extern void XSIMD(codelet_t2bv_8)(planner *);
extern void XSIMD(codelet_t2bv_16)(planner *);
extern void XSIMD(codelet_t2bv_32)(planner *);
extern void XSIMD(codelet_t2bv_64)(planner *);
extern void XSIMD(codelet_t2bv_5)(planner *);
extern void XSIMD(codelet_t2bv_10)(planner *);
extern void XSIMD(codelet_t2bv_20)(planner *);
extern void XSIMD(codelet_t2bv_25)(planner *);
extern void XSIMD(codelet_t3bv_4)(planner *);
extern void XSIMD(codelet_t3bv_8)(planner *);
extern void XSIMD(codelet_t3bv_16)(planner *);
extern void XSIMD(codelet_t3bv_32)(planner *);
extern void XSIMD(codelet_t3bv_5)(planner *);
extern void XSIMD(codelet_t3bv_10)(planner *);
extern void XSIMD(codelet_t3bv_20)(planner *);
extern void XSIMD(codelet_t3bv_25)(planner *);
extern void XSIMD(codelet_t1sv_2)(planner *);
extern void XSIMD(codelet_t1sv_4)(planner *);
extern void XSIMD(codelet_t1sv_8)(planner *);
extern void XSIMD(codelet_t1sv_16)(planner *);
extern void XSIMD(codelet_t1sv_32)(planner *);
extern void XSIMD(codelet_t2sv_4)(planner *);
extern void XSIMD(codelet_t2sv_8)(planner *);
extern void XSIMD(codelet_t2sv_16)(planner *);
extern void XSIMD(codelet_t2sv_32)(planner *);
extern void XSIMD(codelet_q1fv_2)(planner *);
extern void XSIMD(codelet_q1fv_4)(planner *);
extern void XSIMD(codelet_q1fv_5)(planner *);
extern void XSIMD(codelet_q1fv_8)(planner *);
extern void XSIMD(codelet_q1bv_2)(planner *);
extern void XSIMD(codelet_q1bv_4)(planner *);
extern void XSIMD(codelet_q1bv_5)(planner *);
extern void XSIMD(codelet_q1bv_8)(planner *);


extern const solvtab XSIMD(solvtab_dft);
const solvtab XSIMD(solvtab_dft) = {
   SOLVTAB(XSIMD(codelet_n1fv_2)),
   SOLVTAB(XSIMD(codelet_n1fv_3)),
   SOLVTAB(XSIMD(codelet_n1fv_4)),
   SOLVTAB(XSIMD(codelet_n1fv_5)),
   SOLVTAB(XSIMD(codelet_n1fv_6)),
   SOLVTAB(XSIMD(codelet_n1fv_7)),
   SOLVTAB(XSIMD(codelet_n1fv_8)),
   SOLVTAB(XSIMD(codelet_n1fv_9)),
   SOLVTAB(XSIMD(codelet_n1fv_10)),
   SOLVTAB(XSIMD(codelet_n1fv_11)),
   SOLVTAB(XSIMD(codelet_n1fv_12)),
   SOLVTAB(XSIMD(codelet_n1fv_13)),
   SOLVTAB(XSIMD(codelet_n1fv_14)),
   SOLVTAB(XSIMD(codelet_n1fv_15)),
   SOLVTAB(XSIMD(codelet_n1fv_16)),
   SOLVTAB(XSIMD(codelet_n1fv_32)),
   SOLVTAB(XSIMD(codelet_n1fv_64)),
   SOLVTAB(XSIMD(codelet_n1fv_128)),
   SOLVTAB(XSIMD(codelet_n1fv_20)),
   SOLVTAB(XSIMD(codelet_n1fv_25)),
   SOLVTAB(XSIMD(codelet_n1bv_2)),
   SOLVTAB(XSIMD(codelet_n1bv_3)),
   SOLVTAB(XSIMD(codelet_n1bv_4)),
   SOLVTAB(XSIMD(codelet_n1bv_5)),
   SOLVTAB(XSIMD(codelet_n1bv_6)),
   SOLVTAB(XSIMD(codelet_n1bv_7)),
   SOLVTAB(XSIMD(codelet_n1bv_8)),
   SOLVTAB(XSIMD(codelet_n1bv_9)),
   SOLVTAB(XSIMD(codelet_n1bv_10)),
   SOLVTAB(XSIMD(codelet_n1bv_11)),
   SOLVTAB(XSIMD(codelet_n1bv_12)),
   SOLVTAB(XSIMD(codelet_n1bv_13)),
   SOLVTAB(XSIMD(codelet_n1bv_14)),
   SOLVTAB(XSIMD(codelet_n1bv_15)),
   SOLVTAB(XSIMD(codelet_n1bv_16)),
   SOLVTAB(XSIMD(codelet_n1bv_32)),
   SOLVTAB(XSIMD(codelet_n1bv_64)),
   SOLVTAB(XSIMD(codelet_n1bv_128)),
   SOLVTAB(XSIMD(codelet_n1bv_20)),
   SOLVTAB(XSIMD(codelet_n1bv_25)),
   SOLVTAB(XSIMD(codelet_n2fv_2)),
   SOLVTAB(XSIMD(codelet_n2fv_4)),
   SOLVTAB(XSIMD(codelet_n2fv_6)),
   SOLVTAB(XSIMD(codelet_n2fv_8)),
   SOLVTAB(XSIMD(codelet_n2fv_10)),
   SOLVTAB(XSIMD(codelet_n2fv_12)),
   SOLVTAB(XSIMD(codelet_n2fv_14)),
   SOLVTAB(XSIMD(codelet_n2fv_16)),
   SOLVTAB(XSIMD(codelet_n2fv_32)),
   SOLVTAB(XSIMD(codelet_n2fv_64)),
   SOLVTAB(XSIMD(codelet_n2fv_20)),
   SOLVTAB(XSIMD(codelet_n2bv_2)),
   SOLVTAB(XSIMD(codelet_n2bv_4)),
   SOLVTAB(XSIMD(codelet_n2bv_6)),
   SOLVTAB(XSIMD(codelet_n2bv_8)),
   SOLVTAB(XSIMD(codelet_n2bv_10)),
   SOLVTAB(XSIMD(codelet_n2bv_12)),
   SOLVTAB(XSIMD(codelet_n2bv_14)),
   SOLVTAB(XSIMD(codelet_n2bv_16)),
   SOLVTAB(XSIMD(codelet_n2bv_32)),
   SOLVTAB(XSIMD(codelet_n2bv_64)),
   SOLVTAB(XSIMD(codelet_n2bv_20)),
   SOLVTAB(XSIMD(codelet_n2sv_4)),
   SOLVTAB(XSIMD(codelet_n2sv_8)),
   SOLVTAB(XSIMD(codelet_n2sv_16)),
   SOLVTAB(XSIMD(codelet_n2sv_32)),
   SOLVTAB(XSIMD(codelet_n2sv_64)),
   SOLVTAB(XSIMD(codelet_t1fuv_2)),
   SOLVTAB(XSIMD(codelet_t1fuv_3)),
   SOLVTAB(XSIMD(codelet_t1fuv_4)),
   SOLVTAB(XSIMD(codelet_t1fuv_5)),
   SOLVTAB(XSIMD(codelet_t1fuv_6)),
   SOLVTAB(XSIMD(codelet_t1fuv_7)),
   SOLVTAB(XSIMD(codelet_t1fuv_8)),
   SOLVTAB(XSIMD(codelet_t1fuv_9)),
   SOLVTAB(XSIMD(codelet_t1fuv_10)),
   SOLVTAB(XSIMD(codelet_t1fv_2)),
   SOLVTAB(XSIMD(codelet_t1fv_3)),
   SOLVTAB(XSIMD(codelet_t1fv_4)),
   SOLVTAB(XSIMD(codelet_t1fv_5)),
   SOLVTAB(XSIMD(codelet_t1fv_6)),
   SOLVTAB(XSIMD(codelet_t1fv_7)),
   SOLVTAB(XSIMD(codelet_t1fv_8)),
   SOLVTAB(XSIMD(codelet_t1fv_9)),
   SOLVTAB(XSIMD(codelet_t1fv_10)),
   SOLVTAB(XSIMD(codelet_t1fv_12)),
   SOLVTAB(XSIMD(codelet_t1fv_15)),
   SOLVTAB(XSIMD(codelet_t1fv_16)),
   SOLVTAB(XSIMD(codelet_t1fv_32)),
   SOLVTAB(XSIMD(codelet_t1fv_64)),
   SOLVTAB(XSIMD(codelet_t1fv_20)),
   SOLVTAB(XSIMD(codelet_t1fv_25)),
   SOLVTAB(XSIMD(codelet_t2fv_2)),
   SOLVTAB(XSIMD(codelet_t2fv_4)),
   SOLVTAB(XSIMD(codelet_t2fv_8)),
   SOLVTAB(XSIMD(codelet_t2fv_16)),
   SOLVTAB(XSIMD(codelet_t2fv_32)),
   SOLVTAB(XSIMD(codelet_t2fv_64)),
   SOLVTAB(XSIMD(codelet_t2fv_5)),
   SOLVTAB(XSIMD(codelet_t2fv_10)),
   SOLVTAB(XSIMD(codelet_t2fv_20)),
   SOLVTAB(XSIMD(codelet_t2fv_25)),
   SOLVTAB(XSIMD(codelet_t3fv_4)),
   SOLVTAB(XSIMD(codelet_t3fv_8)),
   SOLVTAB(XSIMD(codelet_t3fv_16)),
   SOLVTAB(XSIMD(codelet_t3fv_32)),
   SOLVTAB(XSIMD(codelet_t3fv_5)),
   SOLVTAB(XSIMD(codelet_t3fv_10)),
   SOLVTAB(XSIMD(codelet_t3fv_20)),
   SOLVTAB(XSIMD(codelet_t3fv_25)),
   SOLVTAB(XSIMD(codelet_t1buv_2)),
   SOLVTAB(XSIMD(codelet_t1buv_3)),
   SOLVTAB(XSIMD(codelet_t1buv_4)),
   SOLVTAB(XSIMD(codelet_t1buv_5)),
   SOLVTAB(XSIMD(codelet_t1buv_6)),
   SOLVTAB(XSIMD(codelet_t1buv_7)),
   SOLVTAB(XSIMD(codelet_t1buv_8)),
   SOLVTAB(XSIMD(codelet_t1buv_9)),
   SOLVTAB(XSIMD(codelet_t1buv_10)),
   SOLVTAB(XSIMD(codelet_t1bv_2)),
   SOLVTAB(XSIMD(codelet_t1bv_3)),
   SOLVTAB(XSIMD(codelet_t1bv_4)),
   SOLVTAB(XSIMD(codelet_t1bv_5)),
   SOLVTAB(XSIMD(codelet_t1bv_6)),
   SOLVTAB(XSIMD(codelet_t1bv_7)),
   SOLVTAB(XSIMD(codelet_t1bv_8)),
   SOLVTAB(XSIMD(codelet_t1bv_9)),
   SOLVTAB(XSIMD(codelet_t1bv_10)),
   SOLVTAB(XSIMD(codelet_t1bv_12)),
   SOLVTAB(XSIMD(codelet_t1bv_15)),
   SOLVTAB(XSIMD(codelet_t1bv_16)),
   SOLVTAB(XSIMD(codelet_t1bv_32)),
   SOLVTAB(XSIMD(codelet_t1bv_64)),
   SOLVTAB(XSIMD(codelet_t1bv_20)),
   SOLVTAB(XSIMD(codelet_t1bv_25)),
   SOLVTAB(XSIMD(codelet_t2bv_2)),
   SOLVTAB(XSIMD(codelet_t2bv_4)),
   SOLVTAB(XSIMD(codelet_t2bv_8)),
   SOLVTAB(XSIMD(codelet_t2bv_16)),
   SOLVTAB(XSIMD(codelet_t2bv_32)),
   SOLVTAB(XSIMD(codelet_t2bv_64)),
   SOLVTAB(XSIMD(codelet_t2bv_5)),
   SOLVTAB(XSIMD(codelet_t2bv_10)),
   SOLVTAB(XSIMD(codelet_t2bv_20)),
   SOLVTAB(XSIMD(codelet_t2bv_25)),
   SOLVTAB(XSIMD(codelet_t3bv_4)),
   SOLVTAB(XSIMD(codelet_t3bv_8)),
   SOLVTAB(XSIMD(codelet_t3bv_16)),
   SOLVTAB(XSIMD(codelet_t3bv_32)),
   SOLVTAB(XSIMD(codelet_t3bv_5)),
   SOLVTAB(XSIMD(codelet_t3bv_10)),
   SOLVTAB(XSIMD(codelet_t3bv_20)),
   SOLVTAB(XSIMD(codelet_t3bv_25)),
   SOLVTAB(XSIMD(codelet_t1sv_2)),
   SOLVTAB(XSIMD(codelet_t1sv_4)),
   SOLVTAB(XSIMD(codelet_t1sv_8)),
   SOLVTAB(XSIMD(codelet_t1sv_16)),
   SOLVTAB(XSIMD(codelet_t1sv_32)),
   SOLVTAB(XSIMD(codelet_t2sv_4)),
   SOLVTAB(XSIMD(codelet_t2sv_8)),
   SOLVTAB(XSIMD(codelet_t2sv_16)),
   SOLVTAB(XSIMD(codelet_t2sv_32)),
   SOLVTAB(XSIMD(codelet_q1fv_2)),
   SOLVTAB(XSIMD(codelet_q1fv_4)),
   SOLVTAB(XSIMD(codelet_q1fv_5)),
   SOLVTAB(XSIMD(codelet_q1fv_8)),
   SOLVTAB(XSIMD(codelet_q1bv_2)),
   SOLVTAB(XSIMD(codelet_q1bv_4)),
   SOLVTAB(XSIMD(codelet_q1bv_5)),
   SOLVTAB(XSIMD(codelet_q1bv_8)),
   SOLVTAB_END
};
