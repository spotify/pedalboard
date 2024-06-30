/*
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
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

/* threads.c: Portable thread spawning for loops, via the X(spawn_loop)
   function.  The first portion of this file is a set of macros to
   spawn and join threads on various systems. */

#include "threads/threads.h"
#include "api/api.h"

#if defined(USING_POSIX_THREADS)

#include <pthread.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/* implementation of semaphores and mutexes: */
#if (defined(_POSIX_SEMAPHORES) && (_POSIX_SEMAPHORES >= 200112L))

   /* If optional POSIX semaphores are supported, use them to
      implement both semaphores and mutexes. */
#  include <semaphore.h>
#  include <errno.h>

   typedef sem_t os_sem_t;

   static void os_sem_init(os_sem_t *s) { sem_init(s, 0, 0); }
   static void os_sem_destroy(os_sem_t *s) { sem_destroy(s); }

   static void os_sem_down(os_sem_t *s)
   {
	int err;
	do {
	     err = sem_wait(s);
	} while (err == -1 && errno == EINTR);
	CK(err == 0);
   }

   static void os_sem_up(os_sem_t *s) { sem_post(s); }

   /*
      The reason why we use sem_t to implement mutexes is that I have
      seen mysterious hangs with glibc-2.7 and linux-2.6.22 when using
      pthread_mutex_t, but no hangs with sem_t or with linux >=
      2.6.24.  For lack of better information, sem_t looks like the
      safest choice.
   */
   typedef sem_t os_mutex_t;
   static void os_mutex_init(os_mutex_t *s) { sem_init(s, 0, 1); }
   #define os_mutex_destroy os_sem_destroy
   #define os_mutex_lock os_sem_down
   #define os_mutex_unlock os_sem_up

#else

   /* If optional POSIX semaphores are not defined, use pthread
      mutexes for mutexes, and simulate semaphores with condition
      variables */
   typedef pthread_mutex_t os_mutex_t;

   static void os_mutex_init(os_mutex_t *s)
   {
	pthread_mutex_init(s, (pthread_mutexattr_t *)0);
   }

   static void os_mutex_destroy(os_mutex_t *s) { pthread_mutex_destroy(s); }
   static void os_mutex_lock(os_mutex_t *s) { pthread_mutex_lock(s); }
   static void os_mutex_unlock(os_mutex_t *s) { pthread_mutex_unlock(s); }

   typedef struct {
	pthread_mutex_t m;
	pthread_cond_t c;
	volatile int x;
   } os_sem_t;

   static void os_sem_init(os_sem_t *s)
   {
	pthread_mutex_init(&s->m, (pthread_mutexattr_t *)0);
	pthread_cond_init(&s->c, (pthread_condattr_t *)0);

	/* wrap initialization in lock to exploit the release
	   semantics of pthread_mutex_unlock() */
	pthread_mutex_lock(&s->m);
	s->x = 0;
	pthread_mutex_unlock(&s->m);
   }

   static void os_sem_destroy(os_sem_t *s)
   {
	pthread_mutex_destroy(&s->m);
	pthread_cond_destroy(&s->c);
   }

   static void os_sem_down(os_sem_t *s)
   {
	pthread_mutex_lock(&s->m);
	while (s->x <= 0)
	     pthread_cond_wait(&s->c, &s->m);
	--s->x;
	pthread_mutex_unlock(&s->m);
   }

   static void os_sem_up(os_sem_t *s)
   {
	pthread_mutex_lock(&s->m);
	++s->x;
	pthread_cond_signal(&s->c);
	pthread_mutex_unlock(&s->m);
   }

#endif

#define FFTW_WORKER void *

static void os_create_thread(FFTW_WORKER (*worker)(void *arg),
			     void *arg)
{
     pthread_attr_t attr;
     pthread_t tid;

     pthread_attr_init(&attr);
     pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
     pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

     pthread_create(&tid, &attr, worker, (void *)arg);
     pthread_attr_destroy(&attr);
}

static void os_destroy_thread(void)
{
     pthread_exit((void *)0);
}

/* support for static mutexes */
typedef pthread_mutex_t os_static_mutex_t;
#define OS_STATIC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
static void os_static_mutex_lock(os_static_mutex_t *s) { pthread_mutex_lock(s); }
static void os_static_mutex_unlock(os_static_mutex_t *s) { pthread_mutex_unlock(s); }

#elif defined(__WIN32__) || defined(_WIN32) || defined(_WINDOWS)
/* hack: windef.h defines INT for its own purposes and this causes
   a conflict with our own INT in ifftw.h.  Divert the windows
   definition into another name unlikely to cause a conflict */
#define INT magnus_ab_INTegro_seclorum_nascitur_ordo
#include <windows.h>
#include <process.h>
#include <intrin.h>
#undef INT

typedef HANDLE os_mutex_t;

static void os_mutex_init(os_mutex_t *s)
{
     *s = CreateMutex(NULL, FALSE, NULL);
}

static void os_mutex_destroy(os_mutex_t *s)
{
     CloseHandle(*s);
}

static void os_mutex_lock(os_mutex_t *s)
{
     WaitForSingleObject(*s, INFINITE);
}

static void os_mutex_unlock(os_mutex_t *s)
{
     ReleaseMutex(*s);
}

typedef HANDLE os_sem_t;

static void os_sem_init(os_sem_t *s)
{
     *s = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
}

static void os_sem_destroy(os_sem_t *s)
{
     CloseHandle(*s);
}

static void os_sem_down(os_sem_t *s)
{
     WaitForSingleObject(*s, INFINITE);
}

static void os_sem_up(os_sem_t *s)
{
     ReleaseSemaphore(*s, 1, NULL);
}

#define FFTW_WORKER unsigned __stdcall
typedef unsigned (__stdcall *winthread_start) (void *);

static void os_create_thread(winthread_start worker,
			     void *arg)
{
     _beginthreadex((void *)NULL,               /* security attrib */
		    0,				/* stack size */
		    worker,                     /* start address */
		    arg,			/* parameters */
		    0,				/* creation flags */
		    (unsigned *)NULL);		/* tid */
}

static void os_destroy_thread(void)
{
     _endthreadex(0);
}

/* windows does not have statically-initialized mutexes---fake a
   spinlock */
typedef volatile LONG os_static_mutex_t;
#define OS_STATIC_MUTEX_INITIALIZER 0
static void os_static_mutex_lock(os_static_mutex_t *s)
{
     while (InterlockedExchange(s, 1) == 1) {
          YieldProcessor();
          Sleep(0);
     }
}
static void os_static_mutex_unlock(os_static_mutex_t *s)
{
     LONG old = InterlockedExchange(s, 0);
     A(old == 1);
}
#else
#error "No threading layer defined"
#endif

/************************************************************************/

/* Main code: */
struct worker {
     os_sem_t ready;
     os_sem_t done;
     struct work *w;
     struct worker *cdr;
};

static struct worker *make_worker(void)
{
     struct worker *q = (struct worker *)MALLOC(sizeof(*q), OTHER);
     os_sem_init(&q->ready);
     os_sem_init(&q->done);
     return q;
}

static void unmake_worker(struct worker *q)
{
     os_sem_destroy(&q->done);
     os_sem_destroy(&q->ready);
     X(ifree)(q);
}

struct work {
     spawn_function proc;
     spawn_data d;
     struct worker *q; /* the worker responsible for performing this work */
};

static os_mutex_t queue_lock;
static os_sem_t termination_semaphore;

static struct worker *worker_queue;
#define WITH_QUEUE_LOCK(what)			\
{						\
     os_mutex_lock(&queue_lock);		\
     what;					\
     os_mutex_unlock(&queue_lock);		\
}

static FFTW_WORKER worker(void *arg)
{
     struct worker *ego = (struct worker *)arg;
     struct work *w;

     for (;;) {
	  /* wait until work becomes available */
	  os_sem_down(&ego->ready);

	  w = ego->w;

	  /* !w->proc ==> terminate worker */
	  if (!w->proc) break;

	  /* do the work */
          w->proc(&w->d);

	  /* signal that work is done */
	  os_sem_up(&ego->done);
     }

     /* termination protocol */
     os_sem_up(&termination_semaphore);

     os_destroy_thread();
     /* UNREACHABLE */
     return 0;
}

static void enqueue(struct worker *q)
{
     WITH_QUEUE_LOCK({
	  q->cdr = worker_queue;
	  worker_queue = q;
     });
}

static struct worker *dequeue(void)
{
     struct worker *q;

     WITH_QUEUE_LOCK({
	  q = worker_queue;
	  if (q)
	       worker_queue = q->cdr;
     });

     if (!q) {
	  /* no worker is available.  Create one */
	  q = make_worker();
	  os_create_thread(worker, q);
     }

     return q;
}


static void kill_workforce(void)
{
     struct work w;

     w.proc = 0;

     WITH_QUEUE_LOCK({
	  /* tell all workers that they must terminate.

	     Because workers enqueue themselves before signaling the
	     completion of the work, all workers belong to the worker queue
	     if we get here.  Also, all workers are waiting at
	     os_sem_down(ready), so we can hold the queue lock without
	     deadlocking */
	  while (worker_queue) {
	       struct worker *q = worker_queue;
	       worker_queue = q->cdr;
	       q->w = &w;
	       os_sem_up(&q->ready);
	       os_sem_down(&termination_semaphore);
	       unmake_worker(q);
	  }
     });
}

static os_static_mutex_t initialization_mutex = OS_STATIC_MUTEX_INITIALIZER;

int X(ithreads_init)(void)
{
     os_static_mutex_lock(&initialization_mutex); {
          os_mutex_init(&queue_lock);
          os_sem_init(&termination_semaphore);

          WITH_QUEUE_LOCK({
               worker_queue = 0;
          });
     } os_static_mutex_unlock(&initialization_mutex);

     return 0; /* no error */
}

/* Distribute a loop from 0 to loopmax-1 over nthreads threads.
   proc(d) is called to execute a block of iterations from d->min
   to d->max-1.  d->thr_num indicate the number of the thread
   that is executing proc (from 0 to nthreads-1), and d->data is
   the same as the data parameter passed to X(spawn_loop).

   This function returns only after all the threads have completed. */
void X(spawn_loop)(int loopmax, int nthr, spawn_function proc, void *data)
{
     int block_size;
     int i;

     A(loopmax >= 0);
     A(nthr > 0);
     A(proc);

     if (!loopmax) return;

     /* Choose the block size and number of threads in order to (1)
        minimize the critical path and (2) use the fewest threads that
        achieve the same critical path (to minimize overhead).
        e.g. if loopmax is 5 and nthr is 4, we should use only 3
        threads with block sizes of 2, 2, and 1. */
     block_size = (loopmax + nthr - 1) / nthr;
     nthr = (loopmax + block_size - 1) / block_size;

     if (X(spawnloop_callback)) { /* user-defined spawnloop backend */
          spawn_data *sdata;
          STACK_MALLOC(spawn_data *, sdata, sizeof(spawn_data) * nthr);
          for (i = 0; i < nthr; ++i) {
               spawn_data *d = &sdata[i];
               d->max = (d->min = i * block_size) + block_size;
               if (d->max > loopmax)
                    d->max = loopmax;
               d->thr_num = i;
               d->data = data;
          }
          X(spawnloop_callback)(proc, sdata, sizeof(spawn_data), nthr, X(spawnloop_callback_data));
          STACK_FREE(sdata);
     }
     else {
          struct work *r;
          STACK_MALLOC(struct work *, r, sizeof(struct work) * nthr);

          /* distribute work: */
          for (i = 0; i < nthr; ++i) {
               struct work *w = &r[i];
               spawn_data *d = &w->d;

               d->max = (d->min = i * block_size) + block_size;
               if (d->max > loopmax)
                    d->max = loopmax;
               d->thr_num = i;
               d->data = data;
               w->proc = proc;

               if (i == nthr - 1) {
                    /* do the work ourselves */
                    proc(d);
               } else {
                    /* assign a worker to W */
                    w->q = dequeue();

                    /* tell worker w->q to do it */
                    w->q->w = w; /* Dirac could have written this */
                    os_sem_up(&w->q->ready);
               }
          }

          for (i = 0; i < nthr - 1; ++i) {
               struct work *w = &r[i];
               os_sem_down(&w->q->done);
               enqueue(w->q);
          }

          STACK_FREE(r);
     }
}

void X(threads_cleanup)(void)
{
     kill_workforce();
     os_mutex_destroy(&queue_lock);
     os_sem_destroy(&termination_semaphore);
}

static os_static_mutex_t install_planner_hooks_mutex = OS_STATIC_MUTEX_INITIALIZER;
static os_mutex_t planner_mutex;
static int planner_hooks_installed = 0;

static void lock_planner_mutex(void)
{
     os_mutex_lock(&planner_mutex);
}

static void unlock_planner_mutex(void)
{
     os_mutex_unlock(&planner_mutex);
}

void X(threads_register_planner_hooks)(void)
{
     os_static_mutex_lock(&install_planner_hooks_mutex); {
          if (!planner_hooks_installed) {
               os_mutex_init(&planner_mutex);
               X(set_planner_hooks)(lock_planner_mutex, unlock_planner_mutex);
               planner_hooks_installed = 1;
          }
     } os_static_mutex_unlock(&install_planner_hooks_mutex);
}
