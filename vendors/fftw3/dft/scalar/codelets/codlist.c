#include "kernel/ifftw.h"


extern void X(codelet_n1_2)(planner *);
extern void X(codelet_n1_3)(planner *);
extern void X(codelet_n1_4)(planner *);
extern void X(codelet_n1_5)(planner *);
extern void X(codelet_n1_6)(planner *);
extern void X(codelet_n1_7)(planner *);
extern void X(codelet_n1_8)(planner *);
extern void X(codelet_n1_9)(planner *);
extern void X(codelet_n1_10)(planner *);
extern void X(codelet_n1_11)(planner *);
extern void X(codelet_n1_12)(planner *);
extern void X(codelet_n1_13)(planner *);
extern void X(codelet_n1_14)(planner *);
extern void X(codelet_n1_15)(planner *);
extern void X(codelet_n1_16)(planner *);
extern void X(codelet_n1_32)(planner *);
extern void X(codelet_n1_64)(planner *);
extern void X(codelet_n1_20)(planner *);
extern void X(codelet_n1_25)(planner *);
extern void X(codelet_t1_2)(planner *);
extern void X(codelet_t1_3)(planner *);
extern void X(codelet_t1_4)(planner *);
extern void X(codelet_t1_5)(planner *);
extern void X(codelet_t1_6)(planner *);
extern void X(codelet_t1_7)(planner *);
extern void X(codelet_t1_8)(planner *);
extern void X(codelet_t1_9)(planner *);
extern void X(codelet_t1_10)(planner *);
extern void X(codelet_t1_12)(planner *);
extern void X(codelet_t1_15)(planner *);
extern void X(codelet_t1_16)(planner *);
extern void X(codelet_t1_32)(planner *);
extern void X(codelet_t1_64)(planner *);
extern void X(codelet_t1_20)(planner *);
extern void X(codelet_t1_25)(planner *);
extern void X(codelet_t2_4)(planner *);
extern void X(codelet_t2_8)(planner *);
extern void X(codelet_t2_16)(planner *);
extern void X(codelet_t2_32)(planner *);
extern void X(codelet_t2_64)(planner *);
extern void X(codelet_t2_5)(planner *);
extern void X(codelet_t2_10)(planner *);
extern void X(codelet_t2_20)(planner *);
extern void X(codelet_t2_25)(planner *);
extern void X(codelet_q1_2)(planner *);
extern void X(codelet_q1_4)(planner *);
extern void X(codelet_q1_8)(planner *);
extern void X(codelet_q1_3)(planner *);
extern void X(codelet_q1_5)(planner *);
extern void X(codelet_q1_6)(planner *);


extern const solvtab X(solvtab_dft_standard);
const solvtab X(solvtab_dft_standard) = {
   SOLVTAB(X(codelet_n1_2)),
   SOLVTAB(X(codelet_n1_3)),
   SOLVTAB(X(codelet_n1_4)),
   SOLVTAB(X(codelet_n1_5)),
   SOLVTAB(X(codelet_n1_6)),
   SOLVTAB(X(codelet_n1_7)),
   SOLVTAB(X(codelet_n1_8)),
   SOLVTAB(X(codelet_n1_9)),
   SOLVTAB(X(codelet_n1_10)),
   SOLVTAB(X(codelet_n1_11)),
   SOLVTAB(X(codelet_n1_12)),
   SOLVTAB(X(codelet_n1_13)),
   SOLVTAB(X(codelet_n1_14)),
   SOLVTAB(X(codelet_n1_15)),
   SOLVTAB(X(codelet_n1_16)),
   SOLVTAB(X(codelet_n1_32)),
   SOLVTAB(X(codelet_n1_64)),
   SOLVTAB(X(codelet_n1_20)),
   SOLVTAB(X(codelet_n1_25)),
   SOLVTAB(X(codelet_t1_2)),
   SOLVTAB(X(codelet_t1_3)),
   SOLVTAB(X(codelet_t1_4)),
   SOLVTAB(X(codelet_t1_5)),
   SOLVTAB(X(codelet_t1_6)),
   SOLVTAB(X(codelet_t1_7)),
   SOLVTAB(X(codelet_t1_8)),
   SOLVTAB(X(codelet_t1_9)),
   SOLVTAB(X(codelet_t1_10)),
   SOLVTAB(X(codelet_t1_12)),
   SOLVTAB(X(codelet_t1_15)),
   SOLVTAB(X(codelet_t1_16)),
   SOLVTAB(X(codelet_t1_32)),
   SOLVTAB(X(codelet_t1_64)),
   SOLVTAB(X(codelet_t1_20)),
   SOLVTAB(X(codelet_t1_25)),
   SOLVTAB(X(codelet_t2_4)),
   SOLVTAB(X(codelet_t2_8)),
   SOLVTAB(X(codelet_t2_16)),
   SOLVTAB(X(codelet_t2_32)),
   SOLVTAB(X(codelet_t2_64)),
   SOLVTAB(X(codelet_t2_5)),
   SOLVTAB(X(codelet_t2_10)),
   SOLVTAB(X(codelet_t2_20)),
   SOLVTAB(X(codelet_t2_25)),
   SOLVTAB(X(codelet_q1_2)),
   SOLVTAB(X(codelet_q1_4)),
   SOLVTAB(X(codelet_q1_8)),
   SOLVTAB(X(codelet_q1_3)),
   SOLVTAB(X(codelet_q1_5)),
   SOLVTAB(X(codelet_q1_6)),
   SOLVTAB_END
};
