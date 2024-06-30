/*
 * Copyright (c) 2000 Matteo Frigo
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "kernel/ifftw.h"
#include <string.h>

/* GNU Coding Standards, Sec. 5.2: "Please write the comments in a GNU
   program in English, because English is the one language that nearly
   all programmers in all countries can read."

                    ingemisco tanquam reus
		    culpa rubet vultus meus
		    supplicanti parce [rms]
*/

#define VALIDP(solution) ((solution)->flags.hash_info & H_VALID)
#define LIVEP(solution) ((solution)->flags.hash_info & H_LIVE)
#define SLVNDX(solution) ((solution)->flags.slvndx)
#define BLISS(flags) (((flags).hash_info) & BLESSING)
#define INFEASIBLE_SLVNDX ((1U<<BITS_FOR_SLVNDX)-1)


#define MAXNAM 64  /* maximum length of registrar's name.
		      Used for reading wisdom.  There is no point
		      in doing this right */


#ifdef FFTW_DEBUG
static void check(hashtab *ht);
#endif

/* x <= y */
#define LEQ(x, y) (((x) & (y)) == (x))

/* A subsumes B */
static int subsumes(const flags_t *a, unsigned slvndx_a, const flags_t *b)
{
     if (slvndx_a != INFEASIBLE_SLVNDX) {
	  A(a->timelimit_impatience == 0);
	  return (LEQ(a->u, b->u) && LEQ(b->l, a->l));
     } else {
	  return (LEQ(a->l, b->l) 
		  && a->timelimit_impatience <= b->timelimit_impatience);
     }
}

static unsigned addmod(unsigned a, unsigned b, unsigned p)
{
     /* gcc-2.95/sparc produces incorrect code for the fast version below. */
#if defined(__sparc__) && defined(__GNUC__)
     /* slow version  */
     return (a + b) % p;
#else
     /* faster version */
     unsigned c = a + b;
     return c >= p ? c - p : c;
#endif
}

/*
  slvdesc management:
*/
static void sgrow(planner *ego)
{
     unsigned osiz = ego->slvdescsiz, nsiz = 1 + osiz + osiz / 4;
     slvdesc *ntab = (slvdesc *)MALLOC(nsiz * sizeof(slvdesc), SLVDESCS);
     slvdesc *otab = ego->slvdescs;
     unsigned i;

     ego->slvdescs = ntab;
     ego->slvdescsiz = nsiz;
     for (i = 0; i < osiz; ++i)
	  ntab[i] = otab[i];
     X(ifree0)(otab);
}

static void register_solver(planner *ego, solver *s)
{
     slvdesc *n;
     int kind;

     if (s) { /* add s to solver list */
	  X(solver_use)(s);

	  A(ego->nslvdesc < INFEASIBLE_SLVNDX);
	  if (ego->nslvdesc >= ego->slvdescsiz)
	       sgrow(ego);

	  n = ego->slvdescs + ego->nslvdesc;

	  n->slv = s;
	  n->reg_nam = ego->cur_reg_nam;
	  n->reg_id = ego->cur_reg_id++;
	  
	  A(strlen(n->reg_nam) < MAXNAM);
	  n->nam_hash = X(hash)(n->reg_nam);

	  kind = s->adt->problem_kind;
	  n->next_for_same_problem_kind = ego->slvdescs_for_problem_kind[kind];
	  ego->slvdescs_for_problem_kind[kind] = (int)/*from unsigned*/ego->nslvdesc;

	  ego->nslvdesc++;
     }
}

static unsigned slookup(planner *ego, char *nam, int id)
{
     unsigned h = X(hash)(nam); /* used to avoid strcmp in the common case */
     FORALL_SOLVERS(ego, s, sp, {
	  UNUSED(s);
	  if (sp->reg_id == id && sp->nam_hash == h
	      && !strcmp(sp->reg_nam, nam))
	       return (unsigned)/*from ptrdiff_t*/(sp - ego->slvdescs);
     });
     return INFEASIBLE_SLVNDX;
}

/* Compute a MD5 hash of the configuration of the planner.
   We store it into the wisdom file to make absolutely sure that
   we are reading wisdom that is applicable */
static void signature_of_configuration(md5 *m, planner *ego)
{
     X(md5begin)(m);
     X(md5unsigned)(m, sizeof(R)); /* so we don't mix different precisions */
     FORALL_SOLVERS(ego, s, sp, {
	  UNUSED(s);
	  X(md5int)(m, sp->reg_id);
	  X(md5puts)(m, sp->reg_nam);
     });
     X(md5end)(m);
}

/*
  md5-related stuff:
*/

/* first hash function */
static unsigned h1(const hashtab *ht, const md5sig s)
{
     unsigned h = s[0] % ht->hashsiz;
     A(h == (s[0] % ht->hashsiz));
     return h;
}

/* second hash function (for double hashing) */
static unsigned h2(const hashtab *ht, const md5sig s)
{
     unsigned h = 1U + s[1] % (ht->hashsiz - 1);
     A(h == (1U + s[1] % (ht->hashsiz - 1)));
     return h;
}

static void md5hash(md5 *m, const problem *p, const planner *plnr)
{
     X(md5begin)(m);
     X(md5unsigned)(m, sizeof(R)); /* so we don't mix different precisions */
     X(md5int)(m, plnr->nthr);
     p->adt->hash(p, m);
     X(md5end)(m);
}

static int md5eq(const md5sig a, const md5sig b)
{
     return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static void sigcpy(const md5sig a, md5sig b)
{
     b[0] = a[0]; b[1] = a[1]; b[2] = a[2]; b[3] = a[3];
}

/*
  memoization routines :
*/

/*
   liber scriptus proferetur
   in quo totum continetur
   unde mundus iudicetur
*/
struct solution_s {
     md5sig s;
     flags_t flags;
};

static solution *htab_lookup(hashtab *ht, const md5sig s, 
			     const flags_t *flagsp)
{
     unsigned g, h = h1(ht, s), d = h2(ht, s);
     solution *best = 0;

     ++ht->lookup;

     /* search all entries that match; select the one with
	the lowest flags.u */
     /* This loop may potentially traverse the whole table, since at
	least one element is guaranteed to be !LIVEP, but all elements
	may be VALIDP.  Hence, we stop after at the first invalid
	element or after traversing the whole table. */
     g = h;
     do {
	  solution *l = ht->solutions + g;
	  ++ht->lookup_iter;
	  if (VALIDP(l)) {
	       if (LIVEP(l)
		   && md5eq(s, l->s)
		   && subsumes(&l->flags, SLVNDX(l), flagsp) ) { 
		    if (!best || LEQ(l->flags.u, best->flags.u))
			 best = l;
	       }
	  } else 
	       break;

	  g = addmod(g, d, ht->hashsiz);
     } while (g != h);

     if (best) 
	  ++ht->succ_lookup;
     return best;
}

static solution *hlookup(planner *ego, const md5sig s, 
			 const flags_t *flagsp)
{
     solution *sol = htab_lookup(&ego->htab_blessed, s, flagsp);
     if (!sol) sol = htab_lookup(&ego->htab_unblessed, s, flagsp);
     return sol;
}

static void fill_slot(hashtab *ht, const md5sig s, const flags_t *flagsp,
		      unsigned slvndx, solution *slot)
{
     ++ht->insert;
     ++ht->nelem;
     A(!LIVEP(slot));
     slot->flags.u = flagsp->u;
     slot->flags.l = flagsp->l;
     slot->flags.timelimit_impatience = flagsp->timelimit_impatience;
     slot->flags.hash_info |= H_VALID | H_LIVE;
     SLVNDX(slot) = slvndx;

     /* keep this check enabled in case we add so many solvers
	that the bitfield overflows */
     CK(SLVNDX(slot) == slvndx);     
     sigcpy(s, slot->s);
}

static void kill_slot(hashtab *ht, solution *slot)
{
     A(LIVEP(slot)); /* ==> */ A(VALIDP(slot));

     --ht->nelem;
     slot->flags.hash_info = H_VALID;
}

static void hinsert0(hashtab *ht, const md5sig s, const flags_t *flagsp, 
		     unsigned slvndx)
{
     solution *l;
     unsigned g, h = h1(ht, s), d = h2(ht, s); 

     ++ht->insert_unknown;

     /* search for nonfull slot */
     for (g = h; ; g = addmod(g, d, ht->hashsiz)) {
	  ++ht->insert_iter;
	  l = ht->solutions + g;
	  if (!LIVEP(l)) break;
	  A((g + d) % ht->hashsiz != h);
     }

     fill_slot(ht, s, flagsp, slvndx, l);
}

static void rehash(hashtab *ht, unsigned nsiz)
{
     unsigned osiz = ht->hashsiz, h;
     solution *osol = ht->solutions, *nsol;

     nsiz = (unsigned)X(next_prime)((INT)nsiz);
     nsol = (solution *)MALLOC(nsiz * sizeof(solution), HASHT);
     ++ht->nrehash;

     /* init new table */
     for (h = 0; h < nsiz; ++h) 
	  nsol[h].flags.hash_info = 0;

     /* install new table */
     ht->hashsiz = nsiz;
     ht->solutions = nsol;
     ht->nelem = 0;

     /* copy table */
     for (h = 0; h < osiz; ++h) {
	  solution *l = osol + h;
	  if (LIVEP(l))
	       hinsert0(ht, l->s, &l->flags, SLVNDX(l));
     }

     X(ifree0)(osol);
}

static unsigned minsz(unsigned nelem)
{
     return 1U + nelem + nelem / 8U;
}

static unsigned nextsz(unsigned nelem)
{
     return minsz(minsz(nelem));
}

static void hgrow(hashtab *ht)
{
     unsigned nelem = ht->nelem;
     if (minsz(nelem) >= ht->hashsiz)
	  rehash(ht, nextsz(nelem));
}

#if 0
/* shrink the hash table, never used */
static void hshrink(hashtab *ht)
{
     unsigned nelem = ht->nelem;
     /* always rehash after deletions */
     rehash(ht, nextsz(nelem));
}
#endif

static void htab_insert(hashtab *ht, const md5sig s, const flags_t *flagsp,
			unsigned slvndx)
{
     unsigned g, h = h1(ht, s), d = h2(ht, s);
     solution *first = 0;

     /* Remove all entries that are subsumed by the new one.  */
     /* This loop may potentially traverse the whole table, since at
	least one element is guaranteed to be !LIVEP, but all elements
	may be VALIDP.  Hence, we stop after at the first invalid
	element or after traversing the whole table. */
     g = h;
     do {
	  solution *l = ht->solutions + g;
	  ++ht->insert_iter;
	  if (VALIDP(l)) {
	       if (LIVEP(l) && md5eq(s, l->s)) {
		    if (subsumes(flagsp, slvndx, &l->flags)) {
			 if (!first) first = l;
			 kill_slot(ht, l);
		    } else {
			 /* It is an error to insert an element that
			    is subsumed by an existing entry. */
			 A(!subsumes(&l->flags, SLVNDX(l), flagsp));
		    }
	       }
	  } else 
	       break;

	  g = addmod(g, d, ht->hashsiz);
     } while (g != h);

     if (first) {
	  /* overwrite FIRST */
	  fill_slot(ht, s, flagsp, slvndx, first);
     } else {
	  /* create a new entry */
 	  hgrow(ht);
	  hinsert0(ht, s, flagsp, slvndx);
     }
}

static void hinsert(planner *ego, const md5sig s, const flags_t *flagsp, 
		    unsigned slvndx)
{
     htab_insert(BLISS(*flagsp) ? &ego->htab_blessed : &ego->htab_unblessed,
		 s, flagsp, slvndx );
}


static void invoke_hook(planner *ego, plan *pln, const problem *p, 
			int optimalp)
{
     if (ego->hook)
	  ego->hook(ego, pln, p, optimalp);
}

#ifdef FFTW_RANDOM_ESTIMATOR
/* a "random" estimate, used for debugging to generate "random"
   plans, albeit from a deterministic seed. */

unsigned X(random_estimate_seed) = 0;

static double random_estimate(const planner *ego, const plan *pln, 
			      const problem *p)
{
     md5 m;
     X(md5begin)(&m);
     X(md5unsigned)(&m, X(random_estimate_seed));
     X(md5int)(&m, ego->nthr);
     p->adt->hash(p, &m);
     X(md5putb)(&m, &pln->ops, sizeof(pln->ops));
     X(md5putb)(&m, &pln->adt, sizeof(pln->adt));
     X(md5end)(&m);
     return ego->cost_hook ? ego->cost_hook(p, m.s[0], COST_MAX) : m.s[0];
}

#endif

double X(iestimate_cost)(const planner *ego, const plan *pln, const problem *p)
{
     double cost =
	  + pln->ops.add
	  + pln->ops.mul
	  
#if HAVE_FMA
	  + pln->ops.fma
#else
	  + 2 * pln->ops.fma
#endif
	  
	  + pln->ops.other;
     if (ego->cost_hook)
	  cost = ego->cost_hook(p, cost, COST_MAX);
     return cost;
}

static void evaluate_plan(planner *ego, plan *pln, const problem *p)
{
     if (ESTIMATEP(ego) || !BELIEVE_PCOSTP(ego) || pln->pcost == 0.0) {
	  ego->nplan++;

	  if (ESTIMATEP(ego)) {
	  estimate:
	       /* heuristic */
#ifdef FFTW_RANDOM_ESTIMATOR
	       pln->pcost = random_estimate(ego, pln, p);
	       ego->epcost += X(iestimate_cost)(ego, pln, p);
#else
	       pln->pcost = X(iestimate_cost)(ego, pln, p);
	       ego->epcost += pln->pcost;
#endif
	  } else {
	       double t = X(measure_execution_time)(ego, pln, p);
	       
	       if (t < 0) {  /* unavailable cycle counter */
		    /* Real programmers can write FORTRAN in any language */
		    goto estimate;
	       }

	       pln->pcost = t;
	       ego->pcost += t;
	       ego->need_timeout_check = 1;
	  }
     }
     
     invoke_hook(ego, pln, p, 0);
}

/* maintain dynamic scoping of flags, nthr: */
static plan *invoke_solver(planner *ego, const problem *p, solver *s, 
			   const flags_t *nflags)
{
     flags_t flags = ego->flags;
     int nthr = ego->nthr;
     plan *pln;
     ego->flags = *nflags;
     PLNR_TIMELIMIT_IMPATIENCE(ego) = 0;
     A(p->adt->problem_kind == s->adt->problem_kind);
     pln = s->adt->mkplan(s, p, ego);
     ego->nthr = nthr;
     ego->flags = flags;
     return pln;
}

/* maintain the invariant TIMED_OUT ==> NEED_TIMEOUT_CHECK */
static int timeout_p(planner *ego, const problem *p)
{
     /* do not timeout when estimating.  First, the estimator is the
	planner of last resort.  Second, calling X(elapsed_since)() is
	slower than estimating */
     if (!ESTIMATEP(ego)) {
	  /* do not assume that X(elapsed_since)() is monotonic */
	  if (ego->timed_out) {
	       A(ego->need_timeout_check);
	       return 1;
	  }

	  if (ego->timelimit >= 0 &&
	      X(elapsed_since)(ego, p, ego->start_time) >= ego->timelimit) {
	       ego->timed_out = 1;
	       ego->need_timeout_check = 1;
	       return 1;
	  }
     }

     A(!ego->timed_out);
     ego->need_timeout_check = 0;
     return 0;
}

static plan *search0(planner *ego, const problem *p, unsigned *slvndx, 
		     const flags_t *flagsp)
{
     plan *best = 0;
     int best_not_yet_timed = 1;

     /* Do not start a search if the planner timed out. This check is
	necessary, lest the relaxation mechanism kick in */
     if (timeout_p(ego, p))
	  return 0;

     FORALL_SOLVERS_OF_KIND(p->adt->problem_kind, ego, s, sp, {
	  plan *pln;

	  pln = invoke_solver(ego, p, s, flagsp);

	  if (ego->need_timeout_check) 
	       if (timeout_p(ego, p)) {
		    X(plan_destroy_internal)(pln);
		    X(plan_destroy_internal)(best);
		    return 0;
	       }

	  if (pln) {
	       /* read COULD_PRUNE_NOW_P because PLN may be destroyed
		  before we use COULD_PRUNE_NOW_P */
	       int could_prune_now_p = pln->could_prune_now_p;

	       if (best) {
		    if (best_not_yet_timed) {
			 evaluate_plan(ego, best, p);
			 best_not_yet_timed = 0;
		    }
		    evaluate_plan(ego, pln, p);
		    if (pln->pcost < best->pcost) {
			 X(plan_destroy_internal)(best);
			 best = pln;
                         *slvndx = (unsigned)/*from ptrdiff_t*/(sp - ego->slvdescs);
		    } else {
			 X(plan_destroy_internal)(pln);
		    }
	       } else {
		    best = pln;
                    *slvndx = (unsigned)/*from ptrdiff_t*/(sp - ego->slvdescs);                    
	       }

	       if (ALLOW_PRUNINGP(ego) && could_prune_now_p) 
		    break;
	  }
     });

     return best;
}

static plan *search(planner *ego, const problem *p, unsigned *slvndx, 
		    flags_t *flagsp)
{
     plan *pln = 0;
     unsigned i;

     /* relax impatience in this order: */
     static const unsigned relax_tab[] = {
	  0, /* relax nothing */
	  NO_VRECURSE,
	  NO_FIXED_RADIX_LARGE_N,
	  NO_SLOW,
	  NO_UGLY
     };

     unsigned l_orig = flagsp->l;
     unsigned x = flagsp->u;

     /* guaranteed to be different from X */
     unsigned last_x = ~x; 

     for (i = 0; i < sizeof(relax_tab) / sizeof(relax_tab[0]); ++i) {
	  if (LEQ(l_orig, x & ~relax_tab[i]))
	       x = x & ~relax_tab[i];

	  if (x != last_x) {
	       last_x = x;
	       flagsp->l = x;
	       pln = search0(ego, p, slvndx, flagsp);
	       if (pln) break;
	  }
     }

     if (!pln) {
	  /* search [L_ORIG, U] */
	  if (l_orig != last_x) {
	       last_x = l_orig;
	       flagsp->l = l_orig;
	       pln = search0(ego, p, slvndx, flagsp);
	  }
     }

     return pln;
}

#define CHECK_FOR_BOGOSITY						\
     if ((ego->bogosity_hook ?						\
	  (ego->wisdom_state = ego->bogosity_hook(ego->wisdom_state, p)) \
	  : ego->wisdom_state) == WISDOM_IS_BOGUS)			\
	  goto wisdom_is_bogus;

static plan *mkplan(planner *ego, const problem *p)
{
     plan *pln;
     md5 m;
     unsigned slvndx;
     flags_t flags_of_solution;
     solution *sol;
     solver *s;

     ASSERT_ALIGNED_DOUBLE;
     A(LEQ(PLNR_L(ego), PLNR_U(ego)));

     if (ESTIMATEP(ego))
	  PLNR_TIMELIMIT_IMPATIENCE(ego) = 0; /* canonical form */


#ifdef FFTW_DEBUG
     check(&ego->htab_blessed);
     check(&ego->htab_unblessed);
#endif

     pln = 0;

     CHECK_FOR_BOGOSITY;

     ego->timed_out = 0;

     ++ego->nprob;
     md5hash(&m, p, ego);

     flags_of_solution = ego->flags;

     if (ego->wisdom_state != WISDOM_IGNORE_ALL) {
	  if ((sol = hlookup(ego, m.s, &flags_of_solution))) { 
	       /* wisdom is acceptable */
	       wisdom_state_t owisdom_state = ego->wisdom_state;
	       
	       /* this hook is mainly for MPI, to make sure that
		  wisdom is in sync across all processes for MPI problems */
	       if (ego->wisdom_ok_hook && !ego->wisdom_ok_hook(p, sol->flags))
		    goto do_search; /* ignore not-ok wisdom */
	       
	       slvndx = SLVNDX(sol);
	       
	       if (slvndx == INFEASIBLE_SLVNDX) {
		    if (ego->wisdom_state == WISDOM_IGNORE_INFEASIBLE)
			 goto do_search;
		    else
			 return 0;   /* known to be infeasible */
	       }
	       
	       flags_of_solution = sol->flags;
	       
	       /* inherit blessing either from wisdom
		  or from the planner */
	       flags_of_solution.hash_info |= BLISS(ego->flags);
	       
	       ego->wisdom_state = WISDOM_ONLY;
	       
	       s = ego->slvdescs[slvndx].slv;
	       if (p->adt->problem_kind != s->adt->problem_kind)
		    goto wisdom_is_bogus;
	       
	       pln = invoke_solver(ego, p, s, &flags_of_solution);
	       
	       CHECK_FOR_BOGOSITY; 	  /* catch error in child solvers */
	       
	       sol = 0; /* Paranoia: SOL may be dangling after
			   invoke_solver(); make sure we don't accidentally
			   reuse it. */
	       
	       if (!pln)
		    goto wisdom_is_bogus;
	       
	       ego->wisdom_state = owisdom_state;
	       
	       goto skip_search;
	  }
	  else if (ego->nowisdom_hook) /* for MPI, make sure lack of wisdom */
	       ego->nowisdom_hook(p);  /*   is in sync across all processes */
     }

 do_search:
     /* cannot search in WISDOM_ONLY mode */
     if (ego->wisdom_state == WISDOM_ONLY)
	  goto wisdom_is_bogus;

     flags_of_solution = ego->flags;
     pln = search(ego, p, &slvndx, &flags_of_solution);
     CHECK_FOR_BOGOSITY; 	  /* catch error in child solvers */

     if (ego->timed_out) {
	  A(!pln);
	  if (PLNR_TIMELIMIT_IMPATIENCE(ego) != 0) {
	       /* record (below) that this plan has failed because of
		  timeout */
	       flags_of_solution.hash_info |= BLESSING;
	  } else {
	       /* this is not the top-level problem or timeout is not
		  active: record no wisdom. */
	       return 0;
	  }
     } else {
	  /* canonicalize to infinite timeout */
	  flags_of_solution.timelimit_impatience = 0;
     }

 skip_search:
     if (ego->wisdom_state == WISDOM_NORMAL ||
	 ego->wisdom_state == WISDOM_ONLY) {
	  if (pln) {
	       hinsert(ego, m.s, &flags_of_solution, slvndx);
	       invoke_hook(ego, pln, p, 1);
	  } else {
	       hinsert(ego, m.s, &flags_of_solution, INFEASIBLE_SLVNDX);
	  }
     }

     return pln;

 wisdom_is_bogus:
     X(plan_destroy_internal)(pln);
     ego->wisdom_state = WISDOM_IS_BOGUS;
     return 0;
}

static void htab_destroy(hashtab *ht)
{
     X(ifree)(ht->solutions);
     ht->solutions = 0;
     ht->nelem = 0U;
}

static void mkhashtab(hashtab *ht)
{
     ht->nrehash = 0;
     ht->succ_lookup = ht->lookup = ht->lookup_iter = 0;
     ht->insert = ht->insert_iter = ht->insert_unknown = 0;

     ht->solutions = 0;
     ht->hashsiz = ht->nelem = 0U;
     hgrow(ht);			/* so that hashsiz > 0 */
}

/* destroy hash table entries.  If FORGET_EVERYTHING, destroy the whole
   table.  If FORGET_ACCURSED, then destroy entries that are not blessed. */
static void forget(planner *ego, amnesia a)
{
     switch (a) {
	 case FORGET_EVERYTHING:
	      htab_destroy(&ego->htab_blessed);
	      mkhashtab(&ego->htab_blessed);
	      /* fall through */
	 case FORGET_ACCURSED:
	      htab_destroy(&ego->htab_unblessed);
	      mkhashtab(&ego->htab_unblessed);
	      break;
	 default:
	      break;
     }
}

/* FIXME: what sort of version information should we write? */
#define WISDOM_PREAMBLE PACKAGE "-" VERSION " " STRINGIZE(X(wisdom))
static const char stimeout[] = "TIMEOUT";

/* tantus labor non sit cassus */
static void exprt(planner *ego, printer *p)
{
     unsigned h;
     hashtab *ht = &ego->htab_blessed;
     md5 m;

     signature_of_configuration(&m, ego);

     p->print(p, 
	      "(" WISDOM_PREAMBLE " #x%M #x%M #x%M #x%M\n",
	      m.s[0], m.s[1], m.s[2], m.s[3]);

     for (h = 0; h < ht->hashsiz; ++h) {
	  solution *l = ht->solutions + h;
	  if (LIVEP(l)) {
	       const char *reg_nam;
	       int reg_id;

	       if (SLVNDX(l) == INFEASIBLE_SLVNDX) {
		    reg_nam = stimeout;
		    reg_id = 0;
	       } else {
		    slvdesc *sp = ego->slvdescs + SLVNDX(l);
		    reg_nam = sp->reg_nam;
		    reg_id = sp->reg_id;
	       }

	       /* qui salvandos salvas gratis
		  salva me fons pietatis */
	       p->print(p, "  (%s %d #x%x #x%x #x%x #x%M #x%M #x%M #x%M)\n",
			reg_nam, reg_id, 
			l->flags.l, l->flags.u, l->flags.timelimit_impatience, 
			l->s[0], l->s[1], l->s[2], l->s[3]);
	  }
     }
     p->print(p, ")\n");
}

/* mors stupebit et natura
   cum resurget creatura */
static int imprt(planner *ego, scanner *sc)
{
     char buf[MAXNAM + 1];
     md5uint sig[4];
     unsigned l, u, timelimit_impatience;
     flags_t flags;
     int reg_id;
     unsigned slvndx;
     hashtab *ht = &ego->htab_blessed;
     hashtab old;
     md5 m;

     if (!sc->scan(sc, 
		   "(" WISDOM_PREAMBLE " #x%M #x%M #x%M #x%M\n",
		   sig + 0, sig + 1, sig + 2, sig + 3))
	  return 0; /* don't need to restore hashtable */

     signature_of_configuration(&m, ego);
     if (m.s[0] != sig[0] || m.s[1] != sig[1] ||
	 m.s[2] != sig[2] || m.s[3] != sig[3]) {
	  /* invalid configuration */
	  return 0;
     }
     
     /* make a backup copy of the hash table (cache the hash) */
     {
	  unsigned h, hsiz = ht->hashsiz;
	  old = *ht;
	  old.solutions = (solution *)MALLOC(hsiz * sizeof(solution), HASHT);
	  for (h = 0; h < hsiz; ++h)
	       old.solutions[h] = ht->solutions[h];
     }

     while (1) {
	  if (sc->scan(sc, ")"))
	       break;

	  /* qua resurget ex favilla */
	  if (!sc->scan(sc, "(%*s %d #x%x #x%x #x%x #x%M #x%M #x%M #x%M)",
			MAXNAM, buf, &reg_id, &l, &u, &timelimit_impatience,
			sig + 0, sig + 1, sig + 2, sig + 3))
	       goto bad;

	  if (!strcmp(buf, stimeout) && reg_id == 0) {
	       slvndx = INFEASIBLE_SLVNDX;
	  } else {
	       if (timelimit_impatience != 0)
		    goto bad;

	       slvndx = slookup(ego, buf, reg_id);
	       if (slvndx == INFEASIBLE_SLVNDX)
		    goto bad;
	  }

	  /* inter oves locum praesta */
	  flags.l = l;
	  flags.u = u;
	  flags.timelimit_impatience = timelimit_impatience;
	  flags.hash_info = BLESSING;

	  CK(flags.l == l);
	  CK(flags.u == u);
	  CK(flags.timelimit_impatience == timelimit_impatience);

	  if (!hlookup(ego, sig, &flags))
	       hinsert(ego, sig, &flags, slvndx);
     }

     X(ifree0)(old.solutions);
     return 1;

 bad:
     /* ``The wisdom of FFTW must be above suspicion.'' */
     X(ifree0)(ht->solutions);
     *ht = old;
     return 0;
}

/*
 * create a planner
 */
planner *X(mkplanner)(void)
{
     int i;

     static const planner_adt padt = {
	  register_solver, mkplan, forget, exprt, imprt
     };

     planner *p = (planner *) MALLOC(sizeof(planner), PLANNERS);

     p->adt = &padt;
     p->nplan = p->nprob = 0;
     p->pcost = p->epcost = 0.0;
     p->hook = 0;
     p->cost_hook = 0;
     p->wisdom_ok_hook = 0;
     p->nowisdom_hook = 0;
     p->bogosity_hook = 0;
     p->cur_reg_nam = 0;
     p->wisdom_state = WISDOM_NORMAL;

     p->slvdescs = 0;
     p->nslvdesc = p->slvdescsiz = 0;

     p->flags.l = 0;
     p->flags.u = 0;
     p->flags.timelimit_impatience = 0;
     p->flags.hash_info = 0;
     p->nthr = 1;
     p->need_timeout_check = 1;
     p->timelimit = -1;

     mkhashtab(&p->htab_blessed);
     mkhashtab(&p->htab_unblessed);

     for (i = 0; i < PROBLEM_LAST; ++i)
	  p->slvdescs_for_problem_kind[i] = -1;

     return p;
}

void X(planner_destroy)(planner *ego)
{
     /* destroy hash table */
     htab_destroy(&ego->htab_blessed);
     htab_destroy(&ego->htab_unblessed);

     /* destroy solvdesc table */
     FORALL_SOLVERS(ego, s, sp, {
	  UNUSED(sp);
	  X(solver_destroy)(s);
     });

     X(ifree0)(ego->slvdescs);
     X(ifree)(ego); /* dona eis requiem */
}

plan *X(mkplan_d)(planner *ego, problem *p)
{
     plan *pln = ego->adt->mkplan(ego, p);
     X(problem_destroy)(p);
     return pln;
}

/* like X(mkplan_d), but sets/resets flags as well */
plan *X(mkplan_f_d)(planner *ego, problem *p, 
		    unsigned l_set, unsigned u_set, unsigned u_reset)
{
     flags_t oflags = ego->flags;
     plan *pln;

     PLNR_U(ego) &= ~u_reset;
     PLNR_L(ego) &= ~u_reset;
     PLNR_L(ego) |= l_set;
     PLNR_U(ego) |= u_set | l_set;
     pln = X(mkplan_d)(ego, p);
     ego->flags = oflags;
     return pln;
}

/*
 * Debugging code:
 */
#ifdef FFTW_DEBUG
static void check(hashtab *ht)
{
     unsigned live = 0;
     unsigned i;

     A(ht->nelem < ht->hashsiz);

     for (i = 0; i < ht->hashsiz; ++i) {
	  solution *l = ht->solutions + i; 
	  if (LIVEP(l)) 
	       ++live; 
     }

     A(ht->nelem == live);

     for (i = 0; i < ht->hashsiz; ++i) {
	  solution *l1 = ht->solutions + i; 
	  int foundit = 0;
	  if (LIVEP(l1)) {
	       unsigned g, h = h1(ht, l1->s), d = h2(ht, l1->s);

	       g = h;
	       do {
		    solution *l = ht->solutions + g;
		    if (VALIDP(l)) {
			 if (l1 == l)
			      foundit = 1;
			 else if (LIVEP(l) && md5eq(l1->s, l->s)) {
			      A(!subsumes(&l->flags, SLVNDX(l), &l1->flags));
			      A(!subsumes(&l1->flags, SLVNDX(l1), &l->flags));
			 }
		    } else 
			 break;
		    g = addmod(g, d, ht->hashsiz);
	       } while (g != h);

	       A(foundit);
	  }
     }
}
#endif
