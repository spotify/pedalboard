#include "kernel/ifftw.h"
#include SIMD_HEADER

extern void XSIMD(codelet_hc2cfdftv_2)(planner *);
extern void XSIMD(codelet_hc2cfdftv_4)(planner *);
extern void XSIMD(codelet_hc2cfdftv_6)(planner *);
extern void XSIMD(codelet_hc2cfdftv_8)(planner *);
extern void XSIMD(codelet_hc2cfdftv_10)(planner *);
extern void XSIMD(codelet_hc2cfdftv_12)(planner *);
extern void XSIMD(codelet_hc2cfdftv_16)(planner *);
extern void XSIMD(codelet_hc2cfdftv_32)(planner *);
extern void XSIMD(codelet_hc2cfdftv_20)(planner *);
extern void XSIMD(codelet_hc2cbdftv_2)(planner *);
extern void XSIMD(codelet_hc2cbdftv_4)(planner *);
extern void XSIMD(codelet_hc2cbdftv_6)(planner *);
extern void XSIMD(codelet_hc2cbdftv_8)(planner *);
extern void XSIMD(codelet_hc2cbdftv_10)(planner *);
extern void XSIMD(codelet_hc2cbdftv_12)(planner *);
extern void XSIMD(codelet_hc2cbdftv_16)(planner *);
extern void XSIMD(codelet_hc2cbdftv_32)(planner *);
extern void XSIMD(codelet_hc2cbdftv_20)(planner *);


extern const solvtab XSIMD(solvtab_rdft);
const solvtab XSIMD(solvtab_rdft) = {
   SOLVTAB(XSIMD(codelet_hc2cfdftv_2)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_4)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_6)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_8)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_10)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_12)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_16)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_32)),
   SOLVTAB(XSIMD(codelet_hc2cfdftv_20)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_2)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_4)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_6)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_8)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_10)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_12)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_16)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_32)),
   SOLVTAB(XSIMD(codelet_hc2cbdftv_20)),
   SOLVTAB_END
};
