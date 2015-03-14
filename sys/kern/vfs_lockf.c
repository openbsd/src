/*	$OpenBSD: vfs_lockf.c,v 1.20 2015/03/14 03:38:51 jsg Exp $	*/
/*	$NetBSD: vfs_lockf.c,v 1.7 1996/02/04 02:18:21 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Scooter Morris at Genentech Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_lockf.c	8.3 (Berkeley) 1/6/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/pool.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/unistd.h>

struct pool lockfpool;

/*
 * This variable controls the maximum number of processes that will
 * be checked in doing deadlock detection.
 */
int maxlockdepth = MAXDEPTH;

#define SELF	0x1
#define OTHERS	0x2

#ifdef LOCKF_DEBUG

#define	DEBUG_SETLOCK		0x01
#define	DEBUG_CLEARLOCK		0x02
#define	DEBUG_GETLOCK		0x04
#define	DEBUG_FINDOVR		0x08
#define	DEBUG_SPLIT		0x10
#define	DEBUG_WAKELOCK		0x20

int	lockf_debug = DEBUG_SETLOCK|DEBUG_CLEARLOCK|DEBUG_WAKELOCK;

#define	DPRINTF(args, level)	if (lockf_debug & (level)) printf args
#else
#define	DPRINTF(args, level)
#endif

void
lf_init(void)
{
	pool_init(&lockfpool, sizeof(struct lockf), 0, 0, PR_WAITOK,
	    "lockfpl", NULL);
}

struct lockf *lf_alloc(uid_t, int);
void lf_free(struct lockf *);

/*
 * We enforce a limit on locks by uid, so that a single user cannot
 * run the kernel out of memory.  For now, the limit is pretty coarse.
 * There is no limit on root.
 *
 * Splitting a lock will always succeed, regardless of current allocations.
 * If you're slightly above the limit, we still have to permit an allocation
 * so that the unlock can succeed.  If the unlocking causes too many splits,
 * however, you're totally cutoff.
 */
int maxlocksperuid = 1024;

/*
 * 3 options for allowfail.
 * 0 - always allocate.  1 - cutoff at limit.  2 - cutoff at double limit.
 */
struct lockf *
lf_alloc(uid_t uid, int allowfail)
{
	struct uidinfo *uip;
	struct lockf *lock;

	uip = uid_find(uid);
	if (uid && allowfail && uip->ui_lockcnt >
	    (allowfail == 1 ? maxlocksperuid : (maxlocksperuid * 2)))
		return (NULL);
	uip->ui_lockcnt++;
	lock = pool_get(&lockfpool, PR_WAITOK);
	lock->lf_uid = uid;
	return (lock);
}

void
lf_free(struct lockf *lock)
{
	struct uidinfo *uip;

	uip = uid_find(lock->lf_uid);
	uip->ui_lockcnt--;
	pool_put(&lockfpool, lock);
}


/*
 * Do an advisory lock operation.
 */
int
lf_advlock(struct lockf **head, off_t size, caddr_t id, int op,
    struct flock *fl, int flags)
{
	struct proc *p = curproc;
	struct lockf *lock;
	off_t start, end;
	int error;

	/*
	 * Convert the flock structure into a start and end.
	 */
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;
	case SEEK_END:
		start = size + fl->l_start;
		break;
	default:
		return (EINVAL);
	}
	if (start < 0)
		return (EINVAL);
	if (fl->l_len == 0) {
		end = -1;
	} else {
		end = start + fl->l_len - 1;
		if (end < start)
			return (EINVAL);
	}

	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (*head == NULL) {
		if (op != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}

	lock = lf_alloc(p->p_ucred->cr_uid, op == F_SETLK ? 1 : 2);
	if (!lock)
		return (ENOLCK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = id;
	lock->lf_head = head;
	lock->lf_type = fl->l_type;
	lock->lf_next = NULL;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = flags;
	lock->lf_pid = (flags & F_POSIX) ? p->p_p->ps_pid : -1;

	switch (op) {
	case F_SETLK:
		return (lf_setlock(lock));
	case F_UNLCK:
		error = lf_clearlock(lock);
		lf_free(lock);
		return (error);
	case F_GETLK:
		error = lf_getlock(lock, fl);
		lf_free(lock);
		return (error);
	default:
		lf_free(lock);
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Set a byte-range lock.
 */
int
lf_setlock(struct lockf *lock)
{
	struct lockf *block;
	struct lockf **head = lock->lf_head;
	struct lockf **prev, *overlap, *ltmp;
	static char lockstr[] = "lockf";
	int ovcase, priority, needtolink, error;

#ifdef LOCKF_DEBUG
	if (lockf_debug & DEBUG_SETLOCK)
		lf_print("lf_setlock", lock);
#endif /* LOCKF_DEBUG */

	priority = PLOCK;
	if (lock->lf_type == F_WRLCK)
		priority += 4;
	priority |= PCATCH;
	/*
	 * Scan lock list for this file looking for locks that would block us.
	 */
	while ((block = lf_getblock(lock)) != NULL) {
		if ((lock->lf_flags & F_WAIT) == 0) {
			lf_free(lock);
			return (EAGAIN);
		}
		/*
		 * We are blocked. Since flock style locks cover
		 * the whole file, there is no chance for deadlock.
		 * For byte-range locks we must check for deadlock.
		 *
		 * Deadlock detection is done by looking through the
		 * wait channels to see if there are any cycles that
		 * involve us. MAXDEPTH is set just to make sure we
		 * do not go off into neverland.
		 */
		if ((lock->lf_flags & F_POSIX) &&
		    (block->lf_flags & F_POSIX)) {
			struct proc *wproc;
			struct lockf *waitblock;
			int i = 0;

			/* The block is waiting on something */
			wproc = (struct proc *)block->lf_id;
			while (wproc->p_wchan &&
			    (wproc->p_wmesg == lockstr) &&
			    (i++ < maxlockdepth)) {
				waitblock = (struct lockf *)wproc->p_wchan;
				/* Get the owner of the blocking lock */
				waitblock = waitblock->lf_next;
				if ((waitblock->lf_flags & F_POSIX) == 0)
					break;
				wproc = (struct proc *)waitblock->lf_id;
				if (wproc == (struct proc *)lock->lf_id) {
					lf_free(lock);
					return (EDEADLK);
				}
			}
		}
		/*
		 * For flock type locks, we must first remove
		 * any shared locks that we hold before we sleep
		 * waiting for an exclusive lock.
		 */
		if ((lock->lf_flags & F_FLOCK) && lock->lf_type == F_WRLCK) {
			lock->lf_type = F_UNLCK;
			(void)lf_clearlock(lock);
			lock->lf_type = F_WRLCK;
		}
		/*
		 * Add our lock to the blocked list and sleep until we're free.
		 * Remember who blocked us (for deadlock detection).
		 */
		lock->lf_next = block;
#ifdef LOCKF_DEBUG
		if (lockf_debug & DEBUG_SETLOCK) {
			lf_print("lf_setlock", lock);
			lf_print("lf_setlock: blocking on", block);
		}
#endif /* LOCKF_DEBUG */
		TAILQ_INSERT_TAIL(&block->lf_blkhd, lock, lf_block);
		error = tsleep(lock, priority, lockstr, 0);
		if (lock->lf_next != NULL) {
			TAILQ_REMOVE(&lock->lf_next->lf_blkhd, lock, lf_block);
			lock->lf_next = NULL;
		}
		if (error) {
			lf_free(lock);
			return (error);
		}
	}
	/*
	 * No blocks!!  Add the lock.  Note that we will
	 * downgrade or upgrade any overlapping locks this
	 * process already owns.
	 *
	 * Skip over locks owned by other processes.
	 * Handle any locks that overlap and are owned by ourselves.
	 */
	prev = head;
	block = *head;
	needtolink = 1;
	for (;;) {
		ovcase = lf_findoverlap(block, lock, SELF, &prev, &overlap);
		if (ovcase)
			block = overlap->lf_next;
		/*
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		switch (ovcase) {
		case 0: /* no overlap */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			break;
		case 1: /* overlap == lock */
			/*
			 * If downgrading lock, others may be
			 * able to acquire it.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK)
				lf_wakelock(overlap);
			overlap->lf_type = lock->lf_type;
			lf_free(lock);
			lock = overlap; /* for debug output below */
			break;
		case 2: /* overlap contains lock */
			/*
			 * Check for common starting point and different types.
			 */
			if (overlap->lf_type == lock->lf_type) {
				lf_free(lock);
				lock = overlap; /* for debug output below */
				break;
			}
			if (overlap->lf_start == lock->lf_start) {
				*prev = lock;
				lock->lf_next = overlap;
				overlap->lf_start = lock->lf_end + 1;
			} else
				lf_split(overlap, lock);
			lf_wakelock(overlap);
			break;
		case 3: /* lock contains overlap */
			/*
			 * If downgrading lock, others may be able to
			 * acquire it, otherwise take the list.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK) {
				lf_wakelock(overlap);
			} else {
				while ((ltmp =
				    TAILQ_FIRST(&overlap->lf_blkhd))) {
					TAILQ_REMOVE(&overlap->lf_blkhd, ltmp,
					    lf_block);
					ltmp->lf_next = lock;
					TAILQ_INSERT_TAIL(&lock->lf_blkhd,
					    ltmp, lf_block);
				}
			}
			/*
			 * Add the new lock if necessary and delete the overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap->lf_next;
				prev = &lock->lf_next;
				needtolink = 0;
			} else
				*prev = overlap->lf_next;
			lf_free(overlap);
			continue;
		case 4: /* overlap starts before lock */
			/*
			 * Add lock after overlap on the list.
			 */
			lock->lf_next = overlap->lf_next;
			overlap->lf_next = lock;
			overlap->lf_end = lock->lf_start - 1;
			prev = &lock->lf_next;
			lf_wakelock(overlap);
			needtolink = 0;
			continue;
		case 5: /* overlap ends after lock */
			/*
			 * Add the new lock before overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			overlap->lf_start = lock->lf_end + 1;
			lf_wakelock(overlap);
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & DEBUG_SETLOCK) {
		lf_print("lf_setlock: got the lock", lock);
	}
#endif /* LOCKF_DEBUG */
	return (0);
}

/*
 * Remove a byte-range lock on an inode.
 *
 * Generally, find the lock (or an overlap to that lock)
 * and remove it (or shrink it), then wakeup anyone we can.
 */
int
lf_clearlock(struct lockf *lock)
{
	struct lockf **head = lock->lf_head;
	struct lockf *lf = *head;
	struct lockf *overlap, **prev;
	int ovcase;

	if (lf == NULL)
		return (0);
#ifdef LOCKF_DEBUG
	if (lockf_debug & DEBUG_CLEARLOCK)
		lf_print("lf_clearlock", lock);
#endif /* LOCKF_DEBUG */
	prev = head;
	while ((ovcase = lf_findoverlap(lf, lock, SELF, &prev, &overlap))) {
		lf_wakelock(overlap);

		switch (ovcase) {
		case 1: /* overlap == lock */
			*prev = overlap->lf_next;
			lf_free(overlap);
			break;
		case 2: /* overlap contains lock: split it */
			if (overlap->lf_start == lock->lf_start) {
				overlap->lf_start = lock->lf_end + 1;
				break;
			}
			lf_split(overlap, lock);
			overlap->lf_next = lock->lf_next;
			break;
		case 3: /* lock contains overlap */
			*prev = overlap->lf_next;
			lf = overlap->lf_next;
			lf_free(overlap);			
			continue;
		case 4: /* overlap starts before lock */
			overlap->lf_end = lock->lf_start - 1;
			prev = &overlap->lf_next;
			lf = overlap->lf_next;
			continue;
		case 5: /* overlap ends after lock */
			overlap->lf_start = lock->lf_end + 1;
			break;
		}
		break;
	}
	return (0);
}

/*
 * Check whether there is a blocking lock,
 * and if so return its process identifier.
 */
int
lf_getlock(struct lockf *lock, struct flock *fl)
{
	struct lockf *block;

#ifdef LOCKF_DEBUG
	if (lockf_debug & DEBUG_CLEARLOCK)
		lf_print("lf_getlock", lock);
#endif /* LOCKF_DEBUG */

	if ((block = lf_getblock(lock)) != NULL) {
		fl->l_type = block->lf_type;
		fl->l_whence = SEEK_SET;
		fl->l_start = block->lf_start;
		if (block->lf_end == -1)
			fl->l_len = 0;
		else
			fl->l_len = block->lf_end - block->lf_start + 1;
		fl->l_pid = block->lf_pid;
	} else {
		fl->l_type = F_UNLCK;
	}
	return (0);
}

/*
 * Walk the list of locks for an inode and
 * return the first blocking lock.
 */
struct lockf *
lf_getblock(struct lockf *lock)
{
	struct lockf **prev, *overlap, *lf;

	prev = lock->lf_head;
	lf = *prev;
	while (lf_findoverlap(lf, lock, OTHERS, &prev, &overlap) != 0) {
		/*
		 * We've found an overlap, see if it blocks us
		 */
		if ((lock->lf_type == F_WRLCK || overlap->lf_type == F_WRLCK))
			return (overlap);
		/*
		 * Nope, point to the next one on the list and
		 * see if it blocks us
		 */
		lf = overlap->lf_next;
	}
	return (NULL);
}

/*
 * Walk the list of locks for an inode to
 * find an overlapping lock (if any).
 *
 * NOTE: this returns only the FIRST overlapping lock.  There
 *	 may be more than one.
 */
int
lf_findoverlap(struct lockf *lf, struct lockf *lock, int type,
    struct lockf ***prev, struct lockf **overlap)
{
	off_t start, end;

#ifdef LOCKF_DEBUG
	if (lf && lockf_debug & DEBUG_FINDOVR)
		lf_print("lf_findoverlap: looking for overlap in", lock);
#endif /* LOCKF_DEBUG */

	*overlap = lf;
	start = lock->lf_start;
	end = lock->lf_end;
	while (lf != NULL) {
		if (((type & SELF) && lf->lf_id != lock->lf_id) ||
		    ((type & OTHERS) && lf->lf_id == lock->lf_id)) {
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
#ifdef LOCKF_DEBUG
		if (lockf_debug & DEBUG_FINDOVR)
			lf_print("\tchecking", lf);
#endif /* LOCKF_DEBUG */
		/*
		 * OK, check for overlap
		 *
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */

		/* Case 0 */
		if ((lf->lf_end != -1 && start > lf->lf_end) ||
		    (end != -1 && lf->lf_start > end)) {
			DPRINTF(("no overlap\n"), DEBUG_FINDOVR);
			if ((type & SELF) && end != -1 && lf->lf_start > end)
				return (0);
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
		/* Case 1 */
		if ((lf->lf_start == start) && (lf->lf_end == end)) {
			DPRINTF(("overlap == lock\n"), DEBUG_FINDOVR);
			return (1);
		}
		/* Case 2 */
		if ((lf->lf_start <= start) &&
		    (lf->lf_end == -1 || (end != -1 && lf->lf_end >= end))) {
			DPRINTF(("overlap contains lock\n"), DEBUG_FINDOVR);
			return (2);
		}
		/* Case 3 */
		if (start <= lf->lf_start &&
		    (end == -1 || (lf->lf_end != -1 && end >= lf->lf_end))) {
			DPRINTF(("lock contains overlap\n"), DEBUG_FINDOVR);
			return (3);
		}
		/* Case 4 */
		if ((lf->lf_start < start) &&
		    ((lf->lf_end >= start) || (lf->lf_end == -1))) {
			DPRINTF(("overlap starts before lock\n"),
			    DEBUG_FINDOVR);
			return (4);
		}
		/* Case 5 */
		if ((lf->lf_start > start) && (end != -1) &&
		    ((lf->lf_end > end) || (lf->lf_end == -1))) {
			DPRINTF(("overlap ends after lock\n"), DEBUG_FINDOVR);
			return (5);
		}
		panic("lf_findoverlap: default");
	}
	return (0);
}

/*
 * Split a lock and a contained region into
 * two or three locks as necessary.
 */
void
lf_split(struct lockf *lock1, struct lockf *lock2)
{
	struct lockf *splitlock;

#ifdef LOCKF_DEBUG
	if (lockf_debug & DEBUG_SPLIT) {
		lf_print("lf_split", lock1);
		lf_print("splitting from", lock2);
	}
#endif /* LOCKF_DEBUG */
	/*
	 * Check to see if spliting into only two pieces.
	 */
	if (lock1->lf_start == lock2->lf_start) {
		lock1->lf_start = lock2->lf_end + 1;
		lock2->lf_next = lock1;
		return;
	}
	if (lock1->lf_end == lock2->lf_end) {
		lock1->lf_end = lock2->lf_start - 1;
		lock2->lf_next = lock1->lf_next;
		lock1->lf_next = lock2;
		return;
	}
	/*
	 * Make a new lock consisting of the last part of
	 * the encompassing lock
	 */
	splitlock = lf_alloc(lock1->lf_uid, 0);
	memcpy(splitlock, lock1, sizeof(*splitlock));
	splitlock->lf_start = lock2->lf_end + 1;
	splitlock->lf_block.tqe_next = NULL;
	TAILQ_INIT(&splitlock->lf_blkhd);
	lock1->lf_end = lock2->lf_start - 1;

	lock2->lf_next = splitlock;
	lock1->lf_next = lock2;
}

/*
 * Wakeup a blocklist
 */
void
lf_wakelock(struct lockf *lock)
{
	struct lockf *wakelock;

	while ((wakelock = TAILQ_FIRST(&lock->lf_blkhd))) {
		TAILQ_REMOVE(&lock->lf_blkhd, wakelock, lf_block);
		wakelock->lf_next = NULL;
		wakeup_one(wakelock);
	}
}

#ifdef LOCKF_DEBUG
/*
 * Print out a lock.
 */
void
lf_print(char *tag, struct lockf *lock)
{
	struct lockf	*block;
	
	printf("%s: lock %p for ", tag, lock);
	if (lock->lf_flags & F_POSIX)
		printf("proc %d", ((struct proc *)(lock->lf_id))->p_pid);
	else
		printf("id %p", lock->lf_id);
	printf(" %s, start %llx, end %llx",
		lock->lf_type == F_RDLCK ? "shared" :
		lock->lf_type == F_WRLCK ? "exclusive" :
		lock->lf_type == F_UNLCK ? "unlock" :
		"unknown", lock->lf_start, lock->lf_end);
	block = TAILQ_FIRST(&lock->lf_blkhd);
	if (block)
		printf(" block");
	TAILQ_FOREACH(block, &lock->lf_blkhd, lf_block)
		printf(" %p,", block);
	printf("\n");

}

void
lf_printlist(char *tag, struct lockf *lock)
{
	struct lockf *lf;

	printf("%s: Lock list:\n", tag);
	for (lf = *lock->lf_head; lf; lf = lf->lf_next) {
		printf("\tlock %p for ", lf);
		if (lf->lf_flags & F_POSIX)
			printf("proc %d", ((struct proc*)(lf->lf_id))->p_pid);
		else
			printf("id %p", lf->lf_id);
		printf(" %s, start %llx, end %llx",
			lf->lf_type == F_RDLCK ? "shared" :
			lf->lf_type == F_WRLCK ? "exclusive" :
			lf->lf_type == F_UNLCK ? "unlock" :
			"unknown", lf->lf_start, lf->lf_end);
		printf("\n");
	}
}
#endif /* LOCKF_DEBUG */
