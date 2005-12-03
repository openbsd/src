/*	$OpenBSD: cpu.h,v 1.10 2005/12/03 14:30:05 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 */

#ifndef __M88K_CPU_H__
#define __M88K_CPU_H__

/*
 * CTL_MACHDEP definitinos.
 */
#define	CPU_CONSDEV	1	/* dev_t: console terminal device */
#define	CPU_MAXID	2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#ifdef _KERNEL

#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/intr.h>
#include <sys/sched.h>

#if defined(MULTIPROCESSOR)
#if !defined(MAX_CPUS) || MAX_CPUS > 4
#undef	MAX_CPUS
#define	MAX_CPUS	4
#endif
#else
#undef	MAX_CPUS
#define	MAX_CPUS	1
#endif

#ifndef _LOCORE

extern u_int max_cpus;

/*
 * Per-CPU data structure
 */

struct cpu_info {
	u_int	ci_alive;			/* nonzero if CPU present */

	struct proc *ci_curproc;		/* current process... */
	struct pcb *ci_curpcb;			/* ...and its pcb */

	u_int	ci_cpuid;			/* cpu number */
	u_int	ci_primary;			/* set if master cpu */

	struct schedstate_percpu ci_schedstate;	/* scheduling state */
	int	ci_want_resched;		/* need_resched() invoked */

	struct pcb *ci_idle_pcb;		/* idle pcb (and stack) */

	u_int	ci_intrdepth;			/* interrupt depth */

	u_long	ci_spin_locks;			/* spin locks counter */

	/* XXX ddb state? */
};

extern cpuid_t master_cpu;
extern struct cpu_info m88k_cpus[MAX_CPUS];

#define	CPU_INFO_ITERATOR	cpuid_t
#define	CPU_INFO_FOREACH(cii, ci) \
	for ((cii) = 0; (cii) < MAX_CPUS; (cii)++) \
		if (((ci) = &m88k_cpus[cii])->ci_alive != 0)
#define	CPU_INFO_UNIT(ci)	((ci)->ci_cpuid)

#if defined(MULTIPROCESSOR)

#define	curcpu() \
({									\
	struct cpu_info *cpuptr;					\
									\
	__asm__ __volatile__ ("ldcr %0, cr17" : "=r" (cpuptr));		\
	cpuptr;								\
})

#define	CPU_IS_PRIMARY(ci)	((ci)->ci_primary != 0)

void	cpu_boot_secondary_processors(void);

#else	/* MULTIPROCESSOR */

#define	curcpu()	(&m88k_cpus[0])
#define	CPU_IS_PRIMARY(ci)	1

#endif	/* MULTIPROCESSOR */

/*
 * The md code may hardcode this in some very specific situations.
 */
#if !defined(cpu_number)
#define	cpu_number()		curcpu()->ci_cpuid
#endif

#define	curpcb			curcpu()->ci_curpcb

#endif /* _LOCORE */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_exec(p)		do { /* nothing */ } while (0)
#define	cpu_wait(p)		do { /* nothing */ } while (0)
#define	cpu_swapin(p)		do { /* nothing */ } while (0)
#define	cpu_swapout(p)		do { /* nothing */ } while (0)

#if defined(MULTIPROCESSOR)
#include <sys/lock.h>
#include <sys/mplock.h>
#endif

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe. CLKF_INTR is only valid
 * if the process is in kernel mode. Clockframe is really trapframe,
 * so pointer to clockframe can be safely cast into a pointer to
 * trapframe.
 */
struct clockframe {
	struct trapframe tf;
};

#define	CLKF_USERMODE(framep)	(((framep)->tf.tf_epsr & PSR_MODE) == 0)
#define	CLKF_PC(framep)		((framep)->tf.tf_sxip & XIP_ADDR)
#define	CLKF_INTR(framep) \
	(((struct cpu_info *)(framep)->tf.tf_cpu)->ci_intrdepth > 1)

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

#define SIR_NET		1
#define SIR_CLOCK	2

#define setsoftint(x)	(ssir |= (x))
#define setsoftnet()	setsoftint(SIR_NET)
#define setsoftclock()	setsoftint(SIR_CLOCK)

#define siroff(x)	(ssir &= ~(x))

extern int	ssir;

#define	aston(p)	((p)->p_md.md_astpending = 1)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched(ci) \
do {									\
	ci->ci_want_resched = 1;					\
	if (ci->ci_curproc != NULL)					\
		aston(ci->ci_curproc);					\
} while (0)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the m88k, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, aston(p))

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		aston(p)

/*
 * switchframe - should be double word aligned.
 */
struct switchframe {
	u_int	sf_pc;			/* pc */
	void	*sf_proc;		/* proc pointer */
};

int badvaddr(vaddr_t, int);
void nmihand(void *);

#endif /* _KERNEL */
#endif /* __M88K_CPU_H__ */
