#ifndef LWP_LOCK_H
#define LWP_LOCK_H

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

/* $Id: lock.h,v 1.2 2000/09/11 14:41:08 art Exp $ */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/

/*
	Include file for using Vice locking routines.
*/

/* The following macros allow multi statement macros to be defined safely, i.e.
   - the multi statement macro can be the object of an if statement;
   - the call to the multi statement macro may be legally followed by a semi-colon.
   BEGINMAC and ENDMAC have been tested with both the portable C compiler and
   Hi-C.  Both compilers were from the Palo Alto 4.2BSD software releases, and
   both optimized out the constant loop code.  For an example of the use
   of BEGINMAC and ENDMAC, see the definition for ReleaseWriteLock, below.
   An alternative to this, using "if(1)" for BEGINMAC is not used because it
   may generate worse code with pcc, and may generate warning messages with hi-C.
*/

#define BEGINMAC do {
#define ENDMAC   } while (0)

/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct Lock {
    unsigned char	wait_states;	/* type of lockers waiting */
    unsigned char	excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned char	readers_reading;	/* # readers actually with read locks */
    unsigned char	num_waiting;	/* probably need this soon */
#ifdef LOCK_TRACE
    char *file;
    int line;
#endif /* LOCK_TRACE */
};

/* Prototypes */

void Lock_ReleaseR(register struct Lock *);
void Lock_ReleaseW(register struct Lock *);
void Lock_Obtain(register struct Lock *, int);
void Lock_Init(register struct Lock *);

#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4
/* this next is not a flag, but rather a parameter to Lock_Obtain */
#define BOOSTED_LOCK 6

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

#ifdef LOCK_DEBUG
#define DEBUGWRITE(message,lock) do { \
		fprintf(stderr,message,lock,__FILE__,__LINE__); } while (0)
#define DEBUGWRITE_4(message,lock1,how1,lock2,how2) do { \
		fprintf(stderr,message,lock1,how1,lock2,how2,__FILE__,__LINE__); } while (0)
#else
#define DEBUGWRITE(message,lock) do { ; } while (0)
#define DEBUGWRITE_4(message,lock1,how1,lock2,how2) do { ; } while (0)
#endif

#ifdef LOCK_TRACE
#define StoreFileLine(lock, f, l) \
	(lock)->file = f; \
	(lock)->line = l;
#else
#define StoreFileLine(lock, f, l)
#endif

#define ObtainReadLock(lock) \
	BEGINMAC \
	DEBUGWRITE("ObtainReadLock: %p at %s:%d starting\n",lock); \
	StoreFileLine(lock, __FILE__, __LINE__) \
	RealObtainReadLock(lock) \
	DEBUGWRITE("ObtainReadLock: %p at %s:%d got it\n",lock);\
	ENDMAC

#define ObtainWriteLock(lock) \
	BEGINMAC \
	DEBUGWRITE("ObtainWriteLock: %p at %s:%d starting\n",lock); \
	StoreFileLine(lock, __FILE__, __LINE__) \
	RealObtainWriteLock(lock) \
	DEBUGWRITE("ObtainWriteLock: %p at %s:%d got it\n",lock);\
	ENDMAC

#define ObtainSharedLock(lock) \
	BEGINMAC \
	DEBUGWRITE("ObtainSharedLock: %p at %s:%d starting\n",lock); \
	StoreFileLine(lock, __FILE__, __LINE__) \
	RealObtainSharedLock(lock) \
	DEBUGWRITE("ObtainSharedLock: %p at %s:%d got it\n",lock);\
	ENDMAC

#define BoostSharedLock(lock) \
	BEGINMAC \
	DEBUGWRITE("BoostSharedLock: %p at %s:%d starting\n",lock); \
	StoreFileLine(lock, __FILE__, __LINE__) \
	RealBoostSharedLock(lock) \
	DEBUGWRITE("BoostSharedLock: %p at %s:%d got it\n",lock);\
	ENDMAC

#define UnBoostSharedLock(lock) \
	BEGINMAC \
	DEBUGWRITE("UnBoostSharedLock: %p at %s:%d starting\n",lock); \
	StoreFileLine(lock, __FILE__, __LINE__) \
	RealUnboostSharedLock(lock) \
	DEBUGWRITE("UnBoostSharedLock: %p at %s:%d got it\n",lock);\
	ENDMAC

#define ReleaseReadLock(lock) \
	BEGINMAC \
        DEBUGWRITE("ReleaseReadLock: %p at %s:%d\n",lock);\
	RealReleaseReadLock(lock) \
	ENDMAC

#define ReleaseWriteLock(lock) \
	BEGINMAC \
        DEBUGWRITE("ReleaseWriteLock: %p at %s:%d\n",lock);\
	RealReleaseWriteLock(lock) \
	ENDMAC

#define ReleaseSharedLock(lock) \
	BEGINMAC \
        DEBUGWRITE("ReleaseSharedLock: %p at %s:%d\n",lock);\
	RealReleaseSharedLock(lock) \
	ENDMAC

#define RealObtainReadLock(lock) \
	if (!((lock)->excl_locked & WRITE_LOCK) && !(lock)->wait_states)\
	    (lock) -> readers_reading++;\
	else\
	    Lock_Obtain(lock, READ_LOCK);

#define RealObtainWriteLock(lock)\
	if (!(lock)->excl_locked && !(lock)->readers_reading)\
	    (lock) -> excl_locked = WRITE_LOCK;\
	else\
	    Lock_Obtain(lock, WRITE_LOCK);

#define RealObtainSharedLock(lock)\
	if (!(lock)->excl_locked && !(lock)->wait_states)\
	    (lock) -> excl_locked = SHARED_LOCK;\
	else\
	    Lock_Obtain(lock, SHARED_LOCK);

#define RealBoostSharedLock(lock)\
	if (!(lock)->readers_reading)\
	    (lock)->excl_locked = WRITE_LOCK;\
	else\
	    Lock_Obtain(lock, BOOSTED_LOCK);

/* this must only be called with a WRITE or boosted SHARED lock! */
#define RealUnboostSharedLock(lock)\
	    (lock)->excl_locked = SHARED_LOCK; \
	    if((lock)->wait_states) \
		Lock_ReleaseR(lock);

#define RealReleaseReadLock(lock)\
	    if (!--(lock)->readers_reading && (lock)->wait_states)\
		Lock_ReleaseW(lock) ;


#define RealReleaseWriteLock(lock)\
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    if ((lock)->wait_states) Lock_ReleaseR(lock);

/* can be used on shared or boosted (write) locks */
#define RealReleaseSharedLock(lock)\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    if ((lock)->wait_states) Lock_ReleaseR(lock);

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
#define LockWaiters(lock)\
	((int) ((lock)->num_waiting))

#define CheckLock(lock)\
	((lock)->excl_locked? (int) -1 : (int) (lock)->readers_reading)

#define WriteLocked(lock)\
	((lock)->excl_locked & WRITE_LOCK)

void LWP_WaitProcessR(register char *addr, register struct Lock *alock);
void LWP_WaitProcessW(register char *addr, register struct Lock *alock);
void LWP_WaitProcessS(register char *addr, register struct Lock *alock);

/* This attempts to obtain two locks in a secure fashion */

#define ObtainTwoLocks(lock1, how1, lock2, how2) \
    BEGINMAC\
        DEBUGWRITE_4("ObtainTwoLocks: %p(%d) %p(%d) at %s:%d\n",lock1,how1,lock2,how2);\
	_ObtainTwoLocks(lock1, how1, lock2, how2);\
    ENDMAC

void
_ObtainTwoLocks(register struct Lock *lock1, int how1,
	       register struct Lock *lock2, int how2);

#endif /* LOCK_H */
