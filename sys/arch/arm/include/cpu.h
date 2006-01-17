/*	$OpenBSD: cpu.h,v 1.12 2006/01/17 20:30:12 miod Exp $	*/
/*	$NetBSD: cpu.h,v 1.34 2003/06/23 11:01:08 martin Exp $	*/

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpu.h
 *
 * CPU specific symbols
 *
 * Created      : 18/09/94
 *
 * Based on kate/katelib/arm6.h
 */

#ifndef _ARM_CPU_H_
#define _ARM_CPU_H_

/*
 * User-visible definitions
 */

/*  CTL_MACHDEP definitions. */
#define	CPU_DEBUG		1	/* int: misc kernel debug control */
#define	CPU_BOOTED_DEVICE	2	/* string: device we booted from */
#define	CPU_BOOTED_KERNEL	3	/* string: kernel we booted */
#define	CPU_CONSDEV		4	/* struct: dev_t of our console */
#define	CPU_POWERSAVE		5	/* int: use CPU powersave mode */
#define	CPU_ALLOWAPERTURE	6	/* int: allow mmap of /dev/xf86 */
#define CPU_APMWARN		7	/* APM battery warning percentage */
#define CPU_KBDRESET		8	/* int: console keyboard reset */
#define CPU_ZTSRAWMODE		9	/* int: zts returns unscaled x/y */
#define CPU_ZTSSCALE		10	/* struct: zts scaling parameters */
#define	CPU_MAXSPEED		11	/* int: number of valid machdep ids */
#define CPU_LIDSUSPEND		12	/* int: closing lid causes suspend */
#define	CPU_MAXID		13	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "debug", CTLTYPE_INT }, \
	{ "booted_device", CTLTYPE_STRING }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "powersave", CTLTYPE_INT }, \
	{ "allowaperture", CTLTYPE_INT }, \
	{ "apmwarn", CTLTYPE_INT }, \
	{ "kbdreset", CTLTYPE_INT }, \
	{ "ztsrawmode", CTLTYPE_INT }, \
	{ "ztsscale", CTLTYPE_STRUCT }, \
	{ "maxspeed", CTLTYPE_INT }, \
	{ "lidsuspend", CTLTYPE_INT } \
}    

#ifdef _KERNEL

/*
 * Kernel-only definitions
 */

#include <arm/cpuconf.h>

#include <machine/intr.h>
#ifndef _LOCORE
#if 0
#include <sys/user.h>
#endif
#include <machine/frame.h>
#include <machine/pcb.h>
#endif	/* !_LOCORE */

#include <arm/armreg.h>

#ifndef _LOCORE
/* 1 == use cpu_sleep(), 0 == don't */
extern int cpu_do_powersave;
#endif

#ifdef __PROG32
#ifdef _LOCORE
#define IRQdisable \
	stmfd	sp!, {r0} ; \
	mrs	r0, cpsr ; \
	orr	r0, r0, #(I32_bit) ; \
	msr	cpsr_c, r0 ; \
	ldmfd	sp!, {r0}

#define IRQenable \
	stmfd	sp!, {r0} ; \
	mrs	r0, cpsr ; \
	bic	r0, r0, #(I32_bit) ; \
	msr	cpsr_c, r0 ; \
	ldmfd	sp!, {r0}		

#else
#define IRQdisable __set_cpsr_c(I32_bit, I32_bit);
#define IRQenable __set_cpsr_c(I32_bit, 0);
#endif	/* _LOCORE */
#endif

#ifndef _LOCORE

/* All the CLKF_* macros take a struct clockframe * as an argument. */

/*
 * CLKF_USERMODE: Return TRUE/FALSE (1/0) depending on whether the
 * frame came from USR mode or not.
 */
#ifdef __PROG32
#define CLKF_USERMODE(frame)	((frame->if_spsr & PSR_MODE) == PSR_USR32_MODE)
#else
#define CLKF_USERMODE(frame)	((frame->if_r15 & R15_MODE) == R15_MODE_USR)
#endif

/*
 * CLKF_INTR: True if we took the interrupt from inside another
 * interrupt handler.
 */
extern int current_intr_depth;
#ifdef __PROG32
/* Hack to treat FPE time as interrupt time so we can measure it */
#define CLKF_INTR(frame)						\
	((current_intr_depth > 1) ||					\
	    (frame->if_spsr & PSR_MODE) == PSR_UND32_MODE)
#else
#define CLKF_INTR(frame)	(current_intr_depth > 1) 
#endif

/*
 * CLKF_PC: Extract the program counter from a clockframe
 */
#ifdef __PROG32
#define CLKF_PC(frame)		(frame->if_pc)
#else
#define CLKF_PC(frame)		(frame->if_r15 & R15_PC)
#endif

/*
 * PROC_PC: Find out the program counter for the given process.
 */
#ifdef __PROG32
#define PROC_PC(p)	((p)->p_addr->u_pcb.pcb_tf->tf_pc)
#else
#define PROC_PC(p)	((p)->p_addr->u_pcb.pcb_tf->tf_r15 & R15_PC)
#endif

/* The address of the vector page. */
extern vaddr_t vector_page;
#ifdef __PROG32
void	arm32_vector_init(vaddr_t, int);

#define	ARM_VEC_RESET			(1 << 0)
#define	ARM_VEC_UNDEFINED		(1 << 1)
#define	ARM_VEC_SWI			(1 << 2)
#define	ARM_VEC_PREFETCH_ABORT		(1 << 3)
#define	ARM_VEC_DATA_ABORT		(1 << 4)
#define	ARM_VEC_ADDRESS_EXCEPTION	(1 << 5)
#define	ARM_VEC_IRQ			(1 << 6)
#define	ARM_VEC_FIQ			(1 << 7)

#define	ARM_NVEC			8
#define	ARM_VEC_ALL			0xffffffff
#endif

/*
 * Per-CPU information.  For now we assume one CPU.
 */

#include <sys/device.h>
/*
#include <sys/sched.h>
*/
struct cpu_info {
#if 0 
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#endif
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif
	struct device *ci_dev;		/* Device corresponding to this CPU */
	u_int32_t ci_arm_cpuid;		/* aggregate CPU id */
	u_int32_t ci_arm_cputype;	/* CPU type */
	u_int32_t ci_arm_cpurev;	/* CPU revision */
	u_int32_t ci_ctrl;		/* The CPU control register */
	struct evcnt ci_arm700bugcount;
#ifdef MULTIPROCESSOR
	MP_CPU_INFO_MEMBERS
#endif
};

#ifndef MULTIPROCESSOR
extern struct cpu_info cpu_info_store;
#define	curcpu()	(&cpu_info_store)
#define cpu_number()	0
#endif

#ifdef __PROG32
void	cpu_proc_fork(struct proc *, struct proc *);
#else
#define	cpu_proc_fork(p1, p2)
#endif

/*
 * Scheduling glue
 */

extern int astpending;
#define setsoftast() (astpending = 1)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#define signotify(p)            setsoftast()

#define cpu_wait(p)    /* nothing */

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int want_resched;	/* resched() was called */
#define	need_resched(ci)	(want_resched = 1, setsoftast())

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

#ifndef acorn26
/*
 * cpu device glue (belongs in cpuvar.h)
 */

struct device;
void	cpu_attach	(struct device *);
int	cpu_alloc_idlepcb	(struct cpu_info *);
#endif


/*
 * Random cruft
 */

/* locore.S */
void atomic_set_bit	(u_int *address, u_int setmask);
void atomic_clear_bit	(u_int *address, u_int clearmask);

/* cpuswitch.S */
struct pcb;
void	savectx		(struct pcb *pcb);

/* ast.c */
void userret (register struct proc *p, u_int32_t pc, quad_t ticks);

/* machdep.h */
void bootsync		(int);

/* fault.c */
int badaddr_read	(void *, size_t, void *);

/* syscall.c */
void swi_handler	(trapframe_t *);

/* machine_machdep.c */
void board_startup(void);

#endif	/* !_LOCORE */

#endif /* _KERNEL */

#endif /* !_ARM_CPU_H_ */

/* End of cpu.h */
