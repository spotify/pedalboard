/* fftw hook to be used in the benchmark program.  
   
   We keep it in a separate file because 

   1) bench.c is supposed to test the API---we do not want to #include
      "ifftw.h" and accidentally use internal symbols/macros.
   2) this code is a royal mess.  The messiness is due to
      A) confusion between internal fftw tensors and bench_tensor's
         (which we want to keep separate because the benchmark
	  program tests other routines too)
      B) despite A), our desire to recycle the libbench verifier.
*/

#include <stdio.h>
#include "libbench2/bench-user.h"

#define CALLING_FFTW /* hack for Windows DLL nonsense */
#include "api/api.h"
#include "dft/dft.h"
#include "rdft/rdft.h"

extern int paranoid; /* in bench.c */
extern X(plan) the_plan; /* in bench.c */

/*
  transform an fftw tensor into a bench_tensor.
*/
static bench_tensor *fftw_tensor_to_bench_tensor(tensor *t)
{
     bench_tensor *bt = mktensor(t->rnk);

     if (FINITE_RNK(t->rnk)) {
	  int i;
	  for (i = 0; i < t->rnk; ++i) {
	       /* FIXME: 64-bit unclean because of INT -> int conversion */
	       bt->dims[i].n = t->dims[i].n;
	       bt->dims[i].is = t->dims[i].is;
	       bt->dims[i].os = t->dims[i].os;
	       BENCH_ASSERT(bt->dims[i].n == t->dims[i].n);
	       BENCH_ASSERT(bt->dims[i].is == t->dims[i].is);
	       BENCH_ASSERT(bt->dims[i].os == t->dims[i].os);
	  }
     }
     return bt;
}

/*
  transform an fftw problem into a bench_problem.
*/
static bench_problem *fftw_problem_to_bench_problem(planner *plnr,
						    const problem *p_)
{
     bench_problem *bp = 0;
     switch (p_->adt->problem_kind) {
	 case PROBLEM_DFT:
	 {
	      const problem_dft *p = (const problem_dft *) p_;
	  
	      if (!p->ri || !p->ii)
		   abort();

	      bp = (bench_problem *) bench_malloc(sizeof(bench_problem));

	      bp->kind = PROBLEM_COMPLEX;
	      bp->sign = FFT_SIGN;
	      bp->split = 1; /* tensor strides are in R's, not C's */
	      bp->in = UNTAINT(p->ri);
	      bp->out = UNTAINT(p->ro);
	      bp->ini = UNTAINT(p->ii);
	      bp->outi = UNTAINT(p->io);
	      bp->inphys = bp->outphys = 0;
	      bp->iphyssz = bp->ophyssz = 0;
	      bp->in_place = p->ri == p->ro;
	      bp->sz = fftw_tensor_to_bench_tensor(p->sz);
	      bp->vecsz = fftw_tensor_to_bench_tensor(p->vecsz);
	      bp->k = 0;
	      break;
	 }
	 case PROBLEM_RDFT:
	 {
	      const problem_rdft *p = (const problem_rdft *) p_;
	      int i;

	      if (!p->I || !p->O)
		   abort();

	      for (i = 0; i < p->sz->rnk; ++i)
		   switch (p->kind[i]) {
		       case R2HC01:
		       case R2HC10:
		       case R2HC11:
		       case HC2R01:
		       case HC2R10:
		       case HC2R11:
			    return bp;
		       default:
			    ;
		   }
	  
	      bp = (bench_problem *) bench_malloc(sizeof(bench_problem));

	      bp->kind = PROBLEM_R2R;
	      bp->sign = FFT_SIGN;
	      bp->split = 0;
	      bp->in = UNTAINT(p->I);
	      bp->out = UNTAINT(p->O);
	      bp->ini = bp->outi = 0;
	      bp->inphys = bp->outphys = 0;
	      bp->iphyssz = bp->ophyssz = 0;
	      bp->in_place = p->I == p->O;
	      bp->sz = fftw_tensor_to_bench_tensor(p->sz);
	      bp->vecsz = fftw_tensor_to_bench_tensor(p->vecsz);
	      bp->k = (r2r_kind_t *) bench_malloc(sizeof(r2r_kind_t) * p->sz->rnk);
	      for (i = 0; i < p->sz->rnk; ++i)
		   switch (p->kind[i]) {
		       case R2HC: bp->k[i] = R2R_R2HC; break;
		       case HC2R: bp->k[i] = R2R_HC2R; break;
		       case DHT: bp->k[i] = R2R_DHT; break;
		       case REDFT00: bp->k[i] = R2R_REDFT00; break;
		       case REDFT01: bp->k[i] = R2R_REDFT01; break;
		       case REDFT10: bp->k[i] = R2R_REDFT10; break;
		       case REDFT11: bp->k[i] = R2R_REDFT11; break;
		       case RODFT00: bp->k[i] = R2R_RODFT00; break;
		       case RODFT01: bp->k[i] = R2R_RODFT01; break;
		       case RODFT10: bp->k[i] = R2R_RODFT10; break;
		       case RODFT11: bp->k[i] = R2R_RODFT11; break;
		       default: CK(0);
		   }
	      break;
	 }
	 case PROBLEM_RDFT2:
	 {
	      const problem_rdft2 *p = (const problem_rdft2 *) p_;
	      int rnk = p->sz->rnk;
	  
	      if (!p->r0 || !p->r1 || !p->cr || !p->ci)
		   abort();
	      
	      /* give up verifying rdft2 R2HCII */
	      if (p->kind != R2HC && p->kind != HC2R)
		   return bp;

	      if (rnk > 0) {
		   /* can't verify separate even/odd arrays for now */
		   if (2 * (p->r1 - p->r0) !=
		       ((p->kind == R2HC) ? 
			p->sz->dims[rnk-1].is : p->sz->dims[rnk-1].os))
			return bp;
	      }

	      bp = (bench_problem *) bench_malloc(sizeof(bench_problem));

	      bp->kind = PROBLEM_REAL;
	      bp->sign = p->kind == R2HC ? FFT_SIGN : -FFT_SIGN;
	      bp->split = 1; /* tensor strides are in R's, not C's */
	      if (p->kind == R2HC) {
		   bp->sign = FFT_SIGN;
		   bp->in = UNTAINT(p->r0);
		   bp->out = UNTAINT(p->cr);
		   bp->ini = 0;
		   bp->outi = UNTAINT(p->ci);
	      }
	      else {
		   bp->sign = -FFT_SIGN;
		   bp->out = UNTAINT(p->r0);
		   bp->in = UNTAINT(p->cr);
		   bp->outi = 0;
		   bp->ini = UNTAINT(p->ci);
	      }
	      bp->inphys = bp->outphys = 0;
	      bp->iphyssz = bp->ophyssz = 0;
	      bp->in_place = p->r0 == p->cr;
	      bp->sz = fftw_tensor_to_bench_tensor(p->sz);
	      if (rnk > 0) {
		   if (p->kind == R2HC)
			bp->sz->dims[rnk-1].is /= 2;
		   else 
			bp->sz->dims[rnk-1].os /= 2;
	      }
	      bp->vecsz = fftw_tensor_to_bench_tensor(p->vecsz);
	      bp->k = 0;
	      break;
	 }
	 default: 
	      abort();
     }

     bp->userinfo = 0;
     bp->pstring = 0;
     bp->destroy_input = !NO_DESTROY_INPUTP(plnr);

     return bp;
}

static void hook(planner *plnr, plan *pln, const problem *p_, int optimalp)
{
     int rounds = 5;
     double tol = SINGLE_PRECISION ? 1.0e-3 : 1.0e-10;
     UNUSED(optimalp);

     if (verbose > 5) {
	  printer *pr = X(mkprinter_file)(stdout);
	  pr->print(pr, "%P:%(%p%)\n", p_, pln);
	  X(printer_destroy)(pr);
	  printf("cost %g  \n\n", pln->pcost);
     }

     if (paranoid) {
	  bench_problem *bp;

	  bp = fftw_problem_to_bench_problem(plnr, p_);
	  if (bp) {
	       X(plan) the_plan_save = the_plan;

	       the_plan = (apiplan *) MALLOC(sizeof(apiplan), PLANS);
	       the_plan->pln = pln;
	       the_plan->prb = (problem *) p_;

	       X(plan_awake)(pln, AWAKE_SQRTN_TABLE);
	       verify_problem(bp, rounds, tol);
	       X(plan_awake)(pln, SLEEPY);

	       X(ifree)(the_plan);
	       the_plan = the_plan_save;

	       problem_destroy(bp);
	  }

     }
}

static void paranoid_checks(void)
{
     /* FIXME: assumes char = 8 bits, which is false on at least one
	DSP I know of. */
#if 0
     /* if flags_t is not 64 bits i want to know it. */
     CK(sizeof(flags_t) == 8);

     CK(sizeof(md5uint) >= 4);
#endif

     CK(sizeof(uintptr_t) >= sizeof(R *));

     CK(sizeof(INT) >= sizeof(R *));
}

void install_hook(void)
{
     planner *plnr = X(the_planner)();
     plnr->hook = hook;
     paranoid_checks();
}

void uninstall_hook(void)
{
     planner *plnr = X(the_planner)();
     plnr->hook = 0;
}
