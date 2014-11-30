/*	$OpenBSD: lock_machdep.c,v 1.8 2014/11/30 22:26:14 kettenis Exp $	*/

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

#include <sys/atomic.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/lock.h>
#include <machine/psl.h>

#include <ddb/db_output.h>

void
__mp_lock_init(struct __mp_lock *mpl)
{
	memset(mpl->mpl_cpus, 0, sizeof(mpl->mpl_cpus));
	mpl->mpl_users = 0;
	mpl->mpl_ticket = 1;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

/*
 * On processors with multiple threads we force a thread switch.
 *
 * On UltraSPARC T2 and its successors, the optimal way to do this
 * seems to be to do three nop reads of %ccr.  This works on
 * UltraSPARC T1 as well, even though three nop casx operations seem
 * to be slightly more optimal.  Since these instructions are
 * effectively nops, executing them on earlier non-CMT processors is
 * harmless, so we make this the default.
 *
 * On SPARC T4 and later, we can use the processor-specific pause
 * instruction.
 *
 * On SPARC64 VI and its successors we execute the processor-specific
 * sleep instruction.
 */
static __inline void
__mp_lock_spin_hook(void)
{
	__asm volatile(
		"999:	rd	%%ccr, %%g0			\n"
		"	rd	%%ccr, %%g0			\n"
		"	rd	%%ccr, %%g0			\n"
		"	.section .sun4v_pause_patch, \"ax\"	\n"
		"	.word	999b				\n"
		"	.word	0xb7802080	! pause	128	\n"
		"	.word	999b + 4			\n"
		"	nop					\n"
		"	.word	999b + 8			\n"
		"	nop					\n"
		"	.previous				\n"
		"	.section .sun4u_mtp_patch, \"ax\"	\n"
		"	.word	999b				\n"
		"	.word	0x81b01060	! sleep		\n"
		"	.word	999b + 4			\n"
		"	nop					\n"
		"	.word	999b + 8			\n"
		"	nop					\n"
		"	.previous				\n"
		: : : "memory");
}

#define SPINLOCK_SPIN_HOOK __mp_lock_spin_hook()

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
		db_printf("__mp_lock(0x%x): lock spun out", mpl);
		Debugger();
	}
#endif
}

void
__mp_lock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	u_int64_t s;

	s = intr_disable();
	if (cpu->mplc_depth++ == 0)
		cpu->mplc_ticket = atomic_inc_int_nv(&mpl->mpl_users);
	intr_restore(s);

	__mp_lock_spin(mpl, cpu->mplc_ticket);
	sparc_membar(LoadLoad | LoadStore);
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	u_int64_t s;

	s = intr_disable();
	if (--cpu->mplc_depth == 0) {
		mpl->mpl_ticket++;
		sparc_membar(StoreStore | LoadStore);
	}
	intr_restore(s);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	u_int64_t s;
	int rv;

	s = intr_disable();
	rv = cpu->mplc_depth;
	cpu->mplc_depth = 0;
	mpl->mpl_ticket++;
	sparc_membar(StoreStore | LoadStore);
	intr_restore(s);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	u_int64_t s;
	int rv;

	s = intr_disable();
	rv = cpu->mplc_depth;
	cpu->mplc_depth = 1;
	intr_restore(s);

	return (rv - 1);
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
