/*	$OpenBSD: rf_threadstuff.h,v 1.2 1999/02/16 00:03:32 niklas Exp $	*/
/*	$NetBSD: rf_threadstuff.h,v 1.3 1999/02/05 00:06:18 oster Exp $	*/
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

#ifndef _RF__RF_THREADSTUFF_H_
#define _RF__RF_THREADSTUFF_H_

#include "rf_types.h"
#include <sys/types.h>
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#endif

#define rf_create_managed_mutex(a,b) _rf_create_managed_mutex(a,b,__FILE__,__LINE__)
#define rf_create_managed_cond(a,b) _rf_create_managed_cond(a,b,__FILE__,__LINE__)
#define rf_init_managed_threadgroup(a,b) _rf_init_managed_threadgroup(a,b,__FILE__,__LINE__)
#define rf_init_threadgroup(a) _rf_init_threadgroup(a,__FILE__,__LINE__)
#define rf_destroy_threadgroup(a) _rf_destroy_threadgroup(a,__FILE__,__LINE__)

int     _rf_init_threadgroup(RF_ThreadGroup_t * g, char *file, int line);
int     _rf_destroy_threadgroup(RF_ThreadGroup_t * g, char *file, int line);
int
_rf_init_managed_threadgroup(RF_ShutdownList_t ** listp,
    RF_ThreadGroup_t * g, char *file, int line);

#include <sys/lock.h>
#define decl_simple_lock_data(a,b) a struct simplelock b;
#define simple_lock_addr(a) ((struct simplelock *)&(a))

typedef struct proc *RF_Thread_t;
typedef void *RF_ThreadArg_t;

#define RF_DECLARE_MUTEX(_m_)           decl_simple_lock_data(,(_m_))
#define RF_DECLARE_STATIC_MUTEX(_m_)    decl_simple_lock_data(static,(_m_))
#define RF_DECLARE_EXTERN_MUTEX(_m_)    decl_simple_lock_data(extern,(_m_))

#define RF_DECLARE_COND(_c_)            int _c_;
#define RF_DECLARE_STATIC_COND(_c_)     static int _c_;
#define RF_DECLARE_EXTERN_COND(_c_)     extern int _c_;

#define RF_LOCK_MUTEX(_m_)              simple_lock(&(_m_))
#define RF_UNLOCK_MUTEX(_m_)            simple_unlock(&(_m_))

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

struct RF_ThreadGroup_s {
	int     created;
	int     running;
	int     shutdown;
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
 /* XXX Note that we've removed the assert.  That should get put back in once
  * we actually get something like a kernel thread running */
#define RF_THREADGROUP_WAIT_STOP(_g_) { \
	RF_LOCK_MUTEX((_g_)->mutex); \
	while((_g_)->shutdown < (_g_)->running) { \
		RF_WAIT_COND((_g_)->cond, (_g_)->mutex); \
	} \
	RF_UNLOCK_MUTEX((_g_)->mutex); \
}
#endif


int     rf_mutex_init(struct simplelock *);
int     rf_mutex_destroy(struct simplelock *);
int 
_rf_create_managed_mutex(RF_ShutdownList_t **, struct simplelock *,
    char *, int);
int 
_rf_create_managed_cond(RF_ShutdownList_t ** listp, int *,
    char *file, int line);

int     rf_cond_init(int *c);	/* XXX need to write?? */
int     rf_cond_destroy(int *c);/* XXX need to write?? */
#endif				/* !_RF__RF_THREADSTUFF_H_ */
