/* Re-use libbench2 and the test program, but override bench_main so that
   we can have different command-line syntax. */
#include "libbench2/my-getopt.h"
#include "libbench2/bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "api/fftw3.h"
#include <string.h>
#include <time.h>

#if defined(HAVE_THREADS) || defined(HAVE_OPENMP)
#  define HAVE_SMP
   extern int threads_ok;
#endif

#define CONCAT(prefix, name) prefix ## name
#if defined(BENCHFFT_SINGLE)
#define FFTW(x) CONCAT(fftwf_, x)
#elif defined(BENCHFFT_LDOUBLE)
#define FFTW(x) CONCAT(fftwl_, x)
#elif defined(BENCHFFT_QUAD)
#define FFTW(x) CONCAT(fftwq_, x)
#else
#define FFTW(x) CONCAT(fftw_, x)
#endif

/* from bench.c: */
extern unsigned the_flags;
extern int usewisdom;
extern int nthreads;

/* dummy routines to replace those in hook.c */
void install_hook(void) {}
void uninstall_hook(void) {}

int verbose;

static void do_problem(bench_problem *p)
{
     if (verbose)
	  printf("Planning transform: %s\n", p->pstring);
     /* BENCH_ASSERT(can_do(p)); */
     problem_alloc(p);
     setup(p);
     done(p);
}

static void add_problem(const char *pstring,
			bench_problem ***p, int *ip, int *np)
{
     if (*ip >= *np) {
	  *np = *np * 2 + 1;
	  *p = (bench_problem **) realloc(*p, sizeof(bench_problem *) * *np);
     }
     (*p)[(*ip)++] = problem_parse(pstring);
}

static int sz(const bench_problem *p)
{
     return tensor_sz(p->sz) * tensor_sz(p->vecsz);
}

static int prob_size_cmp(const void *p1_, const void *p2_)
{
     const bench_problem * const *p1 = (const bench_problem * const *) p1_;
     const bench_problem * const *p2 = (const bench_problem * const *) p2_;
     return (sz(*p1) - sz(*p2));
}

static struct my_option options[] =
{
  {"help", NOARG, 'h'},
  {"version", NOARG, 'V'},
  {"verbose", NOARG, 'v'},

  {"canonical", NOARG, 'c'},
  {"time-limit", REQARG, 't'},

  {"output-file", REQARG, 'o'},

  {"impatient", NOARG, 'i'},
  {"measure", NOARG, 'm'},
  {"estimate", NOARG, 'e'},
  {"exhaustive", NOARG, 'x'},

  {"no-system-wisdom", NOARG, 'n'},
  {"wisdom-file", REQARG, 'w'},

#ifdef HAVE_SMP
  {"threads", REQARG, 'T'},
#endif

  /* options to restrict configuration to rdft-only, etcetera? */
  
  {0, NOARG, 0}
};

static void help(FILE *f, const char *program_name)
{
     fprintf(
	  f, 
	  "Usage: %s [options] [sizes]\n"
"    Create wisdom (pre-planned/optimized transforms) for specified sizes,\n"
"    writing wisdom to stdout (or to a file, using -o).\n"
	  "\nOptions:\n"
 "                   -h, --help: print this help\n"
 "                -V, --version: print version/copyright info\n"
 "                -v, --verbose: verbose output\n"
 "              -c, --canonical: plan/optimize canonical set of sizes\n"
 "     -t <h>, --time-limit=<h>: time limit in hours (default: 0, no limit)\n"
 "  -o FILE, --output-file=FILE: output to FILE instead of stdout\n"
 "                -m, --measure: plan in MEASURE mode (PATIENT is default)\n"
 "               -e, --estimate: plan in ESTIMATE mode (not recommended)\n"
 "             -x, --exhaustive: plan in EXHAUSTIVE mode (may be slow)\n"
 "       -n, --no-system-wisdom: don't read /etc/fftw/ system wisdom file\n"
 "  -w FILE, --wisdom-file=FILE: read wisdom from FILE (stdin if -)\n"
#ifdef HAVE_SMP
 "            -T N, --threads=N: plan with N threads\n"
#endif
	  "\nSize syntax: <type><inplace><direction><geometry>\n"
 "      <type> = c/r/k for complex/real(r2c,c2r)/r2r\n" 
 "   <inplace> = i/o for in/out-of place\n"
 " <direction> = f/b for forward/backward, omitted for k transforms\n"
 "  <geometry> = <n1>[x<n2>[x...]], e.g. 10x12x14\n"
 "               -- for k transforms, after each dimension is a <kind>:\n"
 "                     <kind> = f/b/h/e00/e01/e10/e11/o00/o01/o10/o11\n"
 "                              for R2HC/HC2R/DHT/REDFT00/.../RODFT11\n"
	  , program_name);
}

/* powers of two and ten up to 2^20, for now */
static char canonical_sizes[][32] = {
     "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024",
     "2048", "4096", "8192", "16384", "32768", "65536", "131072",
     "262144", "524288", "1048576",

     "10", "100", "1000", "10000", "100000", "1000000",

     "2x2", "4x4", "8x8", "10x10", "16x16", "32x32", "64x64", "100x100",
     "128x128", "256x256", "512x512", "1000x1000", "1024x1024",

     "2x2x2", "4x4x4", "8x8x8", "10x10x10", "16x16x16", "32x32x32",
     "64x64x64", "100x100x100"
};

#define NELEM(array)(sizeof(array) / sizeof((array)[0]))

int bench_main(int argc, char *argv[])
{
     int c;
     unsigned i;
     int impatient = 0;
     int system_wisdom = 1;
     int canonical = 0;
     double hours = 0;
     FILE *output_file;
     char *output_fname = 0;
     bench_problem **problems = 0;
     int nproblems = 0, iproblem = 0;
     time_t begin;

     verbose = 0;
     usewisdom = 0;

     bench_srand(1);
#ifdef HAVE_SMP
     /* do not configure FFTW with threads, unless the
	user requests -T */
     threads_ok = 0;
#endif

     while ((c = my_getopt(argc, argv, options)) != -1) {
	  switch (c) {
	      case 'h':
		   help(stdout, argv[0]);
		   exit(EXIT_SUCCESS);
		   break;

	      case 'V':
		   printf("fftw-wisdom tool for FFTW version " VERSION ".\n");
		   printf(
"\n"
"Copyright (c) 2003, 2007-14 Matteo Frigo\n"
"Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology\n"
"\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA\n"
			);
		   exit(EXIT_SUCCESS);
		   break;
		   
	      case 'v':
		   verbose = 1;
		   break;
		   
	      case 'c':
		   canonical = 1;
		   break;

	      case 't':
		   hours = atof(my_optarg);
		   break;

	      case 'o':
		   if (output_fname)
			bench_free(output_fname);
		   
		   if (!strcmp(my_optarg, "-"))
			output_fname = 0;
		   else {
			output_fname = (char *) bench_malloc(sizeof(char) *
						    (strlen(my_optarg) + 1));
			strcpy(output_fname, my_optarg);
		   }
		   break;

	      case 'm':
	      case 'i':
		   impatient = 1;
		   break;

	      case 'e':
		   the_flags |= FFTW_ESTIMATE;
		   break;

	      case 'x':
		   the_flags |= FFTW_EXHAUSTIVE;
		   break;

	      case 'n':
		   system_wisdom = 0;
		   break;

	      case 'w': {
		   FILE *w = stdin;
		   if (strcmp(my_optarg, "-") && !(w = fopen(my_optarg, "r"))) {
			fprintf(stderr,
				"fftw-wisdom: error opening \"%s\": ", my_optarg);
			perror("");
			exit(EXIT_FAILURE);
		   }
		   if (!FFTW(import_wisdom_from_file)(w)) {
			fprintf(stderr, "fftw_wisdom: error reading wisdom "
				"from \"%s\"\n", my_optarg);
			exit(EXIT_FAILURE);
		   }
		   if (w != stdin)
			fclose(w);
		   break;
	      }

#ifdef HAVE_SMP
	      case 'T':
		   nthreads = atoi(my_optarg);
		   if (nthreads < 1) nthreads = 1;
		   threads_ok = 1;
		   BENCH_ASSERT(FFTW(init_threads)());
		   break;
#endif

	      case '?':
		   /* `my_getopt' already printed an error message. */
		   cleanup();
		   return EXIT_FAILURE;

	      default:
		   abort ();
	  }
     }

     if (!impatient)
	  the_flags |= FFTW_PATIENT;

     if (system_wisdom)
	  if (!FFTW(import_system_wisdom)() && verbose)
	       fprintf(stderr, "fftw-wisdom: system-wisdom import failed\n");

     if (canonical) {
	  for (i = 0; i < NELEM(canonical_sizes); ++i) {
	       unsigned j;
	       char types[][8] = { 
		    "cof", "cob", "cif", "cib", "rof", "rob", "rif", "rib"
	       };
	       
	       for (j = 0; j < NELEM(types); ++j) {
		    char ps[64];
		    if (!strchr(canonical_sizes[i],'x')
			|| !strchr(types[j],'o')) {
#ifdef HAVE_SNPRINTF
			 snprintf(ps, sizeof(ps), "%s%s", types[j], canonical_sizes[i]);
#else
			 sprintf(ps, "%s%s", types[j], canonical_sizes[i]);
#endif
			 add_problem(ps, &problems, &iproblem, &nproblems);
		    }
	       }
	  }
     }

     while (my_optind < argc) {
	  if (!strcmp(argv[my_optind], "-")) {
	       char s[1025];
	       while (1 == fscanf(stdin, "%1024s", s))
		    add_problem(s, &problems, &iproblem, &nproblems);
	  }
	  else
	       add_problem(argv[my_optind], &problems, &iproblem, &nproblems);
	  ++my_optind;
     }

     nproblems = iproblem;
     qsort(problems, nproblems, sizeof(bench_problem *), prob_size_cmp);

     if (!output_fname)
	  output_file = stdout;
     else
	  if (!(output_file = fopen(output_fname, "w"))) {
	       fprintf(stderr,
		       "fftw-wisdom: error creating \"%s\"", output_fname);
	       perror("");
	       exit(EXIT_FAILURE);
	  }

     begin = time((time_t*)0);
     for (iproblem = 0; iproblem < nproblems; ++iproblem) {
	  if (hours <= 0
	      || hours > (time((time_t*)0) - begin) / 3600.0)
	       do_problem(problems[iproblem]);
	  problem_destroy(problems[iproblem]);
	  
     }
     free(problems);
     
     if (verbose && hours > 0
	 && hours < (time((time_t*)0) - begin) / 3600.0)
	  fprintf(stderr, "EXCEEDED TIME LIMIT OF %g HOURS.\n", hours);

     FFTW(export_wisdom_to_file)(output_file);
     if (output_file != stdout)
	  fclose(output_file);
     if (output_fname)
	  bench_free(output_fname);

     cleanup();

     return EXIT_SUCCESS;
}
