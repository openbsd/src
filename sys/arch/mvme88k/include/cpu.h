/*	$OpenBSD: cpu.h,v 1.14 2001/12/13 08:55:51 smurph Exp $ */
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
 */

#ifndef __MACHINE_CPU_H__
#define __MACHINE_CPU_H__

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

#include <machine/psl.h>
#include <machine/pcb.h>
#include <machine/board.h>

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

extern int intstack;

#define	CLKF_USERMODE(framep)	((((struct trapframe *)(framep))->epsr & PSR_MODE) == 0)
#define	CLKF_BASEPRI(framep)	(((struct trapframe *)(framep))->mask == 0)
#define	CLKF_PC(framep)		(((struct trapframe *)(framep))->sxip & ~3)
#define	CLKF_INTR(framep)	(((struct trapframe *)(framep))->r[31] > intstack)

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

/*
 * Internal IO space (iiomapsize).
 *
 * Internal IO space is mapped in the kernel from ``OBIO_START'' to
 * ``intiolimit'' (defined in locore.s).  Since it is always mapped,
 * conversion between physical and kernel virtual addresses is easy.
 */

#ifdef VIRTMAP 
/* This will do non 1:1 phys/virt memory mapping in the future - SPM */
#define	ISIIOVA(va) \
	((char *)(va) >= intiobase && (char *)(va) < intiolimit)
#define	IIOV(pa)	((int)(pa)-(int)iiomapbase+(int)intiobase)
#define	IIOP(va)	((int)(va)-(int)intiobase+(int)iiomapbase)
#define	IIOPOFF(pa)	((int)(pa)-(int)iiomapbase)

#else

#define	ISIIOVA(va) 1
#define	IIOV(pa)	((pa))
#define	IIOP(va)	((va))
#define	IIOPOFF(pa)	((int)(pa)-(int)OBIO_START)
#endif

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
#define	need_resched()		(want_resched = 1, want_ast = 1)

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

struct intrhand {
	int	(*ih_fn) __P((void *));
	void	*ih_arg;
	int	ih_ipl;
	int	ih_wantframe;
	struct	intrhand *ih_next;
};

int	intr_establish __P((int vec, struct intrhand *));

/*
 * return values for intr_establish()
 */

#define INTR_EST_SUCC 		0
#define INTR_EST_BADVEC		1
#define INTR_EST_BADIPL		2


/*
 * There are 256 possible vectors on a MVME1x7 platform (including
 * onboard and VME vectors. Use intr_establish() to register a
 * handler for the given vector. vector number is used to index
 * into the intr_handlers[] table.
 */
extern struct intrhand *intr_handlers[256];

/*
 * switchframe - should be double word aligned.
 */
struct switchframe {
	u_int	sf_pc;			/* pc */
	void	*sf_proc;		/* proc pointer */
};

/* This struct defines the machine dependant pointers */
struct md_p {
	void (*clock_init_func) __P((void));      /* interval clock init function */
	void (*statclock_init_func) __P((void));  /* statistics clock init function */
	void (*delayclock_init_func) __P((void)); /* delay clock init function */
	void (*delay_func) __P((void));           /* delay clock function */
	void (*interrupt_func) __P((u_int, struct m88100_saved_state *));       /* interrupt func */
	void (*fp_precise_func) __P((void));      /* floating point precise function */
	void (*trap_func) __P((unsigned, struct m88100_saved_state *));
	void (*syscall_func) __P((register_t, struct m88100_saved_state *));
	volatile u_char *intr_mask;
	volatile u_char *intr_ipl;
	volatile u_char *intr_src;
};

extern struct md_p md;


int badvaddr __P((vm_offset_t va, int size));
void nmihand __P((void *framep));

#endif /* _KERNEL */
#endif /* __MACHINE_CPU_H__ */
