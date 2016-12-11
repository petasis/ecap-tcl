/*
 * tpool.h: Header file for a thread-pool implementation for C.
 * Taken from: C level Thread Pool, http://wiki.tcl.tk/36950
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*TPoolWork)(Tcl_Interp *interp, void *data);

typedef struct _TPoolThread {
   struct _TPool *tp;
   Tcl_ThreadId   id;
   TPoolWork    func;

   Tcl_Mutex     lock;
   Tcl_Condition wait;

   void        *data;
   int          work;
   Tcl_Interp  *interp;
} TPoolThread;

typedef struct _TPool {
   Tcl_Mutex     lock;
   Tcl_Condition wait;

   unsigned int         next;
   unsigned int         nthread;
   TPoolThread         *thread;
} TPool;

TPool *TPoolInit(int n);
void TPoolFree(TPool *tp);
TPoolThread *TPoolThreadStart(TPool *tp, TPoolWork func, void *data);
TPoolThread *TPoolStartInThreadPosition(TPool *tp, int thread,
                                TPoolWork func, void *data);
TPoolThread *TPoolStartInThread(TPoolThread *t,
                                TPoolWork func, void *data);
void TPoolThreadWait (TPoolThread *t);

#ifdef __cplusplus
}
#endif
