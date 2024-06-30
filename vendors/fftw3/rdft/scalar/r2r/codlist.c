#include "kernel/ifftw.h"


extern void X(codelet_e01_8)(planner *);
extern void X(codelet_e10_8)(planner *);


extern const solvtab X(solvtab_rdft_r2r);
const solvtab X(solvtab_rdft_r2r) = {
   SOLVTAB(X(codelet_e01_8)),
   SOLVTAB(X(codelet_e10_8)),
   SOLVTAB_END
};
