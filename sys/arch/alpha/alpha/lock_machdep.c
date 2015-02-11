/*	$OpenBSD: lock_machdep.c,v 1.2 2015/02/11 01:10:48 dlg Exp $	*/

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
#include <machine/cpu.h>
#include <machine/lock.h>

#include <ddb/db_output.h>

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

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static inline void
__mp_lock_spin(struct __mp_lock *mpl)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_count != 0)
		SPINLOCK_SPIN_HOOK;
#else
	int ticks = __mp_lock_spinout;
	if (!CPU_IS_PRIMARY(curcpu()))				/* XXX */
		ticks += ticks;					/* XXX */

	while (mpl->mpl_count != 0 && --ticks > 0)
		SPINLOCK_SPIN_HOOK;

	if (ticks == 0) {
		db_printf("__mp_lock(%p): lock spun out", mpl);
		Debugger();
	}
#endif
}

void
__mp_lock(struct __mp_lock *mpl)
{
	int s;
	struct cpu_info *ci = curcpu();

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
		s = splhigh();
		if (__cpu_cas(&mpl->mpl_count, 0, 1) == 0) {
			alpha_mb();
			mpl->mpl_cpu = ci;
		}

		if (mpl->mpl_cpu == ci) {
			mpl->mpl_count++;
			splx(s);
			break;
		}
		splx(s);
		
		__mp_lock_spin(mpl);
	}
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	s = splhigh();
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		alpha_mb();
		mpl->mpl_count = 0;
	}
	splx(s);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 1;
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	s = splhigh();
	mpl->mpl_cpu = NULL;
	alpha_mb();
	mpl->mpl_count = 0;
	splx(s);

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
	return (mpl->mpl_cpu == curcpu());
}
