/*	$OpenBSD: rf_threadstuff.h,v 1.1 1999/01/11 14:29:54 niklas Exp $	*/
/*	$NetBSD: rf_threadstuff.h,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * threadstuff.h -- definitions for threads, locks, and synchronization
 *
 * The purpose of this file is provide some illusion of portability.
 * If the functions below can be implemented with the same semantics on
 * some new system, then at least the synchronization and thread control
 * part of the code should not require modification to port to a new machine.
 * the only other place where the pthread package is explicitly used is
 * threadid.h
 *
 * this file should be included above stdio.h to get some necessary defines.
 *
 */

/* :  
 * Log: rf_threadstuff.h,v 
 * Revision 1.38  1996/08/12 22:37:47  jimz
 * add AIX stuff for user driver
 *
 * Revision 1.37  1996/08/11  00:47:09  jimz
 * make AIX friendly
 *
 * Revision 1.36  1996/07/23  22:06:59  jimz
 * add rf_destroy_threadgroup
 *
 * Revision 1.35  1996/07/23  21:31:16  jimz
 * add init_threadgroup
 *
 * Revision 1.34  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.33  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.32  1996/06/17  03:01:11  jimz
 * get rid of JOIN stuff
 *
 * Revision 1.31  1996/06/14  23:15:38  jimz
 * attempt to deal with thread GC problem
 *
 * Revision 1.30  1996/06/11  18:12:36  jimz
 * get rid of JOIN operations
 * use ThreadGroup stuff instead
 * fix some allocation/deallocation and sync bugs
 *
 * Revision 1.29  1996/06/11  13:48:10  jimz
 * make kernel RF_THREAD_CREATE give back happier return vals
 *
 * Revision 1.28  1996/06/10  16:40:01  jimz
 * break user-level stuff out into lib+apps
 *
 * Revision 1.27  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.26  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.25  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.24  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.23  1996/05/20  19:31:54  jimz
 * add atomic debug (mutex and cond leak finder) stuff
 *
 * Revision 1.22  1996/05/20  16:24:49  jimz
 * get happy in simulator
 *
 * Revision 1.21  1996/05/20  16:15:07  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.20  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.19  1996/05/09  17:16:53  jimz
 * correct arg to JOIN_THREAD
 *
 * Revision 1.18  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.17  1995/12/06  15:15:21  root
 * added copyright info
 *
 */

#ifndef _RF__RF_THREADSTUFF_H_
#define _RF__RF_THREADSTUFF_H_

#include "rf_types.h"

#define rf_create_managed_mutex(a,b) _rf_create_managed_mutex(a,b,__FILE__,__LINE__)
#define rf_create_managed_cond(a,b) _rf_create_managed_cond(a,b,__FILE__,__LINE__)
#define rf_init_managed_threadgroup(a,b) _rf_init_managed_threadgroup(a,b,__FILE__,__LINE__)
#define rf_init_threadgroup(a) _rf_init_threadgroup(a,__FILE__,__LINE__)
#define rf_destroy_threadgroup(a) _rf_destroy_threadgroup(a,__FILE__,__LINE__)

int _rf_init_threadgroup(RF_ThreadGroup_t *g, char *file, int line);
int _rf_destroy_threadgroup(RF_ThreadGroup_t *g, char *file, int line);
int _rf_init_managed_threadgroup(RF_ShutdownList_t **listp,
	RF_ThreadGroup_t *g, char *file, int line);

#ifndef SIMULATE /* will null all this calls */
#ifndef KERNEL

#if defined(__osf__) || defined(AIX)
#include <pthread.h>
#endif /* __osf__ || AIX */

#define RF_DEBUG_ATOMIC 0

#if RF_DEBUG_ATOMIC > 0
#define RF_ATENT_M 1
#define RF_ATENT_C 2
typedef struct RF_ATEnt_s RF_ATEnt_t;
struct RF_ATEnt_s {
	char            *file;
	int              line;
	pthread_mutex_t  m;
	pthread_cond_t   c;
	int              type;
	int              otype;
	RF_ATEnt_t      *next;
	RF_ATEnt_t      *prev;
};

#define RF_DECLARE_MUTEX(_m_)                RF_ATEnt_t *_m_;
#define RF_DECLARE_STATIC_MUTEX(_m_)  static RF_ATEnt_t *_m_;
#define RF_DECLARE_EXTERN_MUTEX(_m_)  extern RF_ATEnt_t *_m_;
#define RF_DECLARE_COND(_c_)                 RF_ATEnt_t *_c_;
#define RF_DECLARE_STATIC_COND(_c_)   static RF_ATEnt_t *_c_;
#define RF_DECLARE_EXTERN_COND(_c_)   extern RF_ATEnt_t *_c_;

int _rf_mutex_init(RF_ATEnt_t **m, char *file, int line);
int _rf_mutex_destroy(RF_ATEnt_t **m, char *file, int line);
int _rf_cond_init(RF_ATEnt_t **c, char *file, int line);
int _rf_cond_destroy(RF_ATEnt_t **c, char *file, int line);
void rf_atent_init(void);
void rf_atent_shutdown(void);

#define rf_mutex_init(_m_) _rf_mutex_init(_m_,__FILE__,__LINE__)
#define rf_mutex_destroy(_m_) _rf_mutex_destroy(_m_,__FILE__,__LINE__)
#define rf_cond_init(_m_) _rf_cond_init(_m_,__FILE__,__LINE__)
#define rf_cond_destroy(_m_) _rf_cond_destroy(_m_,__FILE__,__LINE__)

#define RF_LOCK_MUTEX(_a_)     {RF_ASSERT((_a_)->type == RF_ATENT_M); pthread_mutex_lock(&((_a_)->m));}
#define RF_UNLOCK_MUTEX(_a_)   {RF_ASSERT((_a_)->type == RF_ATENT_M); pthread_mutex_unlock(&((_a_)->m));}

#define RF_WAIT_COND(_c_,_m_)  { \
	RF_ASSERT((_c_)->type == RF_ATENT_C); \
	RF_ASSERT((_m_)->type == RF_ATENT_M); \
	pthread_cond_wait( &((_c_)->c), &((_m_)->m) ); \
}
#define RF_SIGNAL_COND(_c_)    {RF_ASSERT((_c_)->type == RF_ATENT_C); pthread_cond_signal( &((_c_)->c));}
#define RF_BROADCAST_COND(_c_) {RF_ASSERT((_c_)->type == RF_ATENT_C); pthread_cond_broadcast(&((_c_)->c));}

#else /* RF_DEBUG_ATOMIC > 0 */

/* defining these as macros allows us to NULL them out in the kernel */
#define RF_DECLARE_MUTEX(_m_)                pthread_mutex_t _m_;
#define RF_DECLARE_STATIC_MUTEX(_m_)  static pthread_mutex_t _m_;
#define RF_DECLARE_EXTERN_MUTEX(_m_)  extern pthread_mutex_t _m_;
#define RF_DECLARE_COND(_c_)                 pthread_cond_t  _c_;
#define RF_DECLARE_STATIC_COND(_c_)   static pthread_cond_t  _c_;
#define RF_DECLARE_EXTERN_COND(_c_)   extern pthread_cond_t  _c_;

int rf_mutex_init(pthread_mutex_t *m);
int rf_mutex_destroy(pthread_mutex_t *m);
int rf_cond_init(pthread_cond_t *c);
int rf_cond_destroy(pthread_cond_t *c);

#define RF_LOCK_MUTEX(_m_)     {pthread_mutex_lock(&(_m_));}
#define RF_UNLOCK_MUTEX(_m_)   pthread_mutex_unlock(&(_m_))

#define RF_WAIT_COND(_c_,_m_)  pthread_cond_wait( &(_c_), &(_m_) )
#define RF_SIGNAL_COND(_c_)    pthread_cond_signal( &(_c_) )
#define RF_BROADCAST_COND(_c_) pthread_cond_broadcast(&(_c_))

#endif /* RF_DEBUG_ATOMIC > 0 */

int _rf_create_managed_mutex(RF_ShutdownList_t **listp, pthread_mutex_t *m, char *file, int line);
int _rf_create_managed_cond(RF_ShutdownList_t **listp, pthread_cond_t *c, char *file, int line);

typedef pthread_t       RF_Thread_t;
#ifdef __osf__
typedef pthread_addr_t  RF_ThreadArg_t;        /* the argument to a thread function */
#else /* __osf__ */
typedef void *RF_ThreadArg_t;        /* the argument to a thread function */
#endif /* __osf__ */
typedef pthread_attr_t  RF_ThreadAttr_t;     /* a thread creation attribute structure */

#ifdef __osf__
#define RF_EXIT_THREAD(_status_)                  pthread_exit( (pthread_addr_t) (_status_) )
#else /* __osf__ */
#define RF_EXIT_THREAD(_status_)                  pthread_exit( (void *) (_status_) )
#endif /* __osf__ */
#define RF_DELAY_THREAD(_secs_, _msecs_)          {struct timespec interval;                   \
						interval.tv_sec = (_secs_);                 \
						interval.tv_nsec = (_msecs_)*1000000;       \
						pthread_delay_np(&interval);                \
					       }
#define RF_DELAY_THREAD_TS(_ts_) pthread_delay_np(&(_ts_))

#ifdef __osf__
#define RF_THREAD_ATTR_CREATE(_attr_)            pthread_attr_create( &(_attr_) )
#define RF_THREAD_ATTR_DELETE(_attr_)            pthread_attr_delete( &(_attr_) )
#endif /* __osf__ */
#ifdef AIX
#define RF_THREAD_ATTR_CREATE(_attr_)            pthread_attr_init( &(_attr_) )
#define RF_THREAD_ATTR_DELETE(_attr_)            pthread_attr_destroy( &(_attr_) )
#endif /* AIX */
#define RF_THREAD_ATTR_SETSTACKSIZE(_attr_,_sz_) pthread_attr_setstacksize(&(_attr_), (long) (_sz_))
#define RF_THREAD_ATTR_GETSTACKSIZE(_attr_)      pthread_attr_getstacksize(_attr_)
#define RF_THREAD_ATTR_SETSCHED(_attr_,_sched_)  pthread_attr_setsched(&(_attr_), (_sched_))
#define RF_CREATE_ATTR_THREAD(_handle_, _attr_, _func_, _arg_) \
  pthread_create(&(_handle_), (_attr_), (pthread_startroutine_t) (_func_), (_arg_))


extern pthread_attr_t raidframe_attr_default;
int rf_thread_create(RF_Thread_t *thread, pthread_attr_t attr,
  void (*func)(), RF_ThreadArg_t arg);

#define RF_CREATE_THREAD(_handle_, _func_, _arg_)  \
  rf_thread_create(&(_handle_), raidframe_attr_default, (_func_), (_arg_))

#else   /* KERNEL */
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/lock.h>
#define decl_simple_lock_data(a,b) a struct simplelock b;
#define simple_lock_addr(a) ((struct simplelock *)&(a))
#else
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/lock.h>
#include <kern/sched_prim.h>
#define decl_simple_lock_data(a,b) a int (b);
#endif /* __NetBSD__ || __OpenBSD__ */

#if defined(__NetBSD__) || defined(__OpenBSD__)
typedef struct proc *RF_Thread_t;
#else
typedef thread_t RF_Thread_t;
#endif
typedef void *RF_ThreadArg_t;

#define RF_DECLARE_MUTEX(_m_)           decl_simple_lock_data(,(_m_))
#define RF_DECLARE_STATIC_MUTEX(_m_)    decl_simple_lock_data(static,(_m_))
#define RF_DECLARE_EXTERN_MUTEX(_m_)    decl_simple_lock_data(extern,(_m_))

#define RF_DECLARE_COND(_c_)            int _c_;
#define RF_DECLARE_STATIC_COND(_c_)     static int _c_;
#define RF_DECLARE_EXTERN_COND(_c_)     extern int _c_;

#define RF_LOCK_MUTEX(_m_)              simple_lock(&(_m_))
#define RF_UNLOCK_MUTEX(_m_)            simple_unlock(&(_m_))


#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/kthread.h>
/*
 * In Net- and OpenBSD, kernel threads are simply processes which share several
 * substructures and never run in userspace.
 *
 * XXX Note, Net- and OpenBSD does not yet have a wakeup_one(), so we always
 * XXX get Thundering Herd when a condition occurs.
 */
#define RF_WAIT_COND(_c_,_m_)           { \
	RF_UNLOCK_MUTEX(_m_); \
	tsleep(&_c_, PRIBIO | PCATCH, "rfwcond", 0); \
	RF_LOCK_MUTEX(_m_); \
}
#define RF_SIGNAL_COND(_c_)            wakeup(&(_c_))
#define RF_BROADCAST_COND(_c_)         wakeup(&(_c_))
#define	RF_CREATE_THREAD(_handle_, _func_, _arg_) \
	kthread_create((void (*) __P((void *)))(_func_), (void *)(_arg_), \
	    (struct proc **)&(_handle_), "raid")
#else /* ! __NetBSD__ && ! __OpenBSD__ */
/*
 * Digital UNIX/Mach threads.
 */
#define RF_WAIT_COND(_c_,_m_)           { \
	assert_wait((vm_offset_t)&(_c_), TRUE); \
	RF_UNLOCK_MUTEX(_m_); \
	thread_block(); \
	RF_LOCK_MUTEX(_m_); \
}
#define RF_SIGNAL_COND(_c_)            thread_wakeup_one(((vm_offset_t)&(_c_)))
#define RF_BROADCAST_COND(_c_)         thread_wakeup(((vm_offset_t)&(_c_)))
extern task_t first_task;
#define RF_CREATE_THREAD(_handle_, _func_, _arg_) \
	(((_handle_ = kernel_thread_w_arg(first_task, (void (*)())_func_, (void *)(_arg_))) != THREAD_NULL) ? 0 : ENOMEM)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* KERNEL */
#else /* SIMULATE */

#define RF_DECLARE_MUTEX(_m_)        int _m_;
#define RF_DECLARE_STATIC_MUTEX(_m_) static int _m_;
#define RF_DECLARE_EXTERN_MUTEX(_m_) extern int _m_;
#define RF_DECLARE_COND(_c_)         int _c_;
#define RF_DECLARE_STATIC_COND(_c_)  static int _c_;
#define RF_DECLARE_EXTERN_COND(_c_)  extern int _c_;

extern int rf_mutex_init(int *m);
extern int rf_mutex_destroy(int *m);
extern int rf_cond_init(int *c);
extern int rf_cond_destroy(int *c);

int rf_mutex_init(int *m);
int rf_mutex_destroy(int *m);
int _rf_create_managed_mutex(RF_ShutdownList_t **listp, int *m, char *file, int line);
int _rf_create_managed_cond(RF_ShutdownList_t **listp, int *m, char *file, int line);

typedef void  *RF_ThreadArg_t;        /* the argument to a thread function */

#define RF_LOCK_MUTEX(_m_)
#define RF_UNLOCK_MUTEX(_m_)

#define RF_WAIT_COND(_c_,_m_)
#define RF_SIGNAL_COND(_c_)
#define RF_BROADCAST_COND(_c_)

#define RF_EXIT_THREAD(_status_)
#define RF_DELAY_THREAD(_secs_, _msecs_)

#define RF_THREAD_ATTR_CREATE(_attr_) ;
#define RF_THREAD_ATTR_DELETE(_attr_) ;
#define RF_THREAD_ATTR_SETSTACKSIZE(_attr_,_sz_) ;
#define RF_THREAD_ATTR_SETSCHED(_attr_,_sched_)  ;
#define RF_CREATE_ATTR_THREAD(_handle_, _attr_, _func_, _arg_) ;

#define RF_CREATE_THREAD(_handle_, _func_, _arg_)  1

#endif /* SIMULATE */

struct RF_ThreadGroup_s {
  int  created;
  int  running;
  int  shutdown;
  RF_DECLARE_MUTEX(mutex)
  RF_DECLARE_COND(cond)
};

/*
 * Someone has started a thread in the group
 */
#define RF_THREADGROUP_STARTED(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->created++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}

/*
 * Thread announcing that it is now running
 */
#define RF_THREADGROUP_RUNNING(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->running++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
	RF_SIGNAL_COND((_g_)->cond); \
}

/*
 * Thread announcing that it is now done
 */
#define RF_THREADGROUP_DONE(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	(_g_)->shutdown++; \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
	RF_SIGNAL_COND((_g_)->cond); \
}

/*
 * Wait for all threads to start running
 */
#define RF_THREADGROUP_WAIT_START(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	while((_g_)->running < (_g_)->created) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}

/*
 * Wait for all threads to stop running
 */
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define RF_THREADGROUP_WAIT_STOP(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	RF_ASSERT((_g_)->running == (_g_)->created); \
	while((_g_)->shutdown < (_g_)->running) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}
#else
  /* XXX Note that we've removed the assert.  That should get put back
     in once we actually get something like a kernel thread running */
#define RF_THREADGROUP_WAIT_STOP(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	while((_g_)->shutdown < (_g_)->running) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}
#endif

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)

int rf_mutex_init(struct simplelock *);
int rf_mutex_destroy(struct simplelock *);
int _rf_create_managed_mutex(RF_ShutdownList_t **, struct simplelock *, 
			     char *, int);
int _rf_create_managed_cond(RF_ShutdownList_t **listp, int *, 
			    char *file, int line);

int rf_cond_init(int *c); /* XXX need to write?? */
int rf_cond_destroy(int *c); /* XXX need to write?? */
#endif
#endif /* !_RF__RF_THREADSTUFF_H_ */
