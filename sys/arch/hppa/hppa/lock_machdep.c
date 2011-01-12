/*	$OpenBSD: lock_machdep.c,v 1.3 2011/01/12 21:11:12 kettenis Exp $	*/

/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/lock.h>
#include <machine/psl.h>

#include <ddb/db_output.h>

static __inline int
__cpu_cas(struct __mp_lock *mpl, volatile unsigned long *addr,
    unsigned long old, unsigned long new)
{
	volatile int *lock = (int *)(((vaddr_t)mpl->mpl_lock + 0xf) & ~0xf);
	volatile register_t old_lock = 0;
	int ret = 1;

	/* Note: lock must be 16-byte aligned. */
	asm volatile (
		"ldcws      0(%2), %0"
		: "=&r" (old_lock), "+m" (lock)
		: "r" (lock)
	);

	if (old_lock == MPL_UNLOCKED) {
		if (*addr == old) {
			*addr = new;
			asm("sync" ::: "memory");
			ret = 0;
		}
		*lock = MPL_UNLOCKED;
	}

	return ret;
}

void
__mp_lock_init(struct __mp_lock *lock)
{
	lock->mpl_lock[0] = MPL_UNLOCKED;
	lock->mpl_lock[1] = MPL_UNLOCKED;
	lock->mpl_lock[2] = MPL_UNLOCKED;
	lock->mpl_lock[3] = MPL_UNLOCKED;
	lock->mpl_cpu = NULL;
	lock->mpl_count = 0;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, this needs to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

#define SPINLOCK_SPIN_HOOK	/**/

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
	int s;

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
		s = hppa_intr_disable();
		if (__cpu_cas(mpl, &mpl->mpl_count, 0, 1) == 0) {
			__asm __volatile("sync" ::: "memory");
			mpl->mpl_cpu = curcpu();
		}
		if (mpl->mpl_cpu == curcpu()) {
			mpl->mpl_count++;
			hppa_intr_enable(s);
			break;
		}
		hppa_intr_enable(s);

		__mp_lock_spin(mpl);
	}
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_unlock(%p): lock not held - %p != %p\n",
		    mpl, mpl->mpl_cpu, curcpu());
		Debugger();
	}
#endif

	s = hppa_intr_disable();
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		__asm __volatile("sync" ::: "memory");
		mpl->mpl_count = 0;
	}
	hppa_intr_enable(s);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 1;
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all(%p): lock not held - %p != %p\n",
		    mpl, mpl->mpl_cpu, curcpu());
		Debugger();
	}
#endif

	s = hppa_intr_disable();
	mpl->mpl_cpu = NULL;
	__asm __volatile("sync" ::: "memory");
	mpl->mpl_count = 0;
	hppa_intr_enable(s);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 2;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all_but_one(%p): lock not held - "
		    "%p != %p\n", mpl, mpl->mpl_cpu, curcpu());
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

/*
 * Emulate a compare-and-swap instruction for rwlocks, by using a
 * __cpu_simple_lock as a critical section.
 *
 * Since we are only competing against other processors for rwlocks,
 * it is not necessary in this case to disable interrupts to prevent
 * reentrancy on the same processor.
 */

__cpu_simple_lock_t rw_cas_spinlock = __SIMPLELOCK_UNLOCKED;

int
rw_cas_hppa(volatile unsigned long *p, unsigned long o, unsigned long n)
{
	int rc = 0;

	__cpu_simple_lock(&rw_cas_spinlock);

	if (*p != o)
		rc = 1;
	else
		*p = n;

	__cpu_simple_unlock(&rw_cas_spinlock);

	return (rc);
}
