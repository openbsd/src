/*	$OpenBSD: lock_machdep.c,v 1.9 2020/04/15 08:09:00 mpi Exp $	*/

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
#include <sys/systm.h>
#include <sys/atomic.h>

#include <machine/cpu.h>
#include <machine/psl.h>

#include <ddb/db_output.h>

void
__ppc_lock_init(struct __ppc_lock *lock)
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

static __inline void
__ppc_lock_spin(struct __ppc_lock *mpl)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_count != 0)
		CPU_BUSY_CYCLE();
#else
	int nticks = __mp_lock_spinout;

	while (mpl->mpl_count != 0 && --nticks > 0)
		CPU_BUSY_CYCLE();

	if (nticks == 0) {
		db_printf("__ppc_lock(%p): lock spun out\n", mpl);
		db_enter();
	}
#endif
}

void
__ppc_lock(struct __ppc_lock *mpl)
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
		int s;

		s = ppc_intr_disable();
		if (atomic_cas_ulong(&mpl->mpl_count, 0, 1) == 0) {
			membar_enter();
			mpl->mpl_cpu = curcpu();
		}

		if (mpl->mpl_cpu == curcpu()) {
			mpl->mpl_count++;
			ppc_intr_enable(s);
			break;
		}
		ppc_intr_enable(s);

		__ppc_lock_spin(mpl);
	}
}

void
__ppc_unlock(struct __ppc_lock *mpl)
{
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__ppc_unlock(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	s = ppc_intr_disable();
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		membar_exit();
		mpl->mpl_count = 0;
	}
	ppc_intr_enable(s);
}

int
__ppc_release_all(struct __ppc_lock *mpl)
{
	int rv = mpl->mpl_count - 1;
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__ppc_release_all(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	s = ppc_intr_disable();
	mpl->mpl_cpu = NULL;
	membar_exit();
	mpl->mpl_count = 0;
	ppc_intr_enable(s);

	return (rv);
}

int
__ppc_release_all_but_one(struct __ppc_lock *mpl)
{
	int rv = mpl->mpl_count - 2;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__ppc_release_all_but_one(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	mpl->mpl_count = 2;

	return (rv);
}

void
__ppc_acquire_count(struct __ppc_lock *mpl, int count)
{
	while (count--)
		__ppc_lock(mpl);
}

int
__ppc_lock_held(struct __ppc_lock *mpl, struct cpu_info *ci)
{
	return mpl->mpl_cpu == ci;
}
