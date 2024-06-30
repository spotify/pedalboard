#include "kernel/ifftw.h"


extern void X(codelet_r2cf_2)(planner *);
extern void X(codelet_r2cf_3)(planner *);
extern void X(codelet_r2cf_4)(planner *);
extern void X(codelet_r2cf_5)(planner *);
extern void X(codelet_r2cf_6)(planner *);
extern void X(codelet_r2cf_7)(planner *);
extern void X(codelet_r2cf_8)(planner *);
extern void X(codelet_r2cf_9)(planner *);
extern void X(codelet_r2cf_10)(planner *);
extern void X(codelet_r2cf_11)(planner *);
extern void X(codelet_r2cf_12)(planner *);
extern void X(codelet_r2cf_13)(planner *);
extern void X(codelet_r2cf_14)(planner *);
extern void X(codelet_r2cf_15)(planner *);
extern void X(codelet_r2cf_16)(planner *);
extern void X(codelet_r2cf_32)(planner *);
extern void X(codelet_r2cf_64)(planner *);
extern void X(codelet_r2cf_128)(planner *);
extern void X(codelet_r2cf_20)(planner *);
extern void X(codelet_r2cf_25)(planner *);
extern void X(codelet_hf_2)(planner *);
extern void X(codelet_hf_3)(planner *);
extern void X(codelet_hf_4)(planner *);
extern void X(codelet_hf_5)(planner *);
extern void X(codelet_hf_6)(planner *);
extern void X(codelet_hf_7)(planner *);
extern void X(codelet_hf_8)(planner *);
extern void X(codelet_hf_9)(planner *);
extern void X(codelet_hf_10)(planner *);
extern void X(codelet_hf_12)(planner *);
extern void X(codelet_hf_15)(planner *);
extern void X(codelet_hf_16)(planner *);
extern void X(codelet_hf_32)(planner *);
extern void X(codelet_hf_64)(planner *);
extern void X(codelet_hf_20)(planner *);
extern void X(codelet_hf_25)(planner *);
extern void X(codelet_hf2_4)(planner *);
extern void X(codelet_hf2_8)(planner *);
extern void X(codelet_hf2_16)(planner *);
extern void X(codelet_hf2_32)(planner *);
extern void X(codelet_hf2_5)(planner *);
extern void X(codelet_hf2_20)(planner *);
extern void X(codelet_hf2_25)(planner *);
extern void X(codelet_r2cfII_2)(planner *);
extern void X(codelet_r2cfII_3)(planner *);
extern void X(codelet_r2cfII_4)(planner *);
extern void X(codelet_r2cfII_5)(planner *);
extern void X(codelet_r2cfII_6)(planner *);
extern void X(codelet_r2cfII_7)(planner *);
extern void X(codelet_r2cfII_8)(planner *);
extern void X(codelet_r2cfII_9)(planner *);
extern void X(codelet_r2cfII_10)(planner *);
extern void X(codelet_r2cfII_12)(planner *);
extern void X(codelet_r2cfII_15)(planner *);
extern void X(codelet_r2cfII_16)(planner *);
extern void X(codelet_r2cfII_32)(planner *);
extern void X(codelet_r2cfII_64)(planner *);
extern void X(codelet_r2cfII_20)(planner *);
extern void X(codelet_r2cfII_25)(planner *);
extern void X(codelet_hc2cf_2)(planner *);
extern void X(codelet_hc2cf_4)(planner *);
extern void X(codelet_hc2cf_6)(planner *);
extern void X(codelet_hc2cf_8)(planner *);
extern void X(codelet_hc2cf_10)(planner *);
extern void X(codelet_hc2cf_12)(planner *);
extern void X(codelet_hc2cf_16)(planner *);
extern void X(codelet_hc2cf_32)(planner *);
extern void X(codelet_hc2cf_20)(planner *);
extern void X(codelet_hc2cf2_4)(planner *);
extern void X(codelet_hc2cf2_8)(planner *);
extern void X(codelet_hc2cf2_16)(planner *);
extern void X(codelet_hc2cf2_32)(planner *);
extern void X(codelet_hc2cf2_20)(planner *);
extern void X(codelet_hc2cfdft_2)(planner *);
extern void X(codelet_hc2cfdft_4)(planner *);
extern void X(codelet_hc2cfdft_6)(planner *);
extern void X(codelet_hc2cfdft_8)(planner *);
extern void X(codelet_hc2cfdft_10)(planner *);
extern void X(codelet_hc2cfdft_12)(planner *);
extern void X(codelet_hc2cfdft_16)(planner *);
extern void X(codelet_hc2cfdft_32)(planner *);
extern void X(codelet_hc2cfdft_20)(planner *);
extern void X(codelet_hc2cfdft2_4)(planner *);
extern void X(codelet_hc2cfdft2_8)(planner *);
extern void X(codelet_hc2cfdft2_16)(planner *);
extern void X(codelet_hc2cfdft2_32)(planner *);
extern void X(codelet_hc2cfdft2_20)(planner *);


extern const solvtab X(solvtab_rdft_r2cf);
const solvtab X(solvtab_rdft_r2cf) = {
   SOLVTAB(X(codelet_r2cf_2)),
   SOLVTAB(X(codelet_r2cf_3)),
   SOLVTAB(X(codelet_r2cf_4)),
   SOLVTAB(X(codelet_r2cf_5)),
   SOLVTAB(X(codelet_r2cf_6)),
   SOLVTAB(X(codelet_r2cf_7)),
   SOLVTAB(X(codelet_r2cf_8)),
   SOLVTAB(X(codelet_r2cf_9)),
   SOLVTAB(X(codelet_r2cf_10)),
   SOLVTAB(X(codelet_r2cf_11)),
   SOLVTAB(X(codelet_r2cf_12)),
   SOLVTAB(X(codelet_r2cf_13)),
   SOLVTAB(X(codelet_r2cf_14)),
   SOLVTAB(X(codelet_r2cf_15)),
   SOLVTAB(X(codelet_r2cf_16)),
   SOLVTAB(X(codelet_r2cf_32)),
   SOLVTAB(X(codelet_r2cf_64)),
   SOLVTAB(X(codelet_r2cf_128)),
   SOLVTAB(X(codelet_r2cf_20)),
   SOLVTAB(X(codelet_r2cf_25)),
   SOLVTAB(X(codelet_hf_2)),
   SOLVTAB(X(codelet_hf_3)),
   SOLVTAB(X(codelet_hf_4)),
   SOLVTAB(X(codelet_hf_5)),
   SOLVTAB(X(codelet_hf_6)),
   SOLVTAB(X(codelet_hf_7)),
   SOLVTAB(X(codelet_hf_8)),
   SOLVTAB(X(codelet_hf_9)),
   SOLVTAB(X(codelet_hf_10)),
   SOLVTAB(X(codelet_hf_12)),
   SOLVTAB(X(codelet_hf_15)),
   SOLVTAB(X(codelet_hf_16)),
   SOLVTAB(X(codelet_hf_32)),
   SOLVTAB(X(codelet_hf_64)),
   SOLVTAB(X(codelet_hf_20)),
   SOLVTAB(X(codelet_hf_25)),
   SOLVTAB(X(codelet_hf2_4)),
   SOLVTAB(X(codelet_hf2_8)),
   SOLVTAB(X(codelet_hf2_16)),
   SOLVTAB(X(codelet_hf2_32)),
   SOLVTAB(X(codelet_hf2_5)),
   SOLVTAB(X(codelet_hf2_20)),
   SOLVTAB(X(codelet_hf2_25)),
   SOLVTAB(X(codelet_r2cfII_2)),
   SOLVTAB(X(codelet_r2cfII_3)),
   SOLVTAB(X(codelet_r2cfII_4)),
   SOLVTAB(X(codelet_r2cfII_5)),
   SOLVTAB(X(codelet_r2cfII_6)),
   SOLVTAB(X(codelet_r2cfII_7)),
   SOLVTAB(X(codelet_r2cfII_8)),
   SOLVTAB(X(codelet_r2cfII_9)),
   SOLVTAB(X(codelet_r2cfII_10)),
   SOLVTAB(X(codelet_r2cfII_12)),
   SOLVTAB(X(codelet_r2cfII_15)),
   SOLVTAB(X(codelet_r2cfII_16)),
   SOLVTAB(X(codelet_r2cfII_32)),
   SOLVTAB(X(codelet_r2cfII_64)),
   SOLVTAB(X(codelet_r2cfII_20)),
   SOLVTAB(X(codelet_r2cfII_25)),
   SOLVTAB(X(codelet_hc2cf_2)),
   SOLVTAB(X(codelet_hc2cf_4)),
   SOLVTAB(X(codelet_hc2cf_6)),
   SOLVTAB(X(codelet_hc2cf_8)),
   SOLVTAB(X(codelet_hc2cf_10)),
   SOLVTAB(X(codelet_hc2cf_12)),
   SOLVTAB(X(codelet_hc2cf_16)),
   SOLVTAB(X(codelet_hc2cf_32)),
   SOLVTAB(X(codelet_hc2cf_20)),
   SOLVTAB(X(codelet_hc2cf2_4)),
   SOLVTAB(X(codelet_hc2cf2_8)),
   SOLVTAB(X(codelet_hc2cf2_16)),
   SOLVTAB(X(codelet_hc2cf2_32)),
   SOLVTAB(X(codelet_hc2cf2_20)),
   SOLVTAB(X(codelet_hc2cfdft_2)),
   SOLVTAB(X(codelet_hc2cfdft_4)),
   SOLVTAB(X(codelet_hc2cfdft_6)),
   SOLVTAB(X(codelet_hc2cfdft_8)),
   SOLVTAB(X(codelet_hc2cfdft_10)),
   SOLVTAB(X(codelet_hc2cfdft_12)),
   SOLVTAB(X(codelet_hc2cfdft_16)),
   SOLVTAB(X(codelet_hc2cfdft_32)),
   SOLVTAB(X(codelet_hc2cfdft_20)),
   SOLVTAB(X(codelet_hc2cfdft2_4)),
   SOLVTAB(X(codelet_hc2cfdft2_8)),
   SOLVTAB(X(codelet_hc2cfdft2_16)),
   SOLVTAB(X(codelet_hc2cfdft2_32)),
   SOLVTAB(X(codelet_hc2cfdft2_20)),
   SOLVTAB_END
};
