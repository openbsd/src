/*	$OpenBSD: cpu.h,v 1.65 2011/04/10 03:56:38 guenther Exp $	*/
/*	$NetBSD: cpu.h,v 1.1 2003/04/26 18:39:39 fvdl Exp $	*/

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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

/*
 * Definitions unique to x86-64 cpu support.
 */
#include <machine/frame.h>
#include <machine/segments.h>
#include <machine/intrdefs.h>
#include <machine/cacheinfo.h>

#ifdef MULTIPROCESSOR
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#include <sys/device.h>
#include <sys/lock.h>
#include <sys/sched.h>
#include <sys/sensors.h>

#ifdef _KERNEL

struct x86_64_tss;
struct cpu_info {
	struct device *ci_dev;
	struct cpu_info *ci_self;
	struct schedstate_percpu ci_schedstate; /* scheduler state */
	struct cpu_info *ci_next;

	struct proc *ci_curproc;
	struct simplelock ci_slock;
	u_int ci_cpuid;
	u_int ci_apicid;
	u_int32_t ci_randseed;

	u_int64_t ci_scratch;

	struct proc *ci_fpcurproc;
	struct proc *ci_fpsaveproc;
	int ci_fpsaving;

	struct pcb *ci_curpcb;
	struct pcb *ci_idle_pcb;

	struct intrsource *ci_isources[MAX_INTR_SOURCES];
	u_int32_t	ci_ipending;
	int		ci_ilevel;
	int		ci_idepth;
	u_int32_t	ci_imask[NIPL];
	u_int32_t	ci_iunmask[NIPL];
#ifdef DIAGNOSTIC
	int		ci_mutex_level;
#endif

	volatile u_int	ci_flags;
	u_int32_t	ci_ipis;

	u_int32_t	ci_feature_flags;
	u_int32_t	ci_feature_eflags;
	u_int32_t	ci_signature;
	u_int32_t	ci_family;
	u_int32_t	ci_model;
	u_int32_t	ci_cflushsz;
	u_int64_t	ci_tsc_freq;

	struct cpu_functions *ci_func;
	void (*cpu_setup)(struct cpu_info *);
	void (*ci_info)(struct cpu_info *);

	int		ci_want_resched;

	struct x86_cache_info ci_cinfo[CAI_COUNT];

	struct	x86_64_tss *ci_tss;
	char		*ci_gdt;

	volatile int	ci_ddb_paused;
#define CI_DDB_RUNNING		0
#define CI_DDB_SHOULDSTOP	1
#define CI_DDB_STOPPED		2
#define CI_DDB_ENTERDDB		3
#define CI_DDB_INDDB		4

	volatile int ci_setperf_state;
#define CI_SETPERF_READY	0
#define CI_SETPERF_SHOULDSTOP	1
#define CI_SETPERF_INTRANSIT	2
#define CI_SETPERF_DONE		3

	struct ksensordev	ci_sensordev;
	struct ksensor		ci_sensor;
};

#define CPUF_BSP	0x0001		/* CPU is the original BSP */
#define CPUF_AP		0x0002		/* CPU is an AP */ 
#define CPUF_SP		0x0004		/* CPU is only processor */  
#define CPUF_PRIMARY	0x0008		/* CPU is active primary processor */

#define CPUF_PRESENT	0x1000		/* CPU is present */
#define CPUF_RUNNING	0x2000		/* CPU is running */
#define CPUF_PAUSE	0x4000		/* CPU is paused in DDB */
#define CPUF_GO		0x8000		/* CPU should start running */

#define PROC_PC(p)	((p)->p_md.md_regs->tf_rip)

extern struct cpu_info cpu_info_primary;
extern struct cpu_info *cpu_info_list;

#define CPU_INFO_ITERATOR		int
#define CPU_INFO_FOREACH(cii, ci)	for (cii = 0, ci = cpu_info_list; \
					    ci != NULL; ci = ci->ci_next)

#define CPU_INFO_UNIT(ci)	((ci)->ci_dev ? (ci)->ci_dev->dv_unit : 0)

/*      
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern void need_resched(struct cpu_info *);
#define clear_resched(ci) (ci)->ci_want_resched = 0

#if defined(MULTIPROCESSOR)

#define MAXCPUS		64	/* bitmask; can be bumped to 64 */

#define CPU_STARTUP(_ci)	((_ci)->ci_func->start(_ci))
#define CPU_STOP(_ci)		((_ci)->ci_func->stop(_ci))
#define CPU_START_CLEANUP(_ci)	((_ci)->ci_func->cleanup(_ci))

#define curcpu()	({struct cpu_info *__ci;                  \
			asm volatile("movq %%gs:8,%0" : "=r" (__ci)); \
			__ci;})
#define cpu_number()	(curcpu()->ci_cpuid)

#define CPU_IS_PRIMARY(ci)	((ci)->ci_flags & CPUF_PRIMARY)

extern struct cpu_info *cpu_info[MAXCPUS];

void cpu_boot_secondary_processors(void);
void cpu_init_idle_pcbs(void);    

void cpu_unidle(struct cpu_info *);

#else /* !MULTIPROCESSOR */

#define MAXCPUS		1

#ifdef _KERNEL
extern struct cpu_info cpu_info_primary;

#define curcpu()		(&cpu_info_primary)

#define cpu_unidle(ci)

#endif

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_number()		0
#define CPU_IS_PRIMARY(ci)	1

#endif	/* MULTIPROCESSOR */

#endif /* _KERNEL */

#include <machine/psl.h>

#ifdef MULTIPROCESSOR
#include <sys/mplock.h>
#endif

#define aston(p)	((p)->p_md.md_astpending = 1)

#define curpcb		curcpu()->ci_curpcb

/*
 * Arguments to hardclock, softclock and statclock
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 */
#define clockframe intrframe

#define	CLKF_USERMODE(frame)	USERMODE((frame)->if_cs, (frame)->if_rflags)
#define CLKF_PC(frame)		((frame)->if_rip)
#define CLKF_INTR(frame)	(curcpu()->ci_idepth > 1)

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)		((p)->p_md.md_regs->tf_rip)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	aston(p)

void signotify(struct proc *);

/*
 * We need a machine-independent name for this.
 */
extern void (*delay_func)(int);
struct timeval;

#define DELAY(x)		(*delay_func)(x)
#define delay(x)		(*delay_func)(x)


#ifdef _KERNEL
extern int biosbasemem;
extern int biosextmem;
extern int cpu;
extern int cpu_feature;
extern int cpu_ecxfeature;
extern int cpu_id;
extern char cpu_vendor[];
extern int cpuid_level;
extern int cpuspeed;

/* identcpu.c */
void	identifycpu(struct cpu_info *);
int	cpu_amd64speed(int *);
void cpu_probe_features(struct cpu_info *);

/* machdep.c */
void	dumpconf(void);
void	cpu_reset(void);
void	x86_64_proc0_tss_ldt_init(void);
void	x86_64_bufinit(void);
void	x86_64_init_pcb_tss_ldt(struct cpu_info *);
void	cpu_proc_fork(struct proc *, struct proc *);
int	amd64_pa_used(paddr_t);
extern void (*cpu_idle_enter_fcn)(void);
extern void (*cpu_idle_cycle_fcn)(void);
extern void (*cpu_idle_leave_fcn)(void);

struct region_descriptor;
void	lgdt(struct region_descriptor *);

struct pcb;
void	savectx(struct pcb *);
void	switch_exit(struct proc *, void (*)(struct proc *));
void	proc_trampoline(void);
void	child_trampoline(void);

/* clock.c */
extern void (*initclock_func)(void);
void	startclocks(void);
void	rtcstart(void);
void	rtcstop(void);
void	i8254_delay(int);
void	i8254_initclocks(void);
void	i8254_startclock(void);
void	i8254_inittimecounter(void);
void	i8254_inittimecounter_simple(void);

/* i8259.c */
void	i8259_default_setup(void);


void cpu_init_msrs(struct cpu_info *);


/* trap.c */
void	child_return(void *);

/* dkcsum.c */
void	dkcsumattach(void);

/* bus_machdep.c */
void x86_bus_space_init(void);
void x86_bus_space_mallocok(void);

/* powernow-k8.c */
void k8_powernow_init(struct cpu_info *);
void k8_powernow_setperf(int);

void est_init(struct cpu_info *);
void est_setperf(int);

#ifdef MULTIPROCESSOR
/* mp_setperf.c */
void mp_setperf_init(void);
#endif

#endif /* _KERNEL */

/* 
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_BIOS		2	/* BIOS variables */
#define	CPU_BLK2CHR		3	/* convert blk maj into chr one */
#define	CPU_CHR2BLK		4	/* convert chr maj into blk one */
#define CPU_ALLOWAPERTURE	5	/* allow mmap of /dev/xf86 */
#define CPU_CPUVENDOR		6	/* cpuid vendor string */
#define CPU_CPUID		7	/* cpuid */
#define CPU_CPUFEATURE		8	/* cpuid features */
#define CPU_APMWARN		9	/* APM battery warning percentage */
#define CPU_KBDRESET		10	/* keyboard reset under pcvt */
#define CPU_APMHALT		11	/* halt -p hack */
#define CPU_XCRYPT		12	/* supports VIA xcrypt in userland */
#define CPU_LIDSUSPEND		13	/* lid close causes a suspend */
#define CPU_MAXID		14	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "bios", CTLTYPE_INT }, \
	{ "blk2chr", CTLTYPE_STRUCT }, \
	{ "chr2blk", CTLTYPE_STRUCT }, \
	{ "allowaperture", CTLTYPE_INT }, \
	{ "cpuvendor", CTLTYPE_STRING }, \
	{ "cpuid", CTLTYPE_INT }, \
	{ "cpufeature", CTLTYPE_INT }, \
	{ "apmwarn", CTLTYPE_INT }, \
	{ "kbdreset", CTLTYPE_INT }, \
	{ "apmhalt", CTLTYPE_INT }, \
	{ "xcrypt", CTLTYPE_INT }, \
	{ "lidsuspend", CTLTYPE_INT }, \
}

/*
 * Default cr4 flags.
 * Doesn't really belong here, but doesn't really belong anywhere else
 * either. Just to avoid painful mismatches of cr4 flags since they are
 * set in three different places.
 */
#define CR4_DEFAULT (CR4_PAE|CR4_PGE|CR4_PSE|CR4_OSFXSR|CR4_OSXMMEXCPT)

#endif /* !_MACHINE_CPU_H_ */
