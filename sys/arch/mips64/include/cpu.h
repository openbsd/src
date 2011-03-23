/*	$OpenBSD: cpu.h,v 1.70 2011/03/23 16:54:36 pirofti Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 *	from: @(#)cpu.h	8.4 (Berkeley) 1/4/94
 */

#ifndef _MIPS64_CPU_H_
#define	_MIPS64_CPU_H_

#ifndef _LOCORE

/*
 * MIPS32-style segment definitions.
 * They only cover the first 512MB of physical addresses.
 */
#define	CKSEG0_BASE		0xffffffff80000000UL
#define	CKSEG1_BASE		0xffffffffa0000000UL
#define	CKSSEG_BASE		0xffffffffc0000000UL
#define	CKSEG3_BASE		0xffffffffe0000000UL
#define	CKSEG_SIZE		0x0000000020000000UL

#define	CKSEG0_TO_PHYS(x)	((u_long)(x) & (CKSEG_SIZE - 1))
#define	CKSEG1_TO_PHYS(x)	((u_long)(x) & (CKSEG_SIZE - 1))
#define	PHYS_TO_CKSEG0(x)	((u_long)(x) | CKSEG0_BASE)
#define	PHYS_TO_CKSEG1(x)	((u_long)(x) | CKSEG1_BASE)

/*
 * MIPS64-style segment definitions.
 * These allow for 36 bits of addressable physical memory, thus 64GB.
 */

/*
 * Cache Coherency Attributes.
 */
/* r8k only */
#define	CCA_NC_COPROCESSOR	0UL	/* uncached, coprocessor ordered */
/* common to r4, r5k, r8k and r1xk */
#define	CCA_NC			2UL	/* uncached, write-around */
#define	CCA_NONCOHERENT		3UL	/* cached, non-coherent, write-back */
/* r8k, r1xk only */
#define	CCA_COHERENT_EXCL	4UL	/* cached, coherent, exclusive */
#define	CCA_COHERENT_EXCLWRITE	5UL	/* cached, coherent, exclusive write */
/* r1xk only */
#define	CCA_NC_ACCELERATED	7UL	/* uncached accelerated */
/* r4k only */
#define	CCA_COHERENT_UPDWRITE	6UL	/* cached, coherent, update on write */

#ifdef TGT_COHERENT
#define	CCA_CACHED		CCA_COHERENT_EXCLWRITE
#else
#define	CCA_CACHED		CCA_NONCOHERENT
#endif

/*
 * Uncached spaces.
 * R1x000 processors use bits 58:57 of uncached virtual addresses (CCA_NC)
 * to select different spaces. Unfortunately, other processors need these
 * bits to be zero, so uncached address have to be decided at runtime.
 */
#define	SP_HUB			0UL	/* Hub space */
#define	SP_IO			1UL	/* I/O space */
#define	SP_SPECIAL		2UL	/* Memory Special space */
#define	SP_NC			3UL	/* Memory Uncached space */

extern vaddr_t uncached_base;

#define	XKSSSEG_BASE		0x4000000000000000UL
#define	XKPHYS_BASE		0x8000000000000000UL
#define	XKSSEG_BASE		0xc000000000000000UL

#define	XKPHYS_TO_PHYS(x)	((paddr_t)(x) & 0x0000000fffffffffUL)
#define	PHYS_TO_XKPHYS(x,c)	((paddr_t)(x) | XKPHYS_BASE | ((c) << 59))
#define	PHYS_TO_XKPHYS_UNCACHED(x,s) \
	(PHYS_TO_XKPHYS(x, CCA_NC) | ((s) << 57))
#define	PHYS_TO_UNCACHED(x)	((paddr_t)(x) | uncached_base)
#define	IS_XKPHYS(va)		(((va) >> 62) == 2)
#define	XKPHYS_TO_CCA(x)	(((x) >> 59) & 0x07)
#define	XKPHYS_TO_SP(x)		(((x) >> 57) & 0x03)

#endif	/* _LOCORE */

#if defined(_KERNEL) || defined(_STANDALONE)

/*
 * Status register.
 */
#define	SR_XX			0x80000000
#define	SR_COP_USABILITY	0x30000000	/* CP0 and CP1 only */
#define	SR_COP_0_BIT		0x10000000
#define	SR_COP_1_BIT		0x20000000
#define	SR_RP			0x08000000
#define	SR_FR_32		0x04000000
#define	SR_RE			0x02000000
#define	SR_DSD			0x01000000	/* Only on R12000 */
#define	SR_BOOT_EXC_VEC		0x00400000
#define	SR_TLB_SHUTDOWN		0x00200000
#define	SR_SOFT_RESET		0x00100000
#define	SR_DIAG_CH		0x00040000
#define	SR_DIAG_CE		0x00020000
#define	SR_DIAG_DE		0x00010000
#define	SR_KX			0x00000080
#define	SR_SX			0x00000040
#define	SR_UX			0x00000020
#define	SR_KSU_MASK		0x00000018
#define	SR_KSU_USER		0x00000010
#define	SR_KSU_SUPER		0x00000008
#define	SR_KSU_KERNEL		0x00000000
#define	SR_ERL			0x00000004
#define	SR_EXL			0x00000002
#define	SR_INT_ENAB		0x00000001

#define	SR_INT_MASK		0x0000ff00
#define	SOFT_INT_MASK_0		0x00000100
#define	SOFT_INT_MASK_1		0x00000200
#define	SR_INT_MASK_0		0x00000400
#define	SR_INT_MASK_1		0x00000800
#define	SR_INT_MASK_2		0x00001000
#define	SR_INT_MASK_3		0x00002000
#define	SR_INT_MASK_4		0x00004000
#define	SR_INT_MASK_5		0x00008000
/*
 * Interrupt control register in RM7000. Expansion of interrupts.
 */
#define	IC_INT_MASK		0x00003f00	/* Two msb reserved */
#define	IC_INT_MASK_6		0x00000100
#define	IC_INT_MASK_7		0x00000200
#define	IC_INT_MASK_8		0x00000400
#define	IC_INT_MASK_9		0x00000800
#define	IC_INT_TIMR		0x00001000	/* 12 Timer */
#define	IC_INT_PERF		0x00002000	/* 13 Performance counter */
#define	IC_INT_TE		0x00000080	/* Timer on INT11 */

#define	ALL_INT_MASK		((IC_INT_MASK << 8) | SR_INT_MASK)
#define	SOFT_INT_MASK		(SOFT_INT_MASK_0 | SOFT_INT_MASK_1)
#define	HW_INT_MASK		(ALL_INT_MASK & ~SOFT_INT_MASK)


/*
 * The bits in the cause register.
 *
 *	CR_BR_DELAY	Exception happened in branch delay slot.
 *	CR_COP_ERR	Coprocessor error.
 *	CR_IP		Interrupt pending bits defined below.
 *	CR_EXC_CODE	The exception type (see exception codes below).
 */
#define	CR_BR_DELAY		0x80000000
#define	CR_COP_ERR		0x30000000
#define	CR_EXC_CODE		0x0000007c
#define	CR_EXC_CODE_SHIFT	2
#define	CR_IPEND		0x003fff00
#define	CR_INT_SOFT0		0x00000100
#define	CR_INT_SOFT1		0x00000200
#define	CR_INT_0		0x00000400
#define	CR_INT_1		0x00000800
#define	CR_INT_2		0x00001000
#define	CR_INT_3		0x00002000
#define	CR_INT_4		0x00004000
#define	CR_INT_5		0x00008000
/* Following on RM7000 */
#define	CR_INT_6		0x00010000
#define	CR_INT_7		0x00020000
#define	CR_INT_8		0x00040000
#define	CR_INT_9		0x00080000
#define	CR_INT_HARD		0x000ffc00
#define	CR_INT_TIMR		0x00100000	/* 12 Timer */
#define	CR_INT_PERF		0x00200000	/* 13 Performance counter */

/*
 * The bits in the context register.
 */
#define	CNTXT_PTE_BASE		0xff800000
#define	CNTXT_BAD_VPN2		0x007ffff0

/*
 * Location of exception vectors.
 */
#define	RESET_EXC_VEC		(CKSEG1_BASE + 0x1fc00000)
#define	TLB_MISS_EXC_VEC	(CKSEG0_BASE + 0x00000000)
#define	XTLB_MISS_EXC_VEC	(CKSEG0_BASE + 0x00000080)
#define	CACHE_ERR_EXC_VEC	(CKSEG0_BASE + 0x00000100)
#define	GEN_EXC_VEC		(CKSEG0_BASE + 0x00000180)

/*
 * Coprocessor 0 registers:
 */
#define	COP_0_TLB_INDEX		$0
#define	COP_0_TLB_RANDOM	$1
#define	COP_0_TLB_LO0		$2
#define	COP_0_TLB_LO1		$3
#define	COP_0_TLB_CONTEXT	$4
#define	COP_0_TLB_PG_MASK	$5
#define	COP_0_TLB_WIRED		$6
#define	COP_0_BAD_VADDR		$8
#define	COP_0_COUNT		$9
#define	COP_0_TLB_HI		$10
#define	COP_0_COMPARE		$11
#define	COP_0_STATUS_REG	$12
#define	COP_0_CAUSE_REG		$13
#define	COP_0_EXC_PC		$14
#define	COP_0_PRID		$15
#define	COP_0_CONFIG		$16
#define	COP_0_LLADDR		$17
#define	COP_0_WATCH_LO		$18
#define	COP_0_WATCH_HI		$19
#define	COP_0_TLB_XCONTEXT	$20
#define	COP_0_TLB_FR_MASK	$21	/* R10000 onwards */
#define	COP_0_DIAG		$22	/* Loongson 2F */
#define	COP_0_ECC		$26
#define	COP_0_CACHE_ERR		$27
#define	COP_0_TAG_LO		$28
#define	COP_0_TAG_HI		$29
#define	COP_0_ERROR_PC		$30

/*
 * RM7000 specific
 */
#define	COP_0_WATCH_1		$18
#define	COP_0_WATCH_2		$19
#define	COP_0_WATCH_M		$24
#define	COP_0_PC_COUNT		$25
#define	COP_0_PC_CTRL		$22

#define	COP_0_ICR		$20	/* Use cfc0/ctc0 to access */

/*
 * Octeon specific
 */
#define COP_0_TLB_PG_GRAIN	$5, 1
#define COP_0_CVMCTL		$9, 7
#define COP_0_CVMMEMCTL		$11, 7
#define COP_0_EBASE		$15, 1

/*
 * Values for the code field in a break instruction.
 */
#define	BREAK_INSTR		0x0000000d
#define	BREAK_VAL_MASK		0x03ff0000
#define	BREAK_VAL_SHIFT		16
#define	BREAK_KDB_VAL		512
#define	BREAK_SSTEP_VAL		513
#define	BREAK_BRKPT_VAL		514
#define	BREAK_SOVER_VAL		515
#define	BREAK_DDB_VAL		516
#define	BREAK_FPUEMUL_VAL	517
#define	BREAK_KDB	(BREAK_INSTR | (BREAK_KDB_VAL << BREAK_VAL_SHIFT))
#define	BREAK_SSTEP	(BREAK_INSTR | (BREAK_SSTEP_VAL << BREAK_VAL_SHIFT))
#define	BREAK_BRKPT	(BREAK_INSTR | (BREAK_BRKPT_VAL << BREAK_VAL_SHIFT))
#define	BREAK_SOVER	(BREAK_INSTR | (BREAK_SOVER_VAL << BREAK_VAL_SHIFT))
#define	BREAK_DDB	(BREAK_INSTR | (BREAK_DDB_VAL << BREAK_VAL_SHIFT))
#define	BREAK_FPUEMUL	(BREAK_INSTR | (BREAK_FPUEMUL_VAL << BREAK_VAL_SHIFT))

/*
 * The floating point version and status registers.
 */
#define	FPC_ID			$0
#define	FPC_CSR			$31

/*
 * The low part of the TLB entry.
 */
#define	VMTLB_PF_NUM		0x3fffffc0
#define	VMTLB_ATTR_MASK		0x00000038
#define	VMTLB_MOD_BIT		0x00000004
#define	VMTLB_VALID_BIT		0x00000002
#define	VMTLB_GLOBAL_BIT	0x00000001

#define	VMTLB_PHYS_PAGE_SHIFT	6

/*
 * The high part of the TLB entry.
 */
#define	VMTLB_VIRT_PAGE_NUM	0xffffe000
#define	VMTLB_PID		0x000000ff
#define	VMTLB_PID_SHIFT		0
#define	VMTLB_VIRT_PAGE_SHIFT	12

/*
 * The number of process id entries.
 */
#define	VMNUM_PIDS		256

/*
 * TLB probe return codes.
 */
#define	VMTLB_NOT_FOUND		0
#define	VMTLB_FOUND		1
#define	VMTLB_FOUND_WITH_PATCH	2
#define	VMTLB_PROBE_ERROR	3

#endif	/* _KERNEL || _STANDALONE */

/*
 * Exported definitions unique to mips cpu support.
 */

#if defined(_KERNEL) && !defined(_LOCORE)

#include <sys/device.h>
#include <sys/lock.h>
#include <machine/intr.h>
#include <sys/sched.h>

struct cpu_hwinfo {
	uint32_t	c0prid;
	uint32_t	c1prid;
	uint32_t	clock;	/* Hz */
	uint32_t	tlbsize;
	uint		type;
	uint32_t	l2size;
};

struct cpu_info {
	struct device	*ci_dev;	/* our device */
	struct cpu_info	*ci_self;	/* pointer to this structure */
	struct cpu_info	*ci_next;	/* next cpu */
	struct proc	*ci_curproc;
	struct user	*ci_curprocpaddr;
	struct proc	*ci_fpuproc;	/* pointer to last proc to use FP */
	uint32_t	 ci_delayconst;
	struct cpu_hwinfo
			ci_hw;

	/* cache information */
	uint		ci_cacheconfiguration;
	uint		ci_cacheways;
	uint		ci_l1instcachesize;
	uint		ci_l1instcacheline;
	uint		ci_l1instcacheset;
	uint		ci_l1datacachesize;
	uint		ci_l1datacacheline;
	uint		ci_l1datacacheset;
	uint		ci_l2size;
	uint		ci_l3size;

	struct schedstate_percpu
			ci_schedstate;
	int		ci_want_resched;	/* need_resched() invoked */
	cpuid_t		ci_cpuid;		/* our CPU ID */
	uint32_t	ci_randseed;		/* per cpu random seed */
	int		ci_ipl;			/* software IPL */
	uint32_t	ci_softpending;		/* pending soft interrupts */
	int		ci_clock_started;
	u_int32_t	ci_cpu_counter_last;	/* last compare value loaded */
	u_int32_t	ci_cpu_counter_interval; /* # of counter ticks/tick */

	u_int32_t	ci_pendingticks;
	struct pmap	*ci_curpmap;
	uint		ci_intrdepth;		/* interrupt depth */
#ifdef MULTIPROCESSOR
	u_long		ci_flags;		/* flags; see below */
	struct intrhand	ci_ipiih;
#endif
	volatile int    ci_ddb;
#define	CI_DDB_RUNNING		0
#define	CI_DDB_SHOULDSTOP	1
#define	CI_DDB_STOPPED		2
#define	CI_DDB_ENTERDDB		3
#define	CI_DDB_INDDB		4

#ifdef DIAGNOSTIC
	int	ci_mutex_level;
#endif
};

#define	CPUF_PRIMARY	0x01		/* CPU is primary CPU */
#define	CPUF_PRESENT	0x02		/* CPU is present */
#define	CPUF_RUNNING	0x04		/* CPU is running */

extern struct cpu_info cpu_info_primary;
extern struct cpu_info *cpu_info_list;
#define CPU_INFO_ITERATOR		int
#define	CPU_INFO_FOREACH(cii, ci)	for (cii = 0, ci = cpu_info_list; \
					    ci != NULL; ci = ci->ci_next)

#define CPU_INFO_UNIT(ci)               ((ci)->ci_dev ? (ci)->ci_dev->dv_unit : 0)

#ifdef MULTIPROCESSOR
#define MAXCPUS				4
#define getcurcpu()			hw_getcurcpu()
#define setcurcpu(ci)			hw_setcurcpu(ci)
extern struct cpu_info *get_cpu_info(int);
#define curcpu() getcurcpu()
#define	CPU_IS_PRIMARY(ci)		((ci)->ci_flags & CPUF_PRIMARY)
#define cpu_number()			(curcpu()->ci_cpuid)

extern struct cpuset cpus_running;
void cpu_unidle(struct cpu_info *);
void cpu_boot_secondary_processors(void);
#define cpu_boot_secondary(ci)          hw_cpu_boot_secondary(ci)
#define cpu_hatch(ci)                   hw_cpu_hatch(ci)

vaddr_t alloc_contiguous_pages(size_t);

#define MIPS64_IPI_NOP		0x00000001
#define MIPS64_IPI_RENDEZVOUS	0x00000002
#define MIPS64_IPI_DDB		0x00000004
#define MIPS64_NIPIS		3	/* must not exceed 32 */

void	mips64_ipi_init(void);
void	mips64_send_ipi(unsigned int, unsigned int);
void	smp_rendezvous_cpus(unsigned long, void (*)(void *), void *arg);

#include <sys/mplock.h>
#else
#define MAXCPUS				1
#define curcpu()			(&cpu_info_primary)
#define	CPU_IS_PRIMARY(ci)		1
#define cpu_number()			0
#define cpu_unidle(ci)
#define get_cpu_info(i)			(&cpu_info_primary)
#endif

void cpu_startclock(struct cpu_info *);

#include <machine/frame.h>

/*
 * Arguments to hardclock encapsulate the previous machine state in
 * an opaque clockframe.
 */
#define	clockframe trap_frame	/* Use normal trap frame */

#define	CLKF_USERMODE(framep)	((framep)->sr & SR_KSU_USER)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(curcpu()->ci_intrdepth > 1)	/* XXX */

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)	((p)->p_md.md_regs->pc)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched(ci) \
	do { \
		(ci)->ci_want_resched = 1; \
		if ((ci)->ci_curproc != NULL) \
			aston((ci)->ci_curproc); \
	} while(0)
#define	clear_resched(ci) 	(ci)->ci_want_resched = 0

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the PICA, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	aston(p)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#ifdef MULTIPROCESSOR
#define	signotify(p)		(aston(p), cpu_unidle(p->p_cpu))
#else
#define	signotify(p)		aston(p)
#endif

#define	aston(p)		p->p_md.md_astpending = 1

#endif /* _KERNEL && !_LOCORE */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_ALLOWAPERTURE	1	/* allow mmap of /dev/xf86 */
#define	CPU_KBDRESET		2	/* keyboard reset */
#define	CPU_MAXID		3	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES {			\
	{ 0, 0 },				\
	{ "allowaperture", CTLTYPE_INT },	\
	{ "kbdreset", CTLTYPE_INT },		\
}

/*
 * MIPS CPU types (cp_imp).
 */
#define	MIPS_R2000	0x01	/* MIPS R2000 CPU		ISA I   */
#define	MIPS_R3000	0x02	/* MIPS R3000 CPU		ISA I   */
#define	MIPS_R6000	0x03	/* MIPS R6000 CPU		ISA II	*/
#define	MIPS_R4000	0x04	/* MIPS R4000/4400 CPU		ISA III	*/
#define	MIPS_R3LSI	0x05	/* LSI Logic R3000 derivate	ISA I	*/
#define	MIPS_R6000A	0x06	/* MIPS R6000A CPU		ISA II	*/
#define MIPS_OCTEON	0x06	/* Cavium OCTEON		MIPS64R2*/
#define	MIPS_R3IDT	0x07	/* IDT R3000 derivate		ISA I	*/
#define	MIPS_R10000	0x09	/* MIPS R10000/T5 CPU		ISA IV  */
#define	MIPS_R4200	0x0a	/* MIPS R4200 CPU (ICE)		ISA III */
#define	MIPS_R4300	0x0b	/* NEC VR4300 CPU		ISA III */
#define	MIPS_R4100	0x0c	/* NEC VR41xx CPU MIPS-16	ISA III */
#define	MIPS_R12000	0x0e	/* MIPS R12000			ISA IV  */
#define	MIPS_R14000	0x0f	/* MIPS R14000			ISA IV  */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	MIPS_R4600	0x20	/* PMCS R4600 Orion		ISA III */
#define	MIPS_R4700	0x21	/* PMCS R4700 Orion		ISA III */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based CPU	ISA I	*/
#define	MIPS_R5000	0x23	/* MIPS R5000 CPU		ISA IV  */
#define	MIPS_RM7000	0x27	/* PMCS RM7000 CPU		ISA IV  */
#define	MIPS_RM52X0	0x28	/* PMCS RM52X0 CPU		ISA IV  */
#define	MIPS_RM9000	0x34	/* PMCS RM9000 CPU		ISA IV  */
#define	MIPS_LOONGSON	0x42	/* STC LoongSon CPU		ISA III */
#define	MIPS_VR5400	0x54	/* NEC Vr5400 CPU		ISA IV+ */
#define	MIPS_LOONGSON2	0x63	/* STC LoongSon2 CPU		ISA III */

/*
 * MIPS FPU types. Only soft, rest is the same as cpu type.
 */
#define	MIPS_SOFT	0x00	/* Software emulation		ISA I   */


#if defined(_KERNEL) && !defined(_LOCORE)

extern vaddr_t CpuCacheAliasMask;

struct exec_package;
struct tlb_entry;
struct user;

u_int	cp0_get_count(void);
uint32_t cp0_get_config(void);
uint32_t cp0_get_prid(void);
void	cp0_set_compare(u_int);
u_int	cp1_get_prid(void);
void	tlb_set_page_mask(uint32_t);
void	tlb_set_pid(int);
void	tlb_set_wired(int);

/*
 * Available cache operation routines. See <machine/cpu.h> for more.
 */
int	Octeon_ConfigCache(struct cpu_info *);
void	Octeon_SyncCache(struct cpu_info *);
void	Octeon_InvalidateICache(struct cpu_info *, vaddr_t, size_t);
void	Octeon_SyncDCachePage(struct cpu_info *, paddr_t);
void	Octeon_HitSyncDCache(struct cpu_info *, paddr_t, size_t);
void	Octeon_HitInvalidateDCache(struct cpu_info *, paddr_t, size_t);
void	Octeon_IOSyncDCache(struct cpu_info *, paddr_t, size_t, int);

int	Loongson2_ConfigCache(struct cpu_info *);
void	Loongson2_SyncCache(struct cpu_info *);
void	Loongson2_InvalidateICache(struct cpu_info *, vaddr_t, size_t);
void	Loongson2_SyncDCachePage(struct cpu_info *, paddr_t);
void	Loongson2_HitSyncDCache(struct cpu_info *, paddr_t, size_t);
void	Loongson2_HitInvalidateDCache(struct cpu_info *, paddr_t, size_t);
void	Loongson2_IOSyncDCache(struct cpu_info *, paddr_t, size_t, int);

int	Mips5k_ConfigCache(struct cpu_info *);
void	Mips5k_SyncCache(struct cpu_info *);
void	Mips5k_InvalidateICache(struct cpu_info *, vaddr_t, size_t);
void	Mips5k_SyncDCachePage(struct cpu_info *, vaddr_t);
void	Mips5k_HitSyncDCache(struct cpu_info *, vaddr_t, size_t);
void	Mips5k_HitInvalidateDCache(struct cpu_info *, vaddr_t, size_t);
void	Mips5k_IOSyncDCache(struct cpu_info *, vaddr_t, size_t, int);

int	Mips10k_ConfigCache(struct cpu_info *);
void	Mips10k_SyncCache(struct cpu_info *);
void	Mips10k_InvalidateICache(struct cpu_info *, vaddr_t, size_t);
void	Mips10k_SyncDCachePage(struct cpu_info *, vaddr_t);
void	Mips10k_HitSyncDCache(struct cpu_info *, vaddr_t, size_t);
void	Mips10k_HitInvalidateDCache(struct cpu_info *, vaddr_t, size_t);
void	Mips10k_IOSyncDCache(struct cpu_info *, vaddr_t, size_t, int);

void	tlb_flush(int);
void	tlb_flush_addr(vaddr_t);
void	tlb_write_indexed(int, struct tlb_entry *);
int	tlb_update(vaddr_t, unsigned);
void	tlb_read(int, struct tlb_entry *);

void	build_trampoline(vaddr_t, vaddr_t);
int	exec_md_map(struct proc *, struct exec_package *);
void	savectx(struct user *, int);

void	enable_fpu(struct proc *);
void	save_fpu(void);
int	fpe_branch_emulate(struct proc *, struct trap_frame *, uint32_t,
	    vaddr_t);

int	guarded_read_4(paddr_t, uint32_t *);
int	guarded_write_4(paddr_t, uint32_t);

void	MipsFPTrap(struct trap_frame *);
register_t MipsEmulateBranch(struct trap_frame *, vaddr_t, uint32_t, uint32_t);

/*
 *  Low level access routines to CPU registers
 */

void	setsoftintr0(void);
void	clearsoftintr0(void);
void	setsoftintr1(void);
void	clearsoftintr1(void);
uint32_t enableintr(void);
uint32_t disableintr(void);
uint32_t getsr(void);
uint32_t setsr(uint32_t);

#endif /* _KERNEL && !_LOCORE */
#endif /* !_MIPS64_CPU_H_ */
