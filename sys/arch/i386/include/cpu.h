/*	$NetBSD: cpu.h,v 1.35 1996/05/05 19:29:26 christos Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _I386_CPU_H_
#define _I386_CPU_H_

/*
 * Definitions unique to i386 cpu support.
 */
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/segments.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_wait(p)			/* nothing */

/*
 * Arguments to hardclock, softclock and statclock
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 *
 * XXX intrframe has a lot of gunk we don't need.
 */
#define clockframe intrframe

#define	CLKF_USERMODE(frame)	USERMODE((frame)->if_cs, (frame)->if_eflags)
#define	CLKF_BASEPRI(frame)	((frame)->if_ppl == 0)
#define	CLKF_PC(frame)		((frame)->if_eip)
#define	CLKF_INTR(frame)	(IDXSEL((frame)->if_cs) == GICODE_SEL)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
int	want_resched;		/* resched() was called */
#define	need_resched()		(want_resched = 1, setsoftast())

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		setsoftast()

/*
 * We need a machine-independent name for this.
 */
#define	DELAY(x)		delay(x)
void	delay __P((int));

#ifdef I586_CPU
/*
 * High resolution clock support (Pentium only)
 */
void	calibrate_cyclecounter __P((void));
extern u_quad_t pentium_base_tsc;
#define CPU_CLOCKUPDATE(otime, ntime)					\
	do {								\
		if (pentium_mhz) {					\
			__asm __volatile("cli\n"			\
					 "movl (%3), %%eax\n"		\
					 "movl %%eax, (%2)\n"		\
					 "movl 4(%3), %%eax\n"		\
					 "movl %%eax, 4(%2)\n"		\
					 ".byte 0xf, 0x31\n"		\
					 "sti\n"			\
					 "#%0 %1 %2 %3"			\
					 : "=m" (*otime),		\
					 "=A" (pentium_base_tsc)	\
					 : "c" (otime), "b" (ntime));	\
		}							\
		else {							\
			*(otime) = *(ntime);				\
		}							\
	} while (0)
#endif
void	delay __P((int));

/*
 * pull in #defines for kinds of processors
 */
#include <machine/cputypes.h>

struct cpu_nameclass {
	char *cpu_name;
	int  cpu_class;
};

#ifdef _KERNEL
extern int cpu;
extern int cpu_class;
extern struct cpu_nameclass i386_cpus[];
#ifdef I586_CPU
extern int pentium_mhz;
#endif

/* autoconf.c */
void	configure __P((void));

/* machdep.c */
void	delay __P((int));
void	dumpconf __P((void));
void	cpu_reset __P((void));

/* locore.s */
struct region_descriptor;
void	lgdt __P((struct region_descriptor *));
void	fillw __P((short, void *, size_t));
short	fusword __P((u_short *));
int	susword __P((u_short *t, u_short));

struct pcb;
void	savectx __P((struct pcb *));
void	switch_exit __P((struct proc *));
void	proc_trampoline __P((void));

/* clock.c */
void	startrtclock __P((void));

/* npx.c */
void	npxdrop __P((void));
void	npxsave __P((void));

#if defined(MATH_EMULATE) || defined(GPL_MATH_EMULATE)
/* math_emulate.c */
int	math_emulate __P((struct trapframe *));
#endif

#ifdef USER_LDT
/* sys_machdep.h */
void	i386_user_cleanup __P((struct pcb *));
int	i386_get_ldt __P((struct proc *, char *, register_t *));
int	i386_set_ldt __P((struct proc *, char *, register_t *));
#endif

/* isa_machdep.c */
void	isa_defaultirq __P((void));
int	isa_nmi __P((void));

/* vm_machdep.c */
int	kvtop __P((caddr_t));

#ifdef VM86
/* vm86.c */
void	vm86_gpfault __P((struct proc *, int));
#endif /* VM86 */

/* trap.c */
void	child_return __P((struct proc *, struct trapframe));

#ifdef GENERIC
/* swapgeneric.c */
void	setconf __P((void));
#endif /* GENERIC */

#endif /* _KERNEL */

/* 
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#endif /* !_I386_CPU_H_ */
