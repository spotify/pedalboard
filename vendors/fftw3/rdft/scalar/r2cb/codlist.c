#include "kernel/ifftw.h"


extern void X(codelet_r2cb_2)(planner *);
extern void X(codelet_r2cb_3)(planner *);
extern void X(codelet_r2cb_4)(planner *);
extern void X(codelet_r2cb_5)(planner *);
extern void X(codelet_r2cb_6)(planner *);
extern void X(codelet_r2cb_7)(planner *);
extern void X(codelet_r2cb_8)(planner *);
extern void X(codelet_r2cb_9)(planner *);
extern void X(codelet_r2cb_10)(planner *);
extern void X(codelet_r2cb_11)(planner *);
extern void X(codelet_r2cb_12)(planner *);
extern void X(codelet_r2cb_13)(planner *);
extern void X(codelet_r2cb_14)(planner *);
extern void X(codelet_r2cb_15)(planner *);
extern void X(codelet_r2cb_16)(planner *);
extern void X(codelet_r2cb_32)(planner *);
extern void X(codelet_r2cb_64)(planner *);
extern void X(codelet_r2cb_128)(planner *);
extern void X(codelet_r2cb_20)(planner *);
extern void X(codelet_r2cb_25)(planner *);
extern void X(codelet_hb_2)(planner *);
extern void X(codelet_hb_3)(planner *);
extern void X(codelet_hb_4)(planner *);
extern void X(codelet_hb_5)(planner *);
extern void X(codelet_hb_6)(planner *);
extern void X(codelet_hb_7)(planner *);
extern void X(codelet_hb_8)(planner *);
extern void X(codelet_hb_9)(planner *);
extern void X(codelet_hb_10)(planner *);
extern void X(codelet_hb_12)(planner *);
extern void X(codelet_hb_15)(planner *);
extern void X(codelet_hb_16)(planner *);
extern void X(codelet_hb_32)(planner *);
extern void X(codelet_hb_64)(planner *);
extern void X(codelet_hb_20)(planner *);
extern void X(codelet_hb_25)(planner *);
extern void X(codelet_hb2_4)(planner *);
extern void X(codelet_hb2_8)(planner *);
extern void X(codelet_hb2_16)(planner *);
extern void X(codelet_hb2_32)(planner *);
extern void X(codelet_hb2_5)(planner *);
extern void X(codelet_hb2_20)(planner *);
extern void X(codelet_hb2_25)(planner *);
extern void X(codelet_r2cbIII_2)(planner *);
extern void X(codelet_r2cbIII_3)(planner *);
extern void X(codelet_r2cbIII_4)(planner *);
extern void X(codelet_r2cbIII_5)(planner *);
extern void X(codelet_r2cbIII_6)(planner *);
extern void X(codelet_r2cbIII_7)(planner *);
extern void X(codelet_r2cbIII_8)(planner *);
extern void X(codelet_r2cbIII_9)(planner *);
extern void X(codelet_r2cbIII_10)(planner *);
extern void X(codelet_r2cbIII_12)(planner *);
extern void X(codelet_r2cbIII_15)(planner *);
extern void X(codelet_r2cbIII_16)(planner *);
extern void X(codelet_r2cbIII_32)(planner *);
extern void X(codelet_r2cbIII_64)(planner *);
extern void X(codelet_r2cbIII_20)(planner *);
extern void X(codelet_r2cbIII_25)(planner *);
extern void X(codelet_hc2cb_2)(planner *);
extern void X(codelet_hc2cb_4)(planner *);
extern void X(codelet_hc2cb_6)(planner *);
extern void X(codelet_hc2cb_8)(planner *);
extern void X(codelet_hc2cb_10)(planner *);
extern void X(codelet_hc2cb_12)(planner *);
extern void X(codelet_hc2cb_16)(planner *);
extern void X(codelet_hc2cb_32)(planner *);
extern void X(codelet_hc2cb_20)(planner *);
extern void X(codelet_hc2cb2_4)(planner *);
extern void X(codelet_hc2cb2_8)(planner *);
extern void X(codelet_hc2cb2_16)(planner *);
extern void X(codelet_hc2cb2_32)(planner *);
extern void X(codelet_hc2cb2_20)(planner *);
extern void X(codelet_hc2cbdft_2)(planner *);
extern void X(codelet_hc2cbdft_4)(planner *);
extern void X(codelet_hc2cbdft_6)(planner *);
extern void X(codelet_hc2cbdft_8)(planner *);
extern void X(codelet_hc2cbdft_10)(planner *);
extern void X(codelet_hc2cbdft_12)(planner *);
extern void X(codelet_hc2cbdft_16)(planner *);
extern void X(codelet_hc2cbdft_32)(planner *);
extern void X(codelet_hc2cbdft_20)(planner *);
extern void X(codelet_hc2cbdft2_4)(planner *);
extern void X(codelet_hc2cbdft2_8)(planner *);
extern void X(codelet_hc2cbdft2_16)(planner *);
extern void X(codelet_hc2cbdft2_32)(planner *);
extern void X(codelet_hc2cbdft2_20)(planner *);


extern const solvtab X(solvtab_rdft_r2cb);
const solvtab X(solvtab_rdft_r2cb) = {
   SOLVTAB(X(codelet_r2cb_2)),
   SOLVTAB(X(codelet_r2cb_3)),
   SOLVTAB(X(codelet_r2cb_4)),
   SOLVTAB(X(codelet_r2cb_5)),
   SOLVTAB(X(codelet_r2cb_6)),
   SOLVTAB(X(codelet_r2cb_7)),
   SOLVTAB(X(codelet_r2cb_8)),
   SOLVTAB(X(codelet_r2cb_9)),
   SOLVTAB(X(codelet_r2cb_10)),
   SOLVTAB(X(codelet_r2cb_11)),
   SOLVTAB(X(codelet_r2cb_12)),
   SOLVTAB(X(codelet_r2cb_13)),
   SOLVTAB(X(codelet_r2cb_14)),
   SOLVTAB(X(codelet_r2cb_15)),
   SOLVTAB(X(codelet_r2cb_16)),
   SOLVTAB(X(codelet_r2cb_32)),
   SOLVTAB(X(codelet_r2cb_64)),
   SOLVTAB(X(codelet_r2cb_128)),
   SOLVTAB(X(codelet_r2cb_20)),
   SOLVTAB(X(codelet_r2cb_25)),
   SOLVTAB(X(codelet_hb_2)),
   SOLVTAB(X(codelet_hb_3)),
   SOLVTAB(X(codelet_hb_4)),
   SOLVTAB(X(codelet_hb_5)),
   SOLVTAB(X(codelet_hb_6)),
   SOLVTAB(X(codelet_hb_7)),
   SOLVTAB(X(codelet_hb_8)),
   SOLVTAB(X(codelet_hb_9)),
   SOLVTAB(X(codelet_hb_10)),
   SOLVTAB(X(codelet_hb_12)),
   SOLVTAB(X(codelet_hb_15)),
   SOLVTAB(X(codelet_hb_16)),
   SOLVTAB(X(codelet_hb_32)),
   SOLVTAB(X(codelet_hb_64)),
   SOLVTAB(X(codelet_hb_20)),
   SOLVTAB(X(codelet_hb_25)),
   SOLVTAB(X(codelet_hb2_4)),
   SOLVTAB(X(codelet_hb2_8)),
   SOLVTAB(X(codelet_hb2_16)),
   SOLVTAB(X(codelet_hb2_32)),
   SOLVTAB(X(codelet_hb2_5)),
   SOLVTAB(X(codelet_hb2_20)),
   SOLVTAB(X(codelet_hb2_25)),
   SOLVTAB(X(codelet_r2cbIII_2)),
   SOLVTAB(X(codelet_r2cbIII_3)),
   SOLVTAB(X(codelet_r2cbIII_4)),
   SOLVTAB(X(codelet_r2cbIII_5)),
   SOLVTAB(X(codelet_r2cbIII_6)),
   SOLVTAB(X(codelet_r2cbIII_7)),
   SOLVTAB(X(codelet_r2cbIII_8)),
   SOLVTAB(X(codelet_r2cbIII_9)),
   SOLVTAB(X(codelet_r2cbIII_10)),
   SOLVTAB(X(codelet_r2cbIII_12)),
   SOLVTAB(X(codelet_r2cbIII_15)),
   SOLVTAB(X(codelet_r2cbIII_16)),
   SOLVTAB(X(codelet_r2cbIII_32)),
   SOLVTAB(X(codelet_r2cbIII_64)),
   SOLVTAB(X(codelet_r2cbIII_20)),
   SOLVTAB(X(codelet_r2cbIII_25)),
   SOLVTAB(X(codelet_hc2cb_2)),
   SOLVTAB(X(codelet_hc2cb_4)),
   SOLVTAB(X(codelet_hc2cb_6)),
   SOLVTAB(X(codelet_hc2cb_8)),
   SOLVTAB(X(codelet_hc2cb_10)),
   SOLVTAB(X(codelet_hc2cb_12)),
   SOLVTAB(X(codelet_hc2cb_16)),
   SOLVTAB(X(codelet_hc2cb_32)),
   SOLVTAB(X(codelet_hc2cb_20)),
   SOLVTAB(X(codelet_hc2cb2_4)),
   SOLVTAB(X(codelet_hc2cb2_8)),
   SOLVTAB(X(codelet_hc2cb2_16)),
   SOLVTAB(X(codelet_hc2cb2_32)),
   SOLVTAB(X(codelet_hc2cb2_20)),
   SOLVTAB(X(codelet_hc2cbdft_2)),
   SOLVTAB(X(codelet_hc2cbdft_4)),
   SOLVTAB(X(codelet_hc2cbdft_6)),
   SOLVTAB(X(codelet_hc2cbdft_8)),
   SOLVTAB(X(codelet_hc2cbdft_10)),
   SOLVTAB(X(codelet_hc2cbdft_12)),
   SOLVTAB(X(codelet_hc2cbdft_16)),
   SOLVTAB(X(codelet_hc2cbdft_32)),
   SOLVTAB(X(codelet_hc2cbdft_20)),
   SOLVTAB(X(codelet_hc2cbdft2_4)),
   SOLVTAB(X(codelet_hc2cbdft2_8)),
   SOLVTAB(X(codelet_hc2cbdft2_16)),
   SOLVTAB(X(codelet_hc2cbdft2_32)),
   SOLVTAB(X(codelet_hc2cbdft2_20)),
   SOLVTAB_END
};
