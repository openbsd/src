#ifndef _RX_MACHDEP_
#define _RX_MACHDEP_

/* $I$d$ */
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*        Copyright Transarc Corporation 1993  - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that neither the name of IBM nor the name  *
* of Transarc be used in advertising or publicity pertaining to            *
* distribution of the software without specific, written prior permission. *
*                                                                          *
* IBM AND TRANSARC DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,   *
* INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO   *
* EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL     *
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
* THIS SOFTWARE.                                                           *
****************************************************************************
*/


#if	defined(AFS_SUN5_ENV) && defined(KERNEL)
#if            defined(AFS_FINEGR_SUNLOCK)
#define	RX_ENABLE_LOCKS	1
#else
#undef	RX_ENABLE_LOCKS
#endif
#include <sys/tiuser.h>
#include <sys/t_lock.h>
#include <sys/mutex.h>
#endif

#ifndef	AFS_AOS_ENV
#define	ADAPT_PERF
#ifndef	AFS_SUN5_ENV
#define MISCMTU
#ifndef __CYGWIN32__
#define ADAPT_MTU
#endif
#endif
#endif

#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
#define PIN(a, b) pin(a, b);
#define UNPIN(a, b) unpin(a, b);
#else
#define PIN(a, b) ;
#define UNPIN(a, b) ;
#endif

#if	defined(AFS_GLOBAL_SUNLOCK) && defined(KERNEL)
extern kmutex_t afs_rxglobal_lock;

#define GLOBAL_LOCK() mutex_enter(&afs_rxglobal_lock)
#define GLOBAL_UNLOCK() mutex_exit(&afs_rxglobal_lock)
#else
#define GLOBAL_LOCK()
#define GLOBAL_UNLOCK()
#endif

#ifdef	RX_ENABLE_LOCKS
extern kmutex_t afs_termStateLock;
extern kcondvar_t afs_termStateCv;

#define RX_MUTEX_DESTROY(a) mutex_destroy(a)
#define RX_MUTEX_ENTER(a) mutex_enter(a)
#define RX_MUTEX_EXIT(a)  mutex_exit(a)
#define RX_MUTEX_INIT(a,b,c,d)  mutex_init(a,b,c,d)
#else
#define MObtainWriteLock(a)
#define MReleaseWriteLock(a)
#define RX_MUTEX_DESTROY(a)
#define RX_MUTEX_ENTER(a)
#define RX_MUTEX_EXIT(a)
#define RX_MUTEX_INIT(a,b,c,d)
#endif

#ifndef AFS_AIX32_ENV
#define IFADDR2SA(f) (&((f)->ifa_addr))
#else				       /* AFS_AIX32_ENV */
#define IFADDR2SA(f) ((f)->ifa_addr)
#endif


#endif				       /* _RX_MACHDEP_ */
