/*	$OpenBSD: cpu.h,v 1.3 1999/01/10 13:34:19 niklas Exp $	*/
/*	$NetBSD: cpu.h,v 1.12 1995/06/28 02:55:56 cgd Exp $	*/

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

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_MAXID	1	/* no valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
}

#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * Definitions unique to ns532 cpu support.
 *
 *   modified from 386 code for the pc532 by Phil Nelson (12/92)
 */

#include "machine/psl.h"
#include "machine/frame.h"

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapout(p)	/* nothing */
#define	cpu_wait(p)	/* nothing */


/*  XXX needed?  PAN
 * function vs. inline configuration;
 * these are defined to get generic functions
 * rather than inline or machine-dependent implementations
 */
#define	NEED_MINMAX		/* need {,i,l,ul}{min,max} functions */
#define	NEED_FFS		/* need ffs function */
#define	NEED_BCMP		/* need bcmp function */
#define	NEED_STRLEN		/* need strlen function */

/*
 * Arguments to hardclock, softclock and gatherstats encapsulate the
 * previous machine state in an opaque clockframe.  The ipl is here
 * as well for strayintr (see locore.s:interrupt and intr.c:strayintr).
 * Note that CLKF_INTR is valid only if CLKF_USERMODE is false.
 */
struct clockframe {
	u_int	psr;		/* psr before interrupt, excluding PSR_ET */
	u_int	pc;		/* pc at interrupt */
	u_int	npc;		/* npc at interrupt */
	u_int	ipv;		/* actual interrupt priority vector */
	u_int	fp;		/* %fp at interrupt */
	u_int   ipr;		/* Interrupt priority register befor int.  */
};
typedef struct clockframe clockframe;

extern int eintstack[];

#define	CLKF_USERMODE(framep)	(((framep)->psr & PSR_PS) == 0)
#define	CLKF_BASEPRI(framep)	(((framep)->psr & PSR_PIL) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	((framep)->fp < (u_int)eintstack)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
int	want_ast;
int	want_resched;		/* resched() was called */
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

/*
 * Software interrupt request `register'.
 */
extern unsigned int ssir;

#define SIR_NET		0x01
#define SIR_CLOCK	0x02
#define SIR_ZS		0x04
#define SIR_LE		0x08

#define setsoftnet()	ssir |= SIR_NET
#define setsoftclock()	ssir |= SIR_CLOCK
#define setsoftzs()	ssir |= SIR_ZS
#define setsoftle()	ssir |= SIR_LE

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
 * Only one process may own the FPU state.
 *
 * XXX this must be per-cpu (eventually)
 */
struct	proc *fpproc;		/* FPU owner */
int	foundfpu;		/* true => we have an FPU */

/*
 * Interrupt handler chains.  Interrupt handlers should return 0 for
 * ``not me'' or 1 (``I took care of it'').  intr_establish() inserts a
 * handler into the list.  The handler is called with its (single)
 * argument, or with a pointer to a clockframe if ih_arg is NULL.
 */
struct intrhand {
	int	(*ih_fun) __P((void *));
	void	*ih_arg;
	struct	intrhand *ih_next;
	unsigned int  ih_flags;
} *intrhand[256];
#define IH_CAN_DELAY 1

void	intr_establish __P((int level, unsigned int flags, struct intrhand *));
void	vmeintr_establish __P((int vec, int level, struct intrhand *));

/*
 * intr_fasttrap() is a lot like intr_establish, but is used for ``fast''
 * interrupt vectors (vectors that are not shared and are handled in the
 * trap window).  Such functions must be written in assembly.
 */
void	intr_fasttrap __P((int level, void (*vec)(void)));

/* disksubr.c */
struct dkbad;
int isbad __P((struct dkbad *bt, int, int, int));
#if 0
/* machdep.c */
int	ldcontrolb __P((caddr_t));
void	dumpconf __P((void));
caddr_t	reserve_dumppages __P((caddr_t));
/* clock.c */
struct timeval;
void	lo_microtime __P((struct timeval *));
int	statintr __P((void *));
int	clockintr __P((void *));/* level 10 (clock) interrupt code */
int	statintr __P((void *));	/* level 14 (statclock) interrupt code */
#endif
/* locore.s */
struct fpstate;
void	savefpstate __P((struct fpstate *));
void	loadfpstate __P((struct fpstate *));
int	probeget __P((caddr_t, int));
void	write_all_windows __P((void));
void	write_user_windows __P((void));
void 	proc_trampoline __P((void));
struct pcb;
void	snapshot __P((struct pcb *));
struct frame *getfp __P((void));
int	xldcontrolb __P((caddr_t, struct pcb *));
void	copywords __P((const void *, void *, size_t));
void	qcopy __P((const void *, void *, size_t));
void	qzero __P((void *, size_t));
/* locore2.c */
void	remrq __P((struct proc *));
/* trap.c */
void	kill_user_windows __P((struct proc *));
int	rwindow_save __P((struct proc *));
void	child_return __P((struct proc *));
#if 0
/* amd7930intr.s */
void	amd7930_trap __P((void));
/* cons.c */
int	cnrom __P((void));
/* zs.c */
void zsconsole __P((struct tty *, int, int, int (**)(struct tty *, int)));
#ifdef KGDB
void zs_kgdb_init __P((void));
#endif
/* fb.c */
void	fb_unblank __P((void));
/* cache.c */
void cache_flush __P((caddr_t, u_int));
/* kgdb_stub.c */
#ifdef KGDB
void kgdb_attach __P((int (*)(void *), void (*)(void *, int), void *));
void kgdb_connect __P((int));
void kgdb_panic __P((void));
#endif
/* pmap.c */
void	pmap_bootstrap __P((vm_offset_t));
vm_offset_t pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t, int));
/* iommu.c */
void	iommu_enter __P((u_int, u_int));
void	iommu_remove __P((u_int, u_int));
#endif

/*
 *
 * The SPARC has a Trap Base Register (TBR) which holds the upper 20 bits
 * of the trap vector table.  The next eight bits are supplied by the
 * hardware when the trap occurs, and the bottom four bits are always
 * zero (so that we can shove up to 16 bytes of executable code---exactly
 * four instructions---into each trap vector).
 *
 * The hardware allocates half the trap vectors to hardware and half to
 * software.
 *
 * Traps have priorities assigned (lower number => higher priority).
 */

struct trapvec {
	int	tv_instr[4];		/* the four instructions */
};
extern struct trapvec *trapbase;	/* the 256 vectors */

extern void wzero __P((void *, u_int));
extern void wcopy __P((const void *, void *, u_int));

extern label_t *nofault;

#endif /* _KERNEL */
#endif /* _CPU_H_ */
