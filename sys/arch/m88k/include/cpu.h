/*	$OpenBSD: cpu.h,v 1.5 2004/11/08 16:39:31 miod Exp $ */
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

#ifndef	MAX_CPUS
#define	MAX_CPUS	4
#endif

#include <machine/pcb.h>
#include <machine/psl.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_exec(p)	/* nothing */
#define	cpu_wait(p)	/* nothing */
#define	cpu_swapout(p)	/* nothing */

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

#define	CLKF_USERMODE(framep)	((((struct trapframe *)(framep))->tf_epsr & PSR_MODE) == 0)
#define	CLKF_PC(framep)		(((struct trapframe *)(framep))->tf_sxip & XIP_ADDR)
#define	CLKF_INTR(framep)	(((struct trapframe *)(framep))->tf_r[31] >= UADDR)

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

#define SIR_NET		1
#define SIR_CLOCK	2

#define setsoftint(x)	(ssir |= (x))
#define setsoftnet()	(ssir |= SIR_NET)
#define setsoftclock()	(ssir |= SIR_CLOCK)

#define siroff(x)	(ssir &= ~x)

extern int	ssir;
extern int	want_ast;

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int	want_resched;		/* resched() was called */
#define	need_resched(ci)		(want_resched = 1, want_ast = 1)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the sparc, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, want_ast = 1)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		(want_ast = 1)

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
