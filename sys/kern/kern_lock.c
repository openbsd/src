/*	$OpenBSD: kern_lock.c,v 1.44 2014/07/09 13:32:00 guenther Exp $	*/

/* 
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code contains ideas from software contributed to Berkeley by
 * Avadis Tevanian, Jr., Michael Wayne Young, and the Mach Operating
 * System project at Carnegie-Mellon University.
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
 *	@(#)kern_lock.c	8.18 (Berkeley) 5/21/95
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <sys/sched.h>

#ifdef MP_LOCKDEBUG
/* CPU-dependent timing, this needs to be settable from ddb. */
int __mp_lock_spinout = 200000000;
#endif

/*
 * Initialize a lock; required before use.
 */
void
lockinit(struct lock *lkp, int prio, char *wmesg, int timo, int flags)
{
	KASSERT(flags == 0);

	memset(lkp, 0, sizeof(struct lock));
	rrw_init(&lkp->lk_lck, wmesg);
}

int
lockstatus(struct lock *lkp)
{
	switch (rrw_status(&lkp->lk_lck)) {
	case RW_WRITE:
		return (LK_EXCLUSIVE);
	case RW_WRITE_OTHER:
		return (LK_EXCLOTHER);
	case RW_READ:
		return (LK_SHARED);
	case 0:
	default:
		return (0);
	}
}

int
lockmgr(struct lock *lkp, u_int flags, void *notused)
{
	int rwflags = 0;

	KASSERT(!((flags & (LK_SHARED|LK_EXCLUSIVE)) ==
	    (LK_SHARED|LK_EXCLUSIVE)));
	KASSERT(!((flags & (LK_CANRECURSE|LK_RECURSEFAIL)) ==
	    (LK_CANRECURSE|LK_RECURSEFAIL)));
	KASSERT((flags & LK_RELEASE) ||
	    (flags & (LK_SHARED|LK_EXCLUSIVE|LK_DRAIN)));

	if (flags & LK_RELEASE) {
		rrw_exit(&lkp->lk_lck);
		return (0);
	}
	if (flags & LK_SHARED)
		rwflags |= RW_READ;
	if (flags & (LK_EXCLUSIVE|LK_DRAIN))
		rwflags |= RW_WRITE;
	if (flags & LK_RECURSEFAIL)
		rwflags |= RW_RECURSEFAIL;
	if (flags & LK_NOWAIT)
		rwflags |= RW_NOSLEEP;

	return (rrw_enter(&lkp->lk_lck, rwflags));

}

#if defined(MULTIPROCESSOR)
/*
 * Functions for manipulating the kernel_lock.  We put them here
 * so that they show up in profiles.
 */

struct __mp_lock kernel_lock;

void
_kernel_lock_init(void)
{
	__mp_lock_init(&kernel_lock);
}

/*
 * Acquire/release the kernel lock.  Intended for use in the scheduler
 * and the lower half of the kernel.
 */

void
_kernel_lock(void)
{
	SCHED_ASSERT_UNLOCKED();
	__mp_lock(&kernel_lock);
}

void
_kernel_unlock(void)
{
	__mp_unlock(&kernel_lock);
}
#endif /* MULTIPROCESSOR */
