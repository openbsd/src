/*	$OpenBSD: lock_machdep.c,v 1.18 2015/05/30 08:41:30 kettenis Exp $	*/
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

/*
 * Machine-dependent spin lock operations.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/lock.h>
#include <machine/cpufunc.h>

#include <ddb/db_output.h>

#ifdef MULTIPROCESSOR
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

static inline u_int
fetch_and_add(u_int *var, u_int value)
{
	asm volatile("lock; xaddl %%eax, %2;"
	    : "=a" (value)
	    : "a" (value), "m" (*var)
	    : "memory");
 
	return (value);
}

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

void
__mp_lock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	long ef = read_eflags();

	disable_intr();
	if (cpu->mplc_depth++ == 0)
		cpu->mplc_ticket = fetch_and_add(&mpl->mpl_users, 1);
	write_eflags(ef);

	__mp_lock_spin(mpl, cpu->mplc_ticket);
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	int ef = read_eflags();

#ifdef MP_LOCKDEBUG
	if (!__mp_lock_held(mpl)) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		Debugger();
	}
#endif

	disable_intr();
	if (--cpu->mplc_depth == 0)
		mpl->mpl_ticket++;
	write_eflags(ef);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	int ef = read_eflags();
	int rv;

	disable_intr();
	rv = cpu->mplc_depth;
	cpu->mplc_depth = 0;
	mpl->mpl_ticket++;
	write_eflags(ef);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	int rv = cpu->mplc_depth - 1;

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

#endif
