/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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


#include "libbench2/bench.h"
#include "my-getopt.h"
#include <stdio.h>
#include <stdlib.h>

int verbose;

static const struct my_option options[] =
{
  {"accuracy", REQARG, 'a'},
  {"accuracy-rounds", REQARG, 405},
  {"impulse-accuracy-rounds", REQARG, 406},
  {"can-do", REQARG, 'd'},
  {"help", NOARG, 'h'},
  {"info", REQARG, 'i'},
  {"info-all", NOARG, 'I'},
  {"print-precision", NOARG, 402},
  {"print-time-min", NOARG, 400},
  {"random-seed", REQARG, 404},
  {"report-benchmark", NOARG, 320},
  {"report-mflops", NOARG, 300},
  {"report-time", NOARG, 310},
  {"report-verbose", NOARG, 330},
  {"speed", REQARG, 's'},
  {"setup-speed", REQARG, 'S'},
  {"time-min", REQARG, 't'},
  {"time-repeat", REQARG, 'r'},
  {"user-option", REQARG, 'o'},
  {"verbose", OPTARG, 'v'},
  {"verify", REQARG, 'y'},
  {"verify-rounds", REQARG, 401},
  {"verify-tolerance", REQARG, 403},
  {0, NOARG, 0}
};

int bench_main(int argc, char *argv[])
{
     double tmin = 0.0;
     double tol;
     int repeat = 0;
     int rounds = 10;
     int iarounds = 0;
     int arounds = 1; /* this is too low for precise results */
     int c;

     report = report_verbose; /* default */
     verbose = 0;

     tol = SINGLE_PRECISION ? 1.0e-3 : (QUAD_PRECISION ? 1e-29 : 1.0e-10);

     main_init(&argc, &argv);

     bench_srand(1);

     while ((c = my_getopt (argc, argv, options)) != -1) {
	  switch (c) {
	      case 't' :
		   tmin = strtod(my_optarg, 0);
		   break;
	      case 'r':
		   repeat = atoi(my_optarg);
		   break;
	      case 's':
		   timer_init(tmin, repeat);
		   speed(my_optarg, 0);
		   break;
	      case 'S':
		   timer_init(tmin, repeat);
		   speed(my_optarg, 1);
		   break;
	      case 'd':
		   report_can_do(my_optarg);
		   break;
	      case 'o':
		   useropt(my_optarg);
		   break;
	      case 'v':
		   if (verbose >= 0) { /* verbose < 0 disables output */
			if (my_optarg)
			     verbose = atoi(my_optarg);
			else
			     ++verbose;
		   }
		   break;
	      case 'y':
		   verify(my_optarg, rounds, tol);
		   break;
	      case 'a':
		   accuracy(my_optarg, arounds, iarounds);
		   break;
	      case 'i':
		   report_info(my_optarg);
		   break;
	      case 'I':
		   report_info_all();
		   break;
	      case 'h':
		   if (verbose >= 0) my_usage(argv[0], options);
		   break;

	      case 300: /* --report-mflops */
		   report = report_mflops;
		   break;

	      case 310: /* --report-time */
		   report = report_time;
		   break;

 	      case 320: /* --report-benchmark */
		   report = report_benchmark;
		   break;

 	      case 330: /* --report-verbose */
		   report = report_verbose;
		   break;

	      case 400: /* --print-time-min */
		   timer_init(tmin, repeat);
		   ovtpvt("%g\n", time_min);
		   break;

	      case 401: /* --verify-rounds */
		   rounds = atoi(my_optarg);
		   break;

	      case 402: /* --print-precision */
		   if (SINGLE_PRECISION)
			ovtpvt("single\n");
		   else if (QUAD_PRECISION)
			ovtpvt("quad\n");
		   else if (LDOUBLE_PRECISION)
			ovtpvt("long-double\n");
		   else if (DOUBLE_PRECISION)
			ovtpvt("double\n");
		   else 
			ovtpvt("unknown %d\n", sizeof(bench_real));
		   break;

	      case 403: /* --verify-tolerance */
		   tol = strtod(my_optarg, 0);
		   break;

	      case 404: /* --random-seed */
		   bench_srand(atoi(my_optarg));
		   break;

	      case 405: /* --accuracy-rounds */
		   arounds = atoi(my_optarg);
		   break;
		   
	      case 406: /* --impulse-accuracy-rounds */
		   iarounds = atoi(my_optarg);
		   break;
		   
	      case '?':
		   /* my_getopt() already printed an error message. */
		   cleanup();
		   return 1;

	      default:
		   abort ();
	  }
     }

     /* assume that any remaining arguments are problems to be
        benchmarked */
     while (my_optind < argc) {
	  timer_init(tmin, repeat);
	  speed(argv[my_optind++], 0);
     }

     cleanup();
     return 0;
}
