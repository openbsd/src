/*	$OpenBSD: cpu.h,v 1.33 2010/11/27 19:41:45 miod Exp $	*/
/*	$NetBSD: cpu.h,v 1.24 1997/03/15 22:25:15 pk Exp $ */

/*
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
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _SPARC_CPU_H_
#define _SPARC_CPU_H_

/*
 * CTL_MACHDEP definitions.
 */
#define CPU_LED_BLINK	1	/* int: twiddle the power LED */
 		/*	2	   formerly int: vsyncblank */
#define CPU_CPUTYPE	3	/* int: cpu type */
#define CPU_V8MUL	4
#define CPU_MAXID	5	/* 4 valid machdep IDs */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "led_blink", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "cputype", CTLTYPE_INT }, \
	{ "v8mul", CTLTYPE_INT }, \
}

#ifdef _KERNEL
/*
 * Exported definitions unique to SPARC cpu support.
 */

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/intr.h>
#include <sparc/sparc/intreg.h>
#include <sparc/sparc/cpuvar.h>

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
	u_int	ipl;		/* actual interrupt priority level */
	u_int	fp;		/* %fp at interrupt */
};
typedef struct clockframe clockframe;

extern int eintstack[];

#define	CLKF_USERMODE(framep)	(((framep)->psr & PSR_PS) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	((framep)->fp < (u_int)eintstack)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int	want_resched;		/* resched() was called */
#define	need_resched(ci)		(want_resched = 1, want_ast = 1)
#define clear_resched(ci) 	want_resched = 0
extern int	want_ast;

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)		((p)->p_md.md_tf->tf_pc)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the sparc, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	do { want_ast = 1; } while (0)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		(want_ast = 1)

extern int	foundfpu;		/* true => we have an FPU */

/* auxreg.c */
void led_blink(void *);
/* scf.c */
void scfblink(void *);
/* disksubr.c */
struct dkbad;
int isbad(struct dkbad *bt, int, int, int);
/* machdep.c */
int	ldcontrolb(caddr_t);
void	dumpconf(void);
caddr_t	reserve_dumppages(caddr_t);
/* clock.c */
struct timeval;
void	lo_microtime(struct timeval *);
int	statintr(void *);
int	clockintr(void *);/* level 10 (clock) interrupt code */
int	statintr(void *);	/* level 14 (statclock) interrupt code */
/* locore.s */
struct fpstate;
void	savefpstate(struct fpstate *);
void	loadfpstate(struct fpstate *);
int	probeget(caddr_t, int);
void	write_all_windows(void);
void	write_user_windows(void);
void 	proc_trampoline(void);
struct pcb;
void	snapshot(struct pcb *);
struct frame *getfp(void);
int	xldcontrolb(caddr_t, struct pcb *);
void	copywords(const void *, void *, size_t);
void	qcopy(const void *, void *, size_t);
void	qzero(void *, size_t);
/* locore2.c */
void	remrunqueue(struct proc *);
/* trap.c */
void	pmap_unuse_final(struct proc *);
int	rwindow_save(struct proc *);
/* amd7930intr.s */
void	amd7930_trap(void);
#ifdef KGDB
/* zs_kgdb.c */
void zs_kgdb_init(void);
#endif
/* fb.c */
void	fb_unblank(void);
/* cache.c */
void cache_flush(caddr_t, u_int);
/* kgdb_stub.c */
#ifdef KGDB
void kgdb_attach(int (*)(void *), void (*)(void *, int), void *);
void kgdb_connect(int);
void kgdb_panic(void);
#endif
/* iommu.c */
void	iommu_enter(u_int, u_int);
void	iommu_remove(u_int, u_int);
/* emul.c */
struct trapframe;
int emulinstr(int, struct trapframe *);

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

#endif /* _KERNEL */
#endif /* _SPARC_CPU_H_ */
