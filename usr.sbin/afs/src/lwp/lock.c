/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/

/*
 * Locking routines for Vice.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
RCSID("$arla: lock.c,v 1.11 2002/06/01 17:47:47 lha Exp $");
#endif
#include "lwp.h"
#include "lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <err.h>


#define FALSE	0
#define TRUE	1

void
Lock_Init(struct Lock *lock)
{
    lock -> readers_reading = 0;
    lock -> excl_locked = 0;
    lock -> wait_states = 0;
    lock -> num_waiting = 0;
    lock -> thread_index = LWP_INVALIDTHREADID;
}

void
Lock_Obtain(struct Lock *lock, int how)
{
    switch (how) {

	case READ_LOCK:		lock->num_waiting++;
				do {
				    lock -> wait_states |= READ_LOCK;
				    LWP_WaitProcess(&lock->readers_reading);
				} while (lock->excl_locked & WRITE_LOCK);
				lock->num_waiting--;
				lock->readers_reading++;
				break;

	case WRITE_LOCK:	lock->num_waiting++;
				do {
				    lock -> wait_states |= WRITE_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->excl_locked || lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	case SHARED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= SHARED_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->excl_locked);
				lock->num_waiting--;
				lock->excl_locked = SHARED_LOCK;
				break;

	case BOOSTED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= WRITE_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	default:		
		errx(-1, "Can't happen, bad LOCK type: %d\n", how);
		/* NOTREACHED */
    }
}

/* release a lock, giving preference to new readers */
void
Lock_ReleaseR(struct Lock *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
	LWP_NoYieldSignal(&lock->readers_reading);
    }
    else {
	lock->wait_states &= ~EXCL_LOCKS;
	LWP_NoYieldSignal(&lock->excl_locked);
    }
}

/* release a lock, giving preference to new writers */
void
Lock_ReleaseW(struct Lock *lock)
{
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
	LWP_NoYieldSignal(&lock->excl_locked);
    }
    else {
	lock->wait_states &= ~READ_LOCK;
	LWP_NoYieldSignal(&lock->readers_reading);
    }
}

/* 
 * These next guys exist to provide an interface to drop a lock atomically with
 * blocking.  They're trivial to do in a non-preemptive LWP environment.
 */

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessR(char *addr, struct Lock *alock)
{
    ReleaseReadLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessW(char *addr, struct Lock *alock)
{
    ReleaseWriteLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessS(char *addr, struct Lock *alock)
{
    ReleaseSharedLock(alock);
    LWP_WaitProcess(addr);
}

#ifndef HAVE___FUNCTION__
#define __FUNCTION__ "unknown"
#endif

#define PANICPRINT(msg)	fprintf(stderr,"Panic in %s at %s:%d: %s\n", __FUNCTION__, __FILE__, __LINE__, msg)
    
static int
WillBlock (struct Lock *lock, int how) 
{
    switch (how) {
    case READ_LOCK:
	return ((lock)->excl_locked & WRITE_LOCK) || (lock)->wait_states;
    case WRITE_LOCK:
	return (lock)->excl_locked || (lock)->readers_reading;
    case SHARED_LOCK:
	return (lock)->excl_locked || (lock)->wait_states;
    default:
	PANICPRINT("unknown locking type");
	return 1; /* Block if unknown */
    }
}

static void
ObtainOneLock(struct Lock *lock, int how)
{
    switch (how) {
    case READ_LOCK: 
	if (!WillBlock(lock, how))
	    (lock) -> readers_reading++;
	else
	    Lock_Obtain(lock, how);
	break;
    case WRITE_LOCK:
    case SHARED_LOCK:
	if (!WillBlock(lock, how))
	    (lock) -> excl_locked = how;
	else
	    Lock_Obtain(lock, how);
	break;
    default:
	PANICPRINT("unknown locking type");
	fprintf(stderr,"%d\n",how);
    }
}

static void
ReleaseOneLock(struct Lock *lock, int how)
{
    switch(how) {
    case READ_LOCK:
	if (!--(lock)->readers_reading && (lock)->wait_states)
	    Lock_ReleaseW(lock);
	break;
    case WRITE_LOCK:
	(lock)->excl_locked &= ~WRITE_LOCK;
	if ((lock)->wait_states) Lock_ReleaseR(lock);
	break;
    case SHARED_LOCK:
	(lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
	if ((lock)->wait_states) Lock_ReleaseR(lock);
	break;
    default:
	PANICPRINT("unknown locking type");
    }
}

/*
 * Obtains two locks in a secure fashion (that's the idea)
 *
 * Takes two locks and two locking modes as parameters.
 */

void
_ObtainTwoLocks(struct Lock *lock1, int how1,
		struct Lock *lock2, int how2)
{
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    
start:
    ObtainOneLock(lock1, how1);
    if (WillBlock(lock2, how2)) {
	ReleaseOneLock(lock1, how1);
	IOMGR_Select(0, 0, 0, 0, &timeout);
	goto start;
    } else {
	ObtainOneLock(lock2, how2);
    }
}
