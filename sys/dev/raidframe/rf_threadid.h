/*	$OpenBSD: rf_threadid.h,v 1.1 1999/01/11 14:29:53 niklas Exp $	*/
/*	$NetBSD: rf_threadid.h,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
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

/* rf_threadid.h
 *
 * simple macros to register and lookup integer identifiers for threads.
 * must include pthread.h before including this
 *
 * This is one of two places where the pthreads package is used explicitly.
 * The other is in threadstuff.h
 *
 * none of this is used in the kernel, so it all gets compiled out if KERNEL is defined
 */

/* :  
 * Log: rf_threadid.h,v 
 * Revision 1.17  1996/08/12 20:11:17  jimz
 * fix up for AIX4
 *
 * Revision 1.16  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.15  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.14  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.13  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.12  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.11  1996/05/20  16:13:46  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.10  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.9  1996/05/17  13:29:06  jimz
 * did a dance on get_threadid such that it will do the pthread_attr_t -> int
 * assignment without warnings, even on really anal compilers
 *
 * Revision 1.8  1995/12/06  15:15:00  root
 * added copyright info
 *
 */

#ifndef _RF__RF_THREADID_H_
#define _RF__RF_THREADID_H_

#ifdef _KERNEL
#define KERNEL
#endif

#ifndef SIMULATE
#ifndef KERNEL

/*
 * User
 */

#include "rf_threadstuff.h"

extern int rf_numThrsRegistered;
extern pthread_key_t rf_thread_id_key;
RF_DECLARE_EXTERN_MUTEX(rf_threadid_mutex)

#define RF_THREAD_MAX 200

/* these should be global since a function is declared.  Should be invoked at only one place in code */
#define RF_DECLARE_GLOBAL_THREADID           \
  int rf_numThrsRegistered = 0;           \
  pthread_key_t rf_thread_id_key;         \
  RF_DECLARE_MUTEX(rf_threadid_mutex)        \
  RF_Thread_t rf_regdThrs[RF_THREAD_MAX];    \
  void rf_ThreadIdEmptyFunc() {}

/* setup must be called exactly once, i.e. it can't be called by each thread */

#ifdef AIX
typedef void (*pthread_destructor_t)(void *);
#endif /* AIX */

#ifdef __osf__
#define rf_setup_threadid() { \
	extern void rf_ThreadIdEmptyFunc(); \
	pthread_keycreate(&rf_thread_id_key, (pthread_destructor_t) rf_ThreadIdEmptyFunc); \
	rf_mutex_init(&rf_threadid_mutex); /* XXX check return val */ \
	rf_numThrsRegistered = 0; \
}
#endif /* __osf__ */

#ifdef AIX
#define rf_setup_threadid() { \
	extern void rf_ThreadIdEmptyFunc(); \
	pthread_key_create(&rf_thread_id_key, (pthread_destructor_t) rf_ThreadIdEmptyFunc); \
	rf_mutex_init(&rf_threadid_mutex); /* XXX check return val */ \
	rf_numThrsRegistered = 0; \
}
#endif /* AIX */

#define rf_shutdown_threadid() { \
	rf_mutex_destroy(&rf_threadid_mutex); \
}

#ifdef __osf__
typedef pthread_addr_t RF_THID_cast_t;
#endif /* __osf__ */

#ifdef AIX
typedef void *RF_THID_cast_t;
#endif /* AIX */

#define rf_assign_threadid()  {RF_LOCK_MUTEX(rf_threadid_mutex); \
                            if (pthread_setspecific(rf_thread_id_key, (RF_THID_cast_t) ((unsigned long)(rf_numThrsRegistered++)))) { RF_PANIC(); } \
                            RF_UNLOCK_MUTEX(rf_threadid_mutex);}

#ifdef __osf__
#define rf_get_threadid(_id_) { \
	RF_THID_cast_t _val; \
	unsigned long _val2; \
	if (pthread_getspecific(rf_thread_id_key, &_val)) \
		RF_PANIC(); \
	(_val2) = (unsigned long)_val; \
	(_id_) = (int)_val2; \
}
#endif /* __osf__ */

#ifdef AIX
#define rf_get_threadid(_id_) { \
	RF_THID_cast_t _val; \
	unsigned long _val2; \
	_val = pthread_getspecific(rf_thread_id_key); \
	(_val2) = (unsigned long)_val; \
	(_id_) = (int)_val2; \
}
#endif /* AIX */

#else  /* KERNEL */

/*
 * Kernel
 */

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <kern/task.h>
#include <kern/thread.h>
#include <mach/machine/vm_param.h>
#endif

#define RF_DECLARE_GLOBAL_THREADID
#define rf_setup_threadid()
#define rf_shutdown_threadid()
#define rf_assign_threadid()



#if defined(__NetBSD__) || defined(__OpenBSD__)

#define rf_get_threadid(_id_) _id_ = 0;

#else
#define rf_get_threadid(_id_) { \
  thread_t thread = current_thread(); \
  _id_ = (int)(((thread->thread_self)>>(8*sizeof(int *)))&0x0fffffff); \
}
#endif /* __NetBSD__ || __OpenBSD__ */
#endif  /* KERNEL */

#else  /* SIMULATE */

/*
 * Simulator
 */

#include "rf_diskevent.h"

#define RF_DECLARE_GLOBAL_THREADID
#define rf_setup_threadid()
#define rf_shutdown_threadid()
#define rf_assign_threadid()

#define rf_get_threadid(_id_)  _id_ = rf_GetCurrentOwner()

#endif  /* SIMULATE */
#endif /* !_RF__RF_THREADID_H_ */
