/* $OpenBSD: cpu.h,v 1.43 2011/03/23 16:54:34 pirofti Exp $ */
/* $NetBSD: cpu.h,v 1.45 2000/08/21 02:03:12 thorpej Exp $ */

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

#ifndef NO_IEEE
typedef union alpha_s_float {
	u_int32_t i;
	u_int32_t frac: 23,
		exp:   8,
		sign:  1;
} s_float;

typedef union alpha_t_float {
	u_int64_t i;
	u_int64_t frac: 52,
		exp:  11,
		sign:  1;
} t_float;
#endif

/*
 * Exported definitions unique to Alpha cpu support.
 */

#include <machine/alpha_cpu.h>
#include <machine/frame.h>
#include <machine/param.h>

#ifdef _KERNEL

#include <machine/bus.h>
#include <machine/intr.h>
#include <sys/device.h>
#include <sys/sched.h>

struct pcb;
struct proc;
struct reg;
struct rpb;
struct trapframe;

extern u_long cpu_implver;		/* from IMPLVER instruction */
extern u_long cpu_amask;		/* from AMASK instruction */
extern int bootdev_debug;
extern int alpha_fp_sync_complete;
extern int alpha_unaligned_print, alpha_unaligned_fix, alpha_unaligned_sigbus;

void	XentArith(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentIF(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentInt(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentMM(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentRestart(void);					/* MAGIC */
void	XentSys(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentUna(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	alpha_init(u_long, u_long, u_long, u_long, u_long);
int	alpha_pa_access(u_long);
void	ast(struct trapframe *);
int	badaddr(void *, size_t);
int	badaddr_read(void *, size_t, void *);
u_int64_t console_restart(struct trapframe *);
void	do_sir(void);
void	dumpconf(void);
void	exception_return(void);					/* MAGIC */
void	frametoreg(struct trapframe *, struct reg *);
long	fswintrberr(void);					/* MAGIC */
void	init_bootstrap_console(void);
void	init_prom_interface(struct rpb *);
void	interrupt(unsigned long, unsigned long, unsigned long,
	    struct trapframe *);
void	machine_check(unsigned long, struct trapframe *, unsigned long,
	    unsigned long);
u_int64_t hwrpb_checksum(void);
void	hwrpb_restart_setup(void);
void	regdump(struct trapframe *);
void	regtoframe(struct reg *, struct trapframe *);
void	savectx(struct pcb *);
void    switch_exit(struct proc *);				/* MAGIC */
void	switch_trampoline(void);				/* MAGIC */
void	syscall(u_int64_t, struct trapframe *);
void	trap(unsigned long, unsigned long, unsigned long, unsigned long,
	    struct trapframe *);
void	trap_init(void);
void	enable_nsio_ide(bus_space_tag_t);

/* Multiprocessor glue; cpu.c */
struct cpu_info;
int	cpu_iccb_send(cpuid_t, const char *);
void	cpu_iccb_receive(void);
void	cpu_hatch(struct cpu_info *);
void	cpu_halt_secondary(unsigned long);
void	cpu_spinup_trampoline(void);				/* MAGIC */
void	cpu_pause(unsigned long);
void	cpu_resume(unsigned long);

/*
 * Machine check information.
 */
struct mchkinfo {
	__volatile int mc_expected;	/* machine check is expected */
	__volatile int mc_received;	/* machine check was received */
};

struct cpu_info {
	struct device *ci_dev;		/* pointer to our device */
	/*
	 * Public members.
	 */
	struct schedstate_percpu ci_schedstate;	/* scheduler state */
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif
#ifdef DIAGNOSTIC
	int	ci_mutex_level;
#endif
	struct proc *ci_curproc;	/* current owner of the processor */
	struct simplelock ci_slock;	/* lock on this data structure */
	cpuid_t ci_cpuid;		/* our CPU ID */
	struct cpu_info *ci_next;

	/*
	 * Private members.
	 */
	struct mchkinfo ci_mcinfo;	/* machine check info */
	struct proc *ci_fpcurproc;	/* current owner of the FPU */
	paddr_t ci_curpcb;		/* PA of current HW PCB */
	struct pcb *ci_idle_pcb;	/* our idle PCB */
	paddr_t ci_idle_pcb_paddr;	/* PA of idle PCB */
	struct cpu_softc *ci_softc;	/* pointer to our device */
	u_long ci_want_resched;		/* preempt current process */
	u_long ci_astpending;		/* AST is pending */
	u_long ci_intrdepth;		/* interrupt trap depth */
	struct trapframe *ci_db_regs;	/* registers for debuggers */
#if defined(MULTIPROCESSOR)
	u_long ci_flags;		/* flags; see below */
	u_long ci_ipis;			/* interprocessor interrupts pending */
#endif
	u_int32_t ci_randseed;
};

#define	CPUF_PRIMARY	0x01		/* CPU is primary CPU */
#define	CPUF_PRESENT	0x02		/* CPU is present */
#define	CPUF_RUNNING	0x04		/* CPU is running */
#define	CPUF_PAUSED	0x08		/* CPU is paused */
#define	CPUF_FPUSAVE	0x10		/* CPU is currently in fpusave_cpu() */

void	fpusave_cpu(struct cpu_info *, int);
void	fpusave_proc(struct proc *, int);

#define	CPU_INFO_UNIT(ci)	((ci)->ci_dev ? (ci)->ci_dev->dv_unit : 0)
#define	CPU_INFO_ITERATOR		int
#define	CPU_INFO_FOREACH(cii, ci)	for (cii = 0, ci = curcpu(); \
					    ci != NULL; ci = ci->ci_next)

#define MAXCPUS	ALPHA_MAXPROCS

#define cpu_unidle(ci)

#if defined(MULTIPROCESSOR)
extern	__volatile u_long cpus_running;
extern	__volatile u_long cpus_paused;
extern	struct cpu_info cpu_info[];

#define	curcpu()			((struct cpu_info *)alpha_pal_rdval())
#define	CPU_IS_PRIMARY(ci)		((ci)->ci_flags & CPUF_PRIMARY)

void	cpu_boot_secondary_processors(void);

void	cpu_pause_resume(unsigned long, int);
void	cpu_pause_resume_all(int);
#else /* ! MULTIPROCESSOR */
extern	struct cpu_info cpu_info_store;

#define	curcpu()	(&cpu_info_store)
#define	CPU_IS_PRIMARY(ci)	1
#endif /* MULTIPROCESSOR */

#define	curproc		curcpu()->ci_curproc
#define	fpcurproc	curcpu()->ci_fpcurproc
#define	curpcb		curcpu()->ci_curpcb

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_number()		alpha_pal_whami()

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.  On the Alpha, we use
 * what we push on an interrupt (a trapframe).
 */
struct clockframe {
	struct trapframe	cf_tf;
};
#define	CLKF_USERMODE(framep)						\
	(((framep)->cf_tf.tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) != 0)
#define	CLKF_PC(framep)		((framep)->cf_tf.tf_regs[FRAME_PC])

/*
 * This isn't perfect; if the clock interrupt comes in before the
 * r/m/w cycle is complete, we won't be counted... but it's not
 * like this statistic has to be extremely accurate.
 */
#define	CLKF_INTR(framep)	(curcpu()->ci_intrdepth)

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)	((p)->p_md.md_tf->tf_regs[FRAME_PC])

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#ifdef MULTIPROCESSOR
#define	need_resched(ci)						\
do {									\
	ci->ci_want_resched = 1;					\
	aston(curcpu());						\
} while (/*CONSTCOND*/0)
#define clear_resched(ci) (ci)->ci_want_resched = 0
#else
#define	need_resched(ci)						\
do {									\
	curcpu()->ci_want_resched = 1;					\
	aston(curcpu());						\
} while (/*CONSTCOND*/0)
#define clear_resched(ci) curcpu()->ci_want_resched = 0
#endif

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the Alpha, request an AST to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#ifdef notyet
#define	need_proftick(p)						\
do {									\
	aston((p)->p_cpu);						\
} while (/*CONSTCOND*/0)
#else
#define	need_proftick(p)						\
do {									\
	aston(curcpu());						\
} while (/*CONSTCOND*/0)
#endif

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#ifdef notyet
#define	signotify(p)	aston((p)->p_cpu)
#else
#define signotify(p)	aston(curcpu())
#endif

/*
 * XXXSMP
 * Should we send an AST IPI?  Or just let it handle it next time
 * it sees a normal kernel entry?  I guess letting it happen later
 * follows the `asynchronous' part of the name...
 */
#define	aston(ci)	((ci)->ci_astpending = 1)
#endif /* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_UNALIGNED_PRINT	3	/* int: print unaligned accesses */
#define	CPU_UNALIGNED_FIX	4	/* int: fix unaligned accesses */
#define	CPU_UNALIGNED_SIGBUS	5	/* int: SIGBUS unaligned accesses */
#define	CPU_BOOTED_KERNEL	6	/* string: booted kernel name */
#define	CPU_FP_SYNC_COMPLETE	7	/* int: always fixup sync fp traps */
#define CPU_CHIPSET		8	/* chipset information */
#define CPU_ALLOWAPERTURE	9
#define	CPU_LED_BLINK		10	/* int: blink leds on DEC 3000 */

#define	CPU_MAXID		11	/* valid machdep IDs */

#define CPU_CHIPSET_MEM		1	/* PCI memory address */
#define CPU_CHIPSET_BWX		2	/* PCI supports BWX */
#define CPU_CHIPSET_TYPE	3	/* PCI chipset name */
#define CPU_CHIPSET_DENSE	4	/* PCI chipset dense memory addr */
#define CPU_CHIPSET_PORTS	5	/* PCI port address */
#define CPU_CHIPSET_HAE_MASK	6	/* PCI chipset mask for HAE register */
#define CPU_CHIPSET_MAXID	7

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ 0, 0 }, \
	{ "unaligned_print", CTLTYPE_INT }, \
	{ "unaligned_fix", CTLTYPE_INT }, \
	{ "unaligned_sigbus", CTLTYPE_INT }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "fp_sync_complete", CTLTYPE_INT }, \
	{ "chipset", CTLTYPE_NODE }, \
	{ "allowaperture", CTLTYPE_INT }, \
	{ "led_blink", CTLTYPE_INT } \
}

#define CTL_CHIPSET_NAMES { \
	{ 0, 0 }, \
	{ "memory", CTLTYPE_QUAD }, \
	{ "bwx", CTLTYPE_INT }, \
	{ "type", CTLTYPE_STRING }, \
	{ "dense_base", CTLTYPE_QUAD }, \
	{ "ports_base", CTLTYPE_QUAD }, \
	{ "hae_mask", CTLTYPE_QUAD }, \
}

#ifdef _KERNEL

struct pcb;
struct proc;
struct reg;
struct rpb;
struct trapframe;

/* IEEE and VAX FP completion */

#ifndef NO_IEEE
void alpha_sts(int, s_float *);					/* MAGIC */
void alpha_stt(int, t_float *);					/* MAGIC */
void alpha_lds(int, s_float *);					/* MAGIC */
void alpha_ldt(int, t_float *);					/* MAGIC */

uint64_t alpha_read_fpcr(void);					/* MAGIC */
void alpha_write_fpcr(u_int64_t);				/* MAGIC */

u_int64_t alpha_read_fp_c(struct proc *);
void alpha_write_fp_c(struct proc *, u_int64_t);

int alpha_fp_complete(u_long, u_long, struct proc *, u_int64_t *);
#endif

void alpha_enable_fp(struct proc *, int);

#ifdef MULTIPROCESSOR
#include <sys/mplock.h>
#endif

#endif /* _KERNEL */
#endif /* _MACHINE_CPU_H_ */
