/*	$OpenBSD: cpu.h,v 1.9 2020/06/10 19:06:53 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

/*
 * User-visible definitions
 */

/* Nothing yet */

#ifdef _KERNEL

/*
 * Kernel-only definitions
 */

#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/psl.h>

#include <sys/device.h>
#include <sys/sched.h>

struct cpu_info {
	struct device	*ci_dev;
	struct cpu_info	*ci_next;
	struct schedstate_percpu ci_schedstate;

	struct proc	*ci_curproc;

#define CPUSAVE_LEN	9
	register_t	ci_tempsave[CPUSAVE_LEN];

	uint64_t	ci_lasttb;
	uint64_t	ci_nexttimerevent;
	uint64_t	ci_nextstatevent;
	int		ci_statspending;
	
	volatile int 	ci_cpl;
	uint32_t	ci_ipending;
#ifdef DIAGNOSTIC
	int		ci_mutex_level;
#endif
	int		ci_want_resched;

	uint32_t	ci_randseed;
};

extern struct cpu_info cpu_info_primary;

register struct cpu_info *__curcpu asm("r13");
#define curcpu()	__curcpu

#define MAXCPUS			1
#define CPU_IS_PRIMARY(ci)	1
#define CPU_INFO_UNIT(ci)	0
#define CPU_INFO_ITERATOR	int
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)
#define cpu_number()		0

#define CLKF_INTR(frame)	0
#define CLKF_USERMODE(frame)	0
#define CLKF_PC(frame)		0

#define aston(p)		((p)->p_md.md_astpending = 1)
#define need_proftick(p)	aston(p)

#define cpu_kick(ci)
#define cpu_unidle(ci)
#define CPU_BUSY_CYCLE()	do {} while (0)
#define signotify(p)		setsoftast()

unsigned int cpu_rnd_messybits(void);

void need_resched(struct cpu_info *);
#define clear_resched(ci)	((ci)->ci_want_resched = 0)

void delay(u_int);
#define DELAY(x)	delay(x)

#define setsoftast()		aston(curcpu()->ci_curproc)

#define PROC_STACK(p)		0
#define PROC_PC(p)		0

static inline void
intr_enable(void)
{
	mtmsr(mfmsr() | PSL_EE);
}

static inline u_long
intr_disable(void)
{
	u_long msr;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	return msr;
}

static inline void
intr_restore(u_long msr)
{
	mtmsr(msr);
}

#endif /* _KERNEL */

#endif /* _MACHINE_CPU_H_ */
