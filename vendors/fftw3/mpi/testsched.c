/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 1999-2003, 2007-8 Massachusetts Institute of Technology
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

/**********************************************************************/
/* This is a modified and combined version of the sched.c and
   test_sched.c files shipped with FFTW 2, written to implement and
   test various all-to-all communications scheduling patterns.

   It is not used in FFTW 3, but I keep it around in case we ever want
   to play with this again or to change algorithms.  In particular, I
   used it to implement and test the fill1_comm_sched routine in
   transpose-pairwise.c, which allows us to create a schedule for one
   process at a time and is much more compact than the FFTW 2 code.

   Note that the scheduling algorithm is somewhat modified from that
   of FFTW 2.  Originally, I thought that one "stall" in the schedule
   was unavoidable for odd numbers of processes, since this is the
   case for the soccer-timetabling problem.  However, because of the
   self-communication step, we can use the self-communication to fill
   in the stalls.  (Thanks to Ralf Wildenhues for pointing this out.)
   This greatly simplifies the process re-sorting algorithm. */

/**********************************************************************/

#include <stdio.h>
#include <stdlib.h>

/* This file contains routines to compute communications schedules for
   all-to-all communications (complete exchanges) that are performed
   in-place.  (That is, the block that processor x sends to processor
   y gets replaced on processor x by a block received from processor y.)

   A schedule, int **sched, is a two-dimensional array where
   sched[pe][i] is the processor that pe expects to exchange a message
   with on the i-th step of the exchange.  sched[pe][i] == -1 for the
   i after the last exchange scheduled on pe.

   Here, processors (pe's, for processing elements), are numbered from
   0 to npes-1.

   There are a couple of constraints that a schedule should satisfy
   (besides the obvious one that every processor has to communicate
   with every other processor exactly once).
   
   * First, and most importantly, there must be no deadlocks.
   
   * Second, we would like to overlap communications as much as possible,
   so that all exchanges occur in parallel.  It turns out that perfect
   overlap is possible for all number of processes (npes).

   It turns out that this scheduling problem is actually well-studied,
   and good solutions are known.  The problem is known as a
   "time-tabling" problem, and is specifically the problem of
   scheduling a sports competition (where n teams must compete exactly
   once with every other team).  The problem is discussed and
   algorithms are presented in:

   [1] J. A. M. Schreuder, "Constructing Timetables for Sport
   Competitions," Mathematical Programming Study 13, pp. 58-67 (1980).

   [2] A. Schaerf, "Scheduling Sport Tournaments using Constraint
   Logic Programming," Proc. of 12th Europ. Conf. on
   Artif. Intell. (ECAI-96), pp. 634-639 (Budapest 1996).
   http://hermes.dis.uniromal.it/~aschaerf/publications.html

   (These people actually impose a lot of additional constraints that
   we don't care about, so they are solving harder problems. [1] gives
   a simple enough algorithm for our purposes, though.)
   
   In the timetabling problem, N teams can all play one another in N-1
   steps if N is even, and N steps if N is odd.  Here, however,
   there is a "self-communication" step (a team must also "play itself")
   and so we can always make an optimal N-step schedule regardless of N.

   However, we have to do more: for a particular processor, the
   communications schedule must be sorted in ascending or descending
   order of processor index.  (This is necessary so that the data
   coming in for the transpose does not overwrite data that will be
   sent later; for that processor the incoming and outgoing blocks are
   of different non-zero sizes.)  Fortunately, because the schedule
   is stall free, each parallel step of the schedule is independent
   of every other step, and we can reorder the steps arbitrarily
   to achieve any desired order on a particular process.
*/

void free_comm_schedule(int **sched, int npes)
{
     if (sched) {
	  int i;

	  for (i = 0; i < npes; ++i)
	       free(sched[i]);
	  free(sched);
     }
}

void empty_comm_schedule(int **sched, int npes)
{
     int i;
     for (i = 0; i < npes; ++i)
	  sched[i][0] = -1;
}

extern void fill_comm_schedule(int **sched, int npes);

/* Create a new communications schedule for a given number of processors.
   The schedule is initialized to a deadlock-free, maximum overlap
   schedule.  Returns NULL on an error (may print a message to
   stderr if there is a program bug detected).  */
int **make_comm_schedule(int npes)
{
     int **sched;
     int i;

     sched = (int **) malloc(sizeof(int *) * npes);
     if (!sched)
	  return NULL;

     for (i = 0; i < npes; ++i)
	  sched[i] = NULL;

     for (i = 0; i < npes; ++i) {
	  sched[i] = (int *) malloc(sizeof(int) * 10 * (npes + 1));
	  if (!sched[i]) {
	       free_comm_schedule(sched,npes);
	       return NULL;
	  }
     }
     
     empty_comm_schedule(sched,npes);
     fill_comm_schedule(sched,npes);

     if (!check_comm_schedule(sched,npes)) {
	  free_comm_schedule(sched,npes);
	  return NULL;
     }

     return sched;
}

static void add_dest_to_comm_schedule(int **sched, int pe, int dest)
{
     int i;
     
     for (i = 0; sched[pe][i] != -1; ++i)
	  ;

     sched[pe][i] = dest;
     sched[pe][i+1] = -1;
}

static void add_pair_to_comm_schedule(int **sched, int pe1, int pe2)
{
     add_dest_to_comm_schedule(sched, pe1, pe2);
     if (pe1 != pe2)
	  add_dest_to_comm_schedule(sched, pe2, pe1);
}

/* Simplification of algorithm presented in [1] (we have fewer
   constraints).  Produces a perfect schedule (npes steps).  */

void fill_comm_schedule(int **sched, int npes)
{
     int pe, i, n;

     if (npes % 2 == 0) {
	  n = npes;
	  for (pe = 0; pe < npes; ++pe)
	       add_pair_to_comm_schedule(sched,pe,pe);
     }
     else
	  n = npes + 1;

     for (pe = 0; pe < n - 1; ++pe) {
	  add_pair_to_comm_schedule(sched, pe, npes % 2 == 0 ? npes - 1 : pe);
	  
	  for (i = 1; i < n/2; ++i) {
	       int pe_a, pe_b;

	       pe_a = pe - i;
	       if (pe_a < 0)
		    pe_a += n - 1;

	       pe_b = (pe + i) % (n - 1);

	       add_pair_to_comm_schedule(sched,pe_a,pe_b);
	  }
     }
}

/* given an array sched[npes], fills it with the communications
   schedule for process pe. */
void fill1_comm_sched(int *sched, int which_pe, int npes)
{
     int pe, i, n, s = 0;
     if (npes % 2 == 0) {
	  n = npes;
	  sched[s++] = which_pe;
     }
     else
	  n = npes + 1;
     for (pe = 0; pe < n - 1; ++pe) {
	  if (npes % 2 == 0) {
	       if (pe == which_pe) sched[s++] = npes - 1;
	       else if (npes - 1 == which_pe) sched[s++] = pe;
	  }
	  else if (pe == which_pe) sched[s++] = pe;

	  if (pe != which_pe && which_pe < n - 1) {
	       i = (pe - which_pe + (n - 1)) % (n - 1);
	       if (i < n/2)
		    sched[s++] = (pe + i) % (n - 1);
	       
	       i = (which_pe - pe + (n - 1)) % (n - 1);
	       if (i < n/2)
		    sched[s++] = (pe - i + (n - 1)) % (n - 1);
	  }
     }
     if (s != npes) {
	  fprintf(stderr, "bug in fill1_com_schedule (%d, %d/%d)\n", 
		  s, which_pe, npes);
	  exit(EXIT_FAILURE);
     }
}

/* sort the communication schedule sched for npes so that the schedule
   on process sortpe is ascending or descending (!ascending). */
static void sort1_comm_sched(int *sched, int npes, int sortpe, int ascending)
{
     int *sortsched, i;
     sortsched = (int *) malloc(npes * sizeof(int) * 2);
     fill1_comm_sched(sortsched, sortpe, npes);
     if (ascending)
          for (i = 0; i < npes; ++i)
               sortsched[npes + sortsched[i]] = sched[i];
     else
          for (i = 0; i < npes; ++i)
               sortsched[2*npes - 1 - sortsched[i]] = sched[i];
     for (i = 0; i < npes; ++i)
          sched[i] = sortsched[npes + i];
     free(sortsched);
}

/* Below, we have various checks in case of bugs: */

/* check for deadlocks by simulating the schedule and looking for
   cycles in the dependency list; returns 0 if there are deadlocks
   (or other errors) */
static int check_schedule_deadlock(int **sched, int npes)
{
     int *step, *depend, *visited, pe, pe2, period, done = 0;
     int counter = 0;

     /* step[pe] is the step in the schedule that a given pe is on */
     step = (int *) malloc(sizeof(int) * npes);

     /* depend[pe] is the pe' that pe is currently waiting for a message
	from (-1 if none) */
     depend = (int *) malloc(sizeof(int) * npes);

     /* visited[pe] tells whether we have visited the current pe already
	when we are looking for cycles. */
     visited = (int *) malloc(sizeof(int) * npes);

     if (!step || !depend || !visited) {
	  free(step); free(depend); free(visited);
	  return 0;
     }

     for (pe = 0; pe < npes; ++pe)
	  step[pe] = 0;

     while (!done) {
	  ++counter;

	  for (pe = 0; pe < npes; ++pe)
	       depend[pe] = sched[pe][step[pe]];
	  
	  /* now look for cycles in the dependencies with period > 2: */
	  for (pe = 0; pe < npes; ++pe)
	       if (depend[pe] != -1) {
		    for (pe2 = 0; pe2 < npes; ++pe2)
			 visited[pe2] = 0;

		    period = 0;
		    pe2 = pe;
		    do {
			 visited[pe2] = period + 1;
			 pe2 = depend[pe2];
			 period++;
		    } while (pe2 != -1 && !visited[pe2]);

		    if (pe2 == -1) {
			 fprintf(stderr,
				 "BUG: unterminated cycle in schedule!\n");
			 free(step); free(depend);
			 free(visited);
			 return 0;
		    }
		    if (period - (visited[pe2] - 1) > 2) {
			 fprintf(stderr,"BUG: deadlock in schedule!\n");
			 free(step); free(depend);
			 free(visited);
			 return 0;
		    }

		    if (pe2 == pe)
			 step[pe]++;
	       }

	  done = 1;
	  for (pe = 0; pe < npes; ++pe)
	       if (sched[pe][step[pe]] != -1) {
		    done = 0;
		    break;
	       }
     }

     free(step); free(depend); free(visited);
     return (counter > 0 ? counter : 1);
}

/* sanity checks; prints message and returns 0 on failure.
   undocumented feature: the return value on success is actually the
   number of steps required for the schedule to complete, counting
   stalls. */
int check_comm_schedule(int **sched, int npes)
{
     int pe, i, comm_pe;
     
     for (pe = 0; pe < npes; ++pe) {
	  for (comm_pe = 0; comm_pe < npes; ++comm_pe) {
	       for (i = 0; sched[pe][i] != -1 && sched[pe][i] != comm_pe; ++i)
		    ;
	       if (sched[pe][i] == -1) {
		    fprintf(stderr,"BUG: schedule never sends message from "
			    "%d to %d.\n",pe,comm_pe);
		    return 0;  /* never send message to comm_pe */
	       }
	  }
	  for (i = 0; sched[pe][i] != -1; ++i)
	       ;
	  if (i != npes) {
	       fprintf(stderr,"BUG: schedule sends too many messages from "
		       "%d\n",pe);
	       return 0;
	  }
     }
     return check_schedule_deadlock(sched,npes);
}

/* invert the order of all the schedules; this has no effect on
   its required properties. */
void invert_comm_schedule(int **sched, int npes)
{
     int pe, i;

     for (pe = 0; pe < npes; ++pe)
	  for (i = 0; i < npes/2; ++i) {
	       int dummy = sched[pe][i];
	       sched[pe][i] = sched[pe][npes-1-i];
	       sched[pe][npes-1-i] = dummy;
	  }
}

/* Sort the schedule for sort_pe in ascending order of processor
   index.  Unfortunately, for odd npes (when schedule has a stall
   to begin with) this will introduce an extra stall due to
   the motion of the self-communication past a stall.  We could
   fix this if it were really important.  Actually, we don't
   get an extra stall when sort_pe == 0 or npes-1, which is sufficient
   for our purposes. */
void sort_comm_schedule(int **sched, int npes, int sort_pe)
{
     int i,j,pe;

     /* Note that we can do this sort in O(npes) swaps because we know
	that the numbers we are sorting are just 0...npes-1.   But we'll
	just do a bubble sort for simplicity here. */

     for (i = 0; i < npes - 1; ++i)
	  for (j = i + 1; j < npes; ++j)
	       if (sched[sort_pe][i] > sched[sort_pe][j]) {
		    for (pe = 0; pe < npes; ++pe) {
			 int s = sched[pe][i];
			 sched[pe][i] = sched[pe][j];
			 sched[pe][j] = s;
		    }
	       }
}

/* print the schedule (for debugging purposes) */
void print_comm_schedule(int **sched, int npes)
{
     int pe, i, width;

     if (npes < 10)
	  width = 1;
     else if (npes < 100)
	  width = 2;
     else
	  width = 3;

     for (pe = 0; pe < npes; ++pe) {
	  printf("pe %*d schedule:", width, pe);
	  for (i = 0; sched[pe][i] != -1; ++i)
	       printf("  %*d",width,sched[pe][i]);
	  printf("\n");
     }
}

int main(int argc, char **argv)
{
     int **sched;
     int npes = -1, sortpe = -1, steps, i;

     if (argc >= 2) {
	  npes = atoi(argv[1]);
	  if (npes <= 0) {
	       fprintf(stderr,"npes must be positive!");
	       return 1;
	  }
     }
     if (argc >= 3) {
	  sortpe = atoi(argv[2]);
	  if (sortpe < 0 || sortpe >= npes) {
	       fprintf(stderr,"sortpe must be between 0 and npes-1.\n");
	       return 1;
	  }
     }

     if (npes != -1) {
	  printf("Computing schedule for npes = %d:\n",npes);
	  sched = make_comm_schedule(npes);
	  if (!sched) {
	       fprintf(stderr,"Out of memory!");
	       return 6;
	  }
	  
	  if (steps = check_comm_schedule(sched,npes))
	       printf("schedule OK (takes %d steps to complete).\n", steps);
	  else
	       printf("schedule not OK.\n");

	  print_comm_schedule(sched, npes);
	  
	  if (sortpe != -1) {
	       printf("\nRe-creating schedule for pe = %d...\n", sortpe);
	       int *sched1 = (int*) malloc(sizeof(int) * npes);
	       for (i = 0; i < npes; ++i) sched1[i] = -1;
	       fill1_comm_sched(sched1, sortpe, npes);
	       printf("  =");
	       for (i = 0; i < npes; ++i) 
		    printf("  %*d", npes < 10 ? 1 : (npes < 100 ? 2 : 3),
			   sched1[i]);
	       printf("\n");

	       printf("\nSorting schedule for sortpe = %d...\n", sortpe);
	       sort_comm_schedule(sched,npes,sortpe);
	       
	       if (steps = check_comm_schedule(sched,npes))
		    printf("schedule OK (takes %d steps to complete).\n", 
			   steps);
	       else
		    printf("schedule not OK.\n");

	       print_comm_schedule(sched, npes);

	       printf("\nInverting schedule...\n");
	       invert_comm_schedule(sched,npes);
	       
	       if (steps = check_comm_schedule(sched,npes))
		    printf("schedule OK (takes %d steps to complete).\n", 
			   steps);
	       else
		    printf("schedule not OK.\n");

	       print_comm_schedule(sched, npes);
	       
	       free_comm_schedule(sched,npes);

	       free(sched1);
	  }
     }
     else {
	  printf("Doing infinite tests...\n");
	  for (npes = 1; ; ++npes) {
	       int *sched1 = (int*) malloc(sizeof(int) * npes);
	       printf("npes = %d...",npes);
	       sched = make_comm_schedule(npes);
	       if (!sched) {
		    fprintf(stderr,"Out of memory!\n");
		    return 5;
	       }
	       for (sortpe = 0; sortpe < npes; ++sortpe) {
		    empty_comm_schedule(sched,npes);
		    fill_comm_schedule(sched,npes);
		    if (!check_comm_schedule(sched,npes)) {
			 fprintf(stderr,
				 "\n -- fill error for sortpe = %d!\n",sortpe);
			 return 2;
		    }

		    for (i = 0; i < npes; ++i) sched1[i] = -1;
		    fill1_comm_sched(sched1, sortpe, npes);
		    for (i = 0; i < npes; ++i)
			 if (sched1[i] != sched[sortpe][i])
			      fprintf(stderr,
				      "\n -- fill1 error for pe = %d!\n",
				      sortpe);

		    sort_comm_schedule(sched,npes,sortpe);
		    if (!check_comm_schedule(sched,npes)) {
			 fprintf(stderr,
				 "\n -- sort error for sortpe = %d!\n",sortpe);
			 return 3;
		    }
		    invert_comm_schedule(sched,npes);
		    if (!check_comm_schedule(sched,npes)) {
			 fprintf(stderr,
				 "\n -- invert error for sortpe = %d!\n",
				 sortpe);
			 return 4;
		    }
	       }
	       free_comm_schedule(sched,npes);
	       printf("OK\n");
	       if (npes % 50 == 0)
		    printf("(...Hit Ctrl-C to stop...)\n");
	       free(sched1);
	  }
     }

     return 0;
}
