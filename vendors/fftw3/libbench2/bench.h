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


/* benchmark program definitions */
#include "libbench2/bench-user.h"

extern double time_min;
extern int time_repeat;

extern void timer_init(double tmin, int repeat);

/* report functions */
extern void (*report)(const bench_problem *p, double *t, int st);

void report_mflops(const bench_problem *p, double *t, int st);
void report_time(const bench_problem *p, double *t, int st);
void report_benchmark(const bench_problem *p, double *t, int st);
void report_verbose(const bench_problem *p, double *t, int st);

void report_can_do(const char *param);
void report_info(const char *param);
void report_info_all(void);

extern int aligned_main(int argc, char *argv[]);
extern int bench_main(int argc, char *argv[]);

extern void speed(const char *param, int setup_only);
extern void accuracy(const char *param, int rounds, int impulse_rounds);

extern double mflops(const bench_problem *p, double t);

extern double bench_drand(void);
extern void bench_srand(int seed);

extern bench_problem *problem_parse(const char *desc);

extern void ovtpvt(const char *format, ...);
extern void ovtpvt_err(const char *format, ...);

extern void fftaccuracy(int n, bench_complex *a, bench_complex *ffta,
			int sign, double err[6]);
extern void fftaccuracy_done(void);

extern void caset(bench_complex *A, int n, bench_complex x);
extern void aset(bench_real *A, int n, bench_real x);
