/*	$NetBSD: cpu.h,v 1.32 1995/12/24 01:08:02 mycroft Exp $	*/

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
#define	CLKF_INTR(frame)	(0)	/* XXX should have an interrupt stack */

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
#endif

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
