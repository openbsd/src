/*	$OpenBSD: lock_machdep.c,v 1.10 2008/06/26 05:42:10 ray Exp $	*/
/* $NetBSD: lock_machdep.c,v 1.1.2.3 2000/05/03 14:40:30 sommerfeld Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/*
 * Machine-dependent spin lock operations.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/lock.h>
#include <machine/cpufunc.h>

#include <ddb/db_output.h>

#ifdef LOCKDEBUG

void
__cpu_simple_lock_init(__cpu_simple_lock_t *lockp)
{
	*lockp = __SIMPLELOCK_UNLOCKED;
}

#if defined (DEBUG) && defined(DDB)
int spin_limit = 10000000;
#endif

void
__cpu_simple_lock(__cpu_simple_lock_t *lockp)
{
#if defined (DEBUG) && defined(DDB)	
	int spincount = 0;
#endif
	
	while (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_LOCKED) {
#if defined(DEBUG) && defined(DDB)
		spincount++;
		if (spincount == spin_limit) {
			extern int db_active;
			db_printf("spundry\n");
			if (db_active) {
				db_printf("but already in debugger\n");
			} else {
				Debugger();
			}
		}
#endif
	}
}

int
__cpu_simple_lock_try(__cpu_simple_lock_t *lockp)
{

	if (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_UNLOCKED)
		return (1);
	return (0);
}

void
__cpu_simple_unlock(__cpu_simple_lock_t *lockp)
{
	*lockp = __SIMPLELOCK_UNLOCKED;
}

#endif

int
rw_cas_486(volatile unsigned long *p, unsigned long o, unsigned long n)
{
	return (i486_atomic_cas_int((u_int *)p, o, n) != o);
}

#ifdef MULTIPROCESSOR
 void
__mp_lock_init(struct __mp_lock *lock)
{
	lock->mpl_cpu = NULL;
	lock->mpl_count = 0;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

extern void Debugger(void);
extern int db_printf(const char *, ...)
    __attribute__((__format__(__kprintf__,1,2)));

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static __inline void
__mp_lock_spin(struct __mp_lock *mpl)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_count != 0)
		SPINLOCK_SPIN_HOOK;
#else
	int ticks = __mp_lock_spinout;

	while (mpl->mpl_count != 0 && ticks-- > 0)
		SPINLOCK_SPIN_HOOK;

	if (ticks == 0) {
		db_printf("__mp_lock(0x%x): lock spun out", mpl);
		Debugger();
	}
#endif
}

void
__mp_lock(struct __mp_lock *mpl)
{
	/*
	 * Please notice that mpl_count gets incremented twice for the
	 * first lock. This is on purpose. The way we release the lock
	 * in mp_unlock is to decrement the mpl_count and then check if
	 * the lock should be released. Since mpl_count is what we're
	 * spinning on, decrementing it in mpl_unlock to 0 means that
	 * we can't clear mpl_cpu, because we're no longer holding the
	 * lock. In theory mpl_cpu doesn't need to be cleared, but it's
	 * safer to clear it and besides, setting mpl_count to 2 on the
	 * first lock makes most of this code much simpler.
	 */

	while (1) {
		int ef = read_eflags();

		disable_intr();
		if (i486_atomic_cas_int(&mpl->mpl_count, 0, 1) == 0) {
			mpl->mpl_cpu = curcpu();
		}

		if (mpl->mpl_cpu == curcpu()) {
			mpl->mpl_count++;
			write_eflags(ef);
			break;
		}
		write_eflags(ef);

		__mp_lock_spin(mpl);
	}
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	int ef = read_eflags();

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	disable_intr();	
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		mpl->mpl_count = 0;
	}
	write_eflags(ef);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 1;
	int ef = read_eflags();

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	disable_intr();
	mpl->mpl_cpu = NULL;
	mpl->mpl_count = 0;
	write_eflags(ef);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 2;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all_but_one(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	mpl->mpl_count = 2;

	return (rv);
}

void
__mp_acquire_count(struct __mp_lock *mpl, int count)
{
	while (count--)
		__mp_lock(mpl);
}

int
__mp_lock_held(struct __mp_lock *mpl)
{
	return mpl->mpl_cpu == curcpu();
}

#endif
