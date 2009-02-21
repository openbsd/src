/*	$OpenBSD: mplock.c,v 1.2 2009/02/21 18:37:48 miod Exp $	*/

/*
 * Copyright (c) 2004 Niklas Hallqvist.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/asm_macro.h>
#include <machine/lock.h>

#include <ddb/db_output.h>

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

#define	SPINLOCK_SPIN_HOOK	do { /* nothing */ } while (0)

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
	struct cpu_info *ci = curcpu();
	uint32_t psr;
	uint gcsr;

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

	for (;;) {
		psr = (*ci->ci_mp_atomic_begin)(&mpl->mpl_lock, &gcsr);

		if (mpl->mpl_count == 0) {
			mpl->mpl_count = 1;
			mpl->mpl_cpu = ci;
		}
		if (mpl->mpl_cpu == ci) {
			mpl->mpl_count++;
			(*ci->ci_mp_atomic_end)(psr, &mpl->mpl_lock, gcsr);
			break;
		}
		(*ci->ci_mp_atomic_end)(psr, &mpl->mpl_lock, gcsr);

		__mp_lock_spin(mpl);
	}
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	struct cpu_info *ci = curcpu();
	u_int32_t psr;
	uint gcsr;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != ci) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	psr = (*ci->ci_mp_atomic_begin)(&mpl->mpl_lock, &gcsr);
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		mpl->mpl_count = 0;
	}
	(*ci->ci_mp_atomic_end)(psr, &mpl->mpl_lock, gcsr);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	struct cpu_info *ci = curcpu();
	u_int32_t psr;
	uint gcsr;
	int rv;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != ci) {
		db_printf("__mp_release_all(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	psr = (*ci->ci_mp_atomic_begin)(&mpl->mpl_lock, &gcsr);
	rv = mpl->mpl_count - 1;
	mpl->mpl_cpu = NULL;
	mpl->mpl_count = 0;
	(*ci->ci_mp_atomic_end)(psr, &mpl->mpl_lock, gcsr);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	struct cpu_info *ci = curcpu();
	u_int32_t psr;
	uint gcsr;
	int rv;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != ci) {
		db_printf("__mp_release_all_but_one(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	psr = (*ci->ci_mp_atomic_begin)(&mpl->mpl_lock, &gcsr);
	rv = mpl->mpl_count - 2;
	mpl->mpl_count = 2;
	(*ci->ci_mp_atomic_end)(psr, &mpl->mpl_lock, gcsr);

	return (rv);
}
