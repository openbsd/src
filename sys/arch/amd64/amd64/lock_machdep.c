/*	$OpenBSD: lock_machdep.c,v 1.8 2015/03/14 03:38:46 jsg Exp $	*/

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

#include <machine/atomic.h>
#include <machine/lock.h>
#include <machine/cpufunc.h>

void
__mp_lock_init(struct __mp_lock *mpl)
{
	memset(mpl->mpl_cpus, 0, sizeof(mpl->mpl_cpus));
	mpl->mpl_users = 0;
	mpl->mpl_ticket = 0;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static __inline void
__mp_lock_spin(struct __mp_lock *mpl, u_int me)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_ticket != me)
		SPINLOCK_SPIN_HOOK;
#else
	int ticks = __mp_lock_spinout;

	while (mpl->mpl_ticket != me && --ticks > 0)
		SPINLOCK_SPIN_HOOK;

	if (ticks == 0) {
		db_printf("__mp_lock(%p): lock spun out", mpl);
		Debugger();
	}
#endif
}

static inline u_int
fetch_and_add(u_int *var, u_int value)
{
	asm volatile("lock; xaddl %%eax, %2;"
	    : "=a" (value)
	    : "a" (value), "m" (*var)
	    : "memory");

	return (value);
}

void
__mp_lock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	long rf = read_rflags();

	disable_intr();
	if (cpu->mplc_depth++ == 0)
		cpu->mplc_ticket = fetch_and_add(&mpl->mpl_users, 1);
	write_rflags(rf);

	__mp_lock_spin(mpl, cpu->mplc_ticket);
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	long rf = read_rflags();

#ifdef MP_LOCKDEBUG
	if (!__mp_lock_held(mpl)) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	disable_intr();	
	if (--cpu->mplc_depth == 0)
		mpl->mpl_ticket++;
	write_rflags(rf);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	long rf = read_rflags();
	int rv;

	disable_intr();
 	rv = cpu->mplc_depth;
	cpu->mplc_depth = 0;
	mpl->mpl_ticket++;
	write_rflags(rf);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	int rv = cpu->mplc_depth - 1;

#ifdef MP_LOCKDEBUG
	if (!__mp_lock_held(mpl)) {
		db_printf("__mp_release_all_but_one(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	cpu->mplc_depth = 1;

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
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];

	return (cpu->mplc_ticket == mpl->mpl_ticket && cpu->mplc_depth > 0);
}
