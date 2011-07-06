/*      $OpenBSD: cpu.h,v 1.39 2011/07/06 20:42:05 miod Exp $      */
/*      $NetBSD: cpu.h,v 1.41 1999/10/21 20:01:36 ragge Exp $      */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden
 * All rights reserved.
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
 *      This product includes software developed at Ludd, University of Lule}
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_
#ifdef _KERNEL

#include <sys/cdefs.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/mtpr.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/uvax.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/intr.h>

#include <sys/sched.h>
struct cpu_info {
	struct proc *ci_curproc;

	struct schedstate_percpu ci_schedstate; /* scheduler state */
	u_int32_t 		ci_randseed;
#ifdef DIAGNOSTIC
	int	ci_mutex_level;
#endif
};

extern struct cpu_info cpu_info_store;
#define	curcpu()	(&cpu_info_store)
#define cpu_number()	0
#define CPU_IS_PRIMARY(ci)	1
#define CPU_INFO_ITERATOR	int
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)
#define CPU_INFO_UNIT(ci)	0
#define MAXCPUS	1
#define cpu_unidle(ci)

struct clockframe {
        int     pc;
        int     ps;
};

/*
 * All cpu-dependent info is kept in this struct. Pointer to the
 * struct for the current cpu is set up in locore.c.
 */
struct	cpu_dep {
	void	(*cpu_init)(void); /* pmap init before mm is on */
	int	(*cpu_mchk)(caddr_t);   /* Machine check handling */
	void	(*cpu_memerr)(void); /* Memory subsystem errors */
	    /* Autoconfiguration */
	void	(*cpu_conf)(void);
	int	(*cpu_clkread)(time_t);	/* Read cpu clock time */
	void	(*cpu_clkwrite)(void);	/* Write system time to cpu */
	short	cpu_vups;	/* speed of cpu */
	short	cpu_scbsz;	/* (estimated) size of system control block */
	void	(*cpu_halt)(void); /* Cpu dependent halt call */
	void	(*cpu_reboot)(int); /* Cpu dependent reboot call */
	void	(*cpu_clrf)(void); /* Clear cold/warm start flags */
	void	(*cpu_hardclock)(struct clockframe *);	/* hardclock handler */
};

extern struct cpu_dep *dep_call; /* Holds pointer to current CPU struct. */

extern struct device *booted_from;
extern int mastercpu;
extern int bootdev;

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */

#define need_resched(ci){ \
	want_resched++; \
	mtpr(AST_OK,PR_ASTLVL); \
	}
#define clear_resched(ci) 	want_resched = 0

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#define signotify(p)     mtpr(AST_OK,PR_ASTLVL);

extern	int     want_resched;   /* resched() was called */

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)	(((struct trapframe *)((p)->p_addr->u_pcb.framep))->pc)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the vax, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define need_proftick(p) mtpr(AST_OK,PR_ASTLVL)

#define	cpu_idle_enter()	do { /* nothing */ } while (0)
#define	cpu_idle_cycle()	do { /* nothing */ } while (0)
#define	cpu_idle_leave()	do { /* nothing */ } while (0)

/*
 * This defines the I/O device register space size in pages.
 */
#define	IOSPSZ	((64*1024) / VAX_NBPG)	/* 64k == 128 pages */

struct device;

extern char cpu_model[100];

/* Some low-level prototypes */
int	badaddr(caddr_t, int);
void	dumpconf(void);
void	dumpsys(void);
void	swapconf(void);
void	disk_printtype(int, int);
vaddr_t	vax_map_physmem(paddr_t, int);
void	vax_unmap_physmem(vaddr_t, int);
void	ioaccess(vaddr_t, paddr_t, int);
void	iounaccess(vaddr_t, int);
void	findcpu(void);
#ifdef DDB
int	kdbrint(int);
#endif
#endif /* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_LED_BLINK		2	/* int: display led patterns */
#define CPU_MAXID		3	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "led_blink", CTLTYPE_INT } \
}

#endif /* _MACHINE_CPU_H_ */
