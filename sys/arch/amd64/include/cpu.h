/*	$OpenBSD: cpu.h,v 1.1 2004/01/28 01:39:39 mickey Exp $	*/
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

#ifndef _AMD64_CPU_H_
#define _AMD64_CPU_H_

/*
 * Definitions unique to x86-64 cpu support.
 */
#include <machine/frame.h>
#include <machine/segments.h>
#include <machine/tss.h>
#include <machine/intrdefs.h>
#include <machine/cacheinfo.h>

#include <sys/device.h>
#include <sys/lock.h>

struct cpu_info {
	struct device *ci_dev;
	struct cpu_info *ci_self;
#if 0
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#endif
	struct cpu_info *ci_next;

	struct proc *ci_curproc;
	struct simplelock ci_slock;
	u_int ci_cpuid;
	u_int ci_apicid;
	u_long ci_spin_locks;
	u_long ci_simple_locks;

	u_int64_t ci_scratch;

	struct proc *ci_fpcurproc;
	int ci_fpsaving;

	volatile u_int32_t ci_tlb_ipi_mask;

	struct pcb *ci_curpcb;
	struct pcb *ci_idle_pcb;
	int ci_idle_tss_sel;

	struct intrsource *ci_isources[MAX_INTR_SOURCES];
	u_int32_t	ci_ipending;
	int		ci_ilevel;
	int		ci_idepth;
	u_int32_t	ci_imask[NIPL];
	u_int32_t	ci_iunmask[NIPL];

	paddr_t 	ci_idle_pcb_paddr;
	u_int		ci_flags;
	u_int32_t	ci_ipis;

	u_int32_t	ci_feature_flags;
	u_int32_t	ci_signature;
	u_int64_t	ci_tsc_freq;

	struct cpu_functions *ci_func;
	void (*cpu_setup)(struct cpu_info *);
	void (*ci_info)(struct cpu_info *);

	int		ci_want_resched;
	int		ci_astpending;
	struct trapframe *ci_ddb_regs;

	struct x86_cache_info ci_cinfo[CAI_COUNT];

	struct timeval 	ci_cc_time;
	int64_t		ci_cc_cc;
	int64_t		ci_cc_ms_delta;
	int64_t		ci_cc_denom;

	char		*ci_gdt;

	struct x86_64_tss	ci_doubleflt_tss;
	struct x86_64_tss	ci_ddbipi_tss;

	char *ci_doubleflt_stack;
	char *ci_ddbipi_stack;

	struct evcnt ci_ipi_events[X86_NIPI];
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
#define CPU_INFO_FOREACH(cii, ci)	cii = 0, ci = cpu_info_list; \
					ci != NULL; ci = ci->ci_next

#if defined(MULTIPROCESSOR)

#define X86_MAXPROCS		32	/* bitmask; can be bumped to 64 */

#define CPU_STARTUP(_ci)	((_ci)->ci_func->start(_ci))
#define CPU_STOP(_ci)		((_ci)->ci_func->stop(_ci))
#define CPU_START_CLEANUP(_ci)	((_ci)->ci_func->cleanup(_ci))

#define curcpu()	({struct cpu_info *__ci;                  \
			asm volatile("movq %%gs:8,%0" : "=r" (__ci)); \
			__ci;})
#define cpu_number()	(curcpu()->ci_cpuid)

#define CPU_IS_PRIMARY(ci)	((ci)->ci_flags & CPUF_PRIMARY)

extern struct cpu_info *cpu_info[X86_MAXPROCS];

void cpu_boot_secondary_processors(void);
void cpu_init_idle_pcbs(void);    


/*      
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern void need_resched(struct cpu_info *);

#else /* !MULTIPROCESSOR */

#define X86_MAXPROCS		1

#ifdef _KERNEL
extern struct cpu_info cpu_info_primary;

#define curcpu()		(&cpu_info_primary)

#endif

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_number()		0
#define CPU_IS_PRIMARY(ci)	1

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */

#ifdef MULTIPROCESSOR
#define need_resched(ci)						\
do {									\
	struct cpu_info *__ci = (ci);					\
	__ci->ci_want_resched = 1;					\
	if (__ci->ci_curproc != NULL)					\
		aston(__ci->ci_curproc);				\
} while (/*CONSTCOND*/0)
#else
#define need_resched()							\
do {									\
	struct cpu_info *__ci = curcpu();				\
	__ci->ci_want_resched = 1;					\
	if (__ci->ci_curproc != NULL)					\
		aston(__ci->ci_curproc);				\
} while (/*CONSTCOND*/0)
#endif

#endif

#define aston(p)	((p)->p_md.md_astpending = 1)

extern u_int32_t cpus_attached;

#define curpcb		curcpu()->ci_curpcb
#define curproc		curcpu()->ci_curproc

/*
 * Arguments to hardclock, softclock and statclock
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 */
#define clockframe intrframe

#define	CLKF_USERMODE(frame)	USERMODE((frame)->if_cs, (frame)->if_rflags)
#define CLKF_BASEPRI(frame)	(0)
#define CLKF_PC(frame)		((frame)->if_rip)
#define CLKF_INTR(frame)	(curcpu()->ci_idepth > 1)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, aston(p))

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		aston(p)

/*
 * We need a machine-independent name for this.
 */
extern void (*delay_func)(int);
struct timeval;
extern void (*microtime_func)(struct timeval *);

#define DELAY(x)		(*delay_func)(x)
#define delay(x)		(*delay_func)(x)
#define microtime(tv)		(*microtime_func)(tv)


/*
 * pull in #defines for kinds of processors
 */

#ifdef _KERNEL
extern int biosbasemem;
extern int biosextmem;
extern int cpu;
extern int cpu_feature;
extern int cpu_id;
extern char cpu_vendor[];
extern int cpuid_level;

/* kern_microtime.c */

extern struct timeval cc_microset_time;
void	cc_microtime(struct timeval *);
void	cc_microset(struct cpu_info *);

/* identcpu.c */

void	identifycpu(struct cpu_info *);
void cpu_probe_features(struct cpu_info *);

/* machdep.c */
void	delay(int);
void	dumpconf(void);
int	cpu_maxproc(void);
void	cpu_reset(void);
void	x86_64_proc0_tss_ldt_init(void);
void	x86_64_bufinit(void);
void	x86_64_init_pcb_tss_ldt(struct cpu_info *);
void	cpu_proc_fork(struct proc *, struct proc *);

struct region_descriptor;
void	lgdt(struct region_descriptor *);
void	fillw(short, void *, size_t);

struct pcb;
void	savectx(struct pcb *);
void	switch_exit(struct proc *, void (*)(struct proc *));
void	proc_trampoline(void);
void	child_trampoline(void);

/* clock.c */
void	initrtclock(void);
void	startrtclock(void);
void	i8254_delay(int);
void	i8254_microtime(struct timeval *);
void	i8254_initclocks(void);

void cpu_init_msrs(struct cpu_info *);


/* trap.c */
void	child_return(void *);

/* consinit.c */
void kgdb_port_init(void);

/* bus_machdep.c */
void x86_bus_space_init(void);
void x86_bus_space_mallocok(void);

#endif /* _KERNEL */

#include <machine/psl.h>

/* 
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_BIOSBASEMEM		2	/* int: bios-reported base mem (K) */
#define	CPU_BIOSEXTMEM		3	/* int: bios-reported ext. mem (K) */
#define	CPU_NKPDE		4	/* int: number of kernel PDEs */
#define	CPU_BOOTED_KERNEL	5	/* string: booted kernel name */
#define CPU_DISKINFO		6	/* disk geometry information */
#define CPU_FPU_PRESENT		7	/* FPU is present */
#define	CPU_MAXID		8	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "biosbasemem", CTLTYPE_INT }, \
	{ "biosextmem", CTLTYPE_INT }, \
	{ "nkpde", CTLTYPE_INT }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "diskinfo", CTLTYPE_STRUCT }, \
	{ "fpu_present", CTLTYPE_INT }, \
}


/*
 * Structure for CPU_DISKINFO sysctl call.
 * XXX this should be somewhere else.
 */
#define MAX_BIOSDISKS	16

struct disklist {
	int dl_nbiosdisks;			   /* number of bios disks */
	struct biosdisk_info {
		int bi_dev;			   /* BIOS device # (0x80 ..) */
		int bi_cyl;			   /* cylinders on disk */
		int bi_head;			   /* heads per track */
		int bi_sec;			   /* sectors per track */
		u_int64_t bi_lbasecs;		   /* total sec. (iff ext13) */
#define BIFLAG_INVALID		0x01
#define BIFLAG_EXTINT13		0x02
		int bi_flags;
	} dl_biosdisks[MAX_BIOSDISKS];

	int dl_nnativedisks;			   /* number of native disks */
	struct nativedisk_info {
		char ni_devname[16];		   /* native device name */
		int ni_nmatches; 		   /* # of matches w/ BIOS */
		int ni_biosmatches[MAX_BIOSDISKS]; /* indices in dl_biosdisks */
	} dl_nativedisks[1];			   /* actually longer */
};

#endif /* !_AMD64_CPU_H_ */
