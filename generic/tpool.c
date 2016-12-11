/*
 * tpool.c: Implementation file for a thread-pool implementation for C.
 * Taken from: C level Thread Pool, http://wiki.tcl.tk/36950
 */

/*
 * Simple thread pool manager
 * John Roll 2012
 */

#include <stdlib.h>

#define TCL_THREADS 1

#include "tcl.h"
#include "tpool.h"

void TPoolWorker(void *data) {
  TPoolThread *t = (TPoolThread *) data;
  TPool       *tp = t->tp;
  // int i_have_work;

  Tcl_MutexLock(&t->lock);
  // Create an interp...
  t->interp = Tcl_CreateInterp();
  if (t->interp == NULL) return;
  if (Tcl_Init(t->interp) != TCL_OK) return;
  while ( 1 ) {
    t->work = 0;
    Tcl_ConditionNotify(&t->wait);
    Tcl_ConditionWait(&t->wait, &t->lock, NULL /* no timeout */);

    if ( t->work ) {
      Tcl_MutexUnlock(&t->lock);

      t->func(t->interp, t->data);

      Tcl_MutexLock(&t->lock);

      t->work = 0;

      Tcl_ConditionNotify(&t->wait);

      Tcl_MutexLock(&tp->lock);
      Tcl_ConditionNotify(&tp->wait);
      Tcl_MutexUnlock(&tp->lock);
    }
  }
}

TPool *TPoolInit(int n) {
  int    i;
  TPool *tp  = calloc(sizeof(TPool), 1);
  tp->thread = calloc(sizeof(TPoolThread), n);
  tp->nthread = n;

  for ( i = 0; i < n; i++ ) {
    tp->thread[i].work = 1;
    tp->thread[i].tp   = tp;
    Tcl_CreateThread(&tp->thread[i].id, TPoolWorker, &tp->thread[i],
                     TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS);
  }
  for ( i = 0; i < n; i++ ) {
    TPoolThread *t = &tp->thread[i];

    Tcl_MutexLock(&t->lock);
    if ( t->work ) {
        Tcl_ConditionWait(&t->wait, &t->lock, NULL /* no timeout */);
    }
    Tcl_MutexUnlock(&t->lock);
  }

  return tp;
}

void TPoolFree(TPool *tp) {
  if (tp && tp->thread) free(tp->thread);
  if (tp) free(tp);
}

TPoolThread *TPoolThreadStart(TPool *tp, TPoolWork func, void *data) {
  int start = tp->next;

  Tcl_MutexLock(&tp->lock);

  while ( tp->thread[tp->next].work ) { // Find a thread that will work for us.
    // TPoolThread *t;
    tp->next = ((++(tp->next)) % tp->nthread);

    if ( tp->next == start ) {
      Tcl_ConditionWait(&tp->wait, &tp->lock, NULL /* no timeout */);
    }
  }
  Tcl_MutexUnlock(&tp->lock);

  {
    TPoolThread *t = &tp->thread[tp->next];

    Tcl_MutexLock(&t->lock);

    t->func = func;
    t->data = data;
    t->work = 1;

    Tcl_ConditionNotify(&t->wait);

    Tcl_MutexUnlock(&t->lock);

    return t;
  }
}

TPoolThread *TPoolStartInThreadPosition(TPool *tp, int thread,
                                TPoolWork func, void *data) {
  TPoolThread *t = &tp->thread[thread];
  return TPoolStartInThread(t, func, data);
}

TPoolThread *TPoolStartInThread(TPoolThread *t,
                                TPoolWork func, void *data) {

  TPoolThreadWait(t);
  Tcl_MutexLock(&t->lock);
  t->func = func;
  t->data = data;
  t->work = 1;

  Tcl_ConditionNotify(&t->wait);

  Tcl_MutexUnlock(&t->lock);

  return t;
}

void TPoolThreadWait(TPoolThread *t) {
   Tcl_MutexLock(&t->lock);

   while ( t->work ) {
       Tcl_ConditionWait(&t->wait, &t->lock, NULL /* no timeout */);
   }

   Tcl_MutexUnlock(&t->lock);
}
