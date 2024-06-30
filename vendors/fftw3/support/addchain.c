/* addition-chain optimizer */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int verbose;
static int mulcost = 18;
static int ldcost = 2;
static int sqcost = 10;
static int reflcost = 8;
#define INFTY 100000

static int *answer;
static int best_so_far;

static void print_answer(int n, int t)
{
     int i;
     printf("| (%d, %d) -> [", n, t);
     for (i = 0; i < t; ++i)
	  printf("%d;", answer[i]);
     printf("] (* %d *)\n", best_so_far);
}

#define DO(i, j, k, cst)			\
if (k < n) {					\
     int c = A[i] + A[j] + cst;			\
     if (c < A[k]) {				\
	  A[k] = c;				\
	  changed = 1;				\
     }						\
}

#define DO3(i, j, l, k, cst)			\
if (k < n) {					\
     int c = A[i] + A[j] + A[l] + cst;		\
     if (c < A[k]) {				\
	  A[k] = c;				\
	  changed = 1;				\
     }						\
}

static int optimize(int n, int *A)
{
     int i, j, k, changed, cst, cstmax;

     do {
	  changed = 0;
	  for (i = 0; i < n; ++i) {
	       k = i + i;
	       DO(i, i, k, sqcost);
	  }

	  for (i = 0; i < n; ++i) {
	       for (j = 0; j <= i; ++j) {
		    k = i + j;
		    DO(i, j, k, mulcost);
		    k = i - j;
		    DO(i, j, k, mulcost);

		    k = i + j;
		    DO3(i, j, i - j, k, reflcost);
	       }
	  }

     } while (changed);

     cst = cstmax = 0;
     for (i = 0; i < n; ++i) {
	  cst += A[i];
	  if (A[i] > cstmax) cstmax = A[i];
     }
/*     return cstmax; */
     return cst;
}

static void search(int n, int t, int *A, int *B, int depth)
{
     if (depth == 0) {
	  int i, tc;
	  for (i = 0; i < n; ++i)
	       A[i] = INFTY;
	  A[0] = 0;		/* always free */
	  for (i = 1; i <= t; ++i)
	       A[B[-i]] = ldcost;

	  tc = optimize(n, A);
	  if (tc < best_so_far) {
	       best_so_far = tc;
	       for (i = 1; i <= t; ++i)
		    answer[t - i] = B[-i];
	       if (verbose)
		    print_answer(n, t);
	  }
     } else {
	  for (B[0] = B[-1] + 1; B[0] < n; ++B[0])
	       search(n, t, A, B + 1, depth - 1);
     }
}

static void doit(int n, int t)
{
     int *A;
     int *B;

     A = malloc(n * sizeof(int));
     B = malloc((t + 1) * sizeof(int));
     answer = malloc(t * sizeof(int));

     B[0] = 0;
     best_so_far = INFTY;
     search(n, t, A, B + 1, t);

     print_answer(n, t);

     free(A); free(B); free(answer);
}

int main(int argc, char *argv[])
{
     int n = 32;
     int t = 3;
     int all;
     int ch;

     verbose = 0;
     all = 0;
     while ((ch = getopt(argc, argv, "n:t:m:l:r:s:va")) != -1) {
	  switch (ch) {
	  case 'n':
	       n = atoi(optarg);
	       break;
	  case 't':
	       t = atoi(optarg);
	       break;
	  case 'm':
	       mulcost = atoi(optarg);
	       break;
	  case 'l':
	       ldcost = atoi(optarg);
	       break;
	  case 's':
	       sqcost = atoi(optarg);
	       break;
	  case 'r':
	       reflcost = atoi(optarg);
	       break;
	  case 'v':
	       ++verbose;
	       break;
	  case 'a':
	       ++all;
	       break;
	  case '?':
	       fprintf(stderr, "use the source\n");
	       exit(1);
	  }
     }

     if (all) {
	  for (n = 4; n <= 64; n *= 2) {
	       int n1 = n - 1; if (n1 > 7) n1 = 7;
	       for (t = 1; t <= n1; ++t)
		    doit(n, t);
	  }
     } else {
	  doit(n, t);
     }

     return 0;
}
