/*	$OpenBSD: cpu.h,v 1.9 2004/10/20 12:49:15 pefo Exp $	*/

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

#ifndef _MIPS_CPU_H_
#define _MIPS_CPU_H_

#include <machine/psl.h>

#ifdef __LP64__
#define KSEG0_BASE	0xffffffff80000000
#define KSEG1_BASE	0xffffffffa0000000
#define KSSEG_BASE	0xffffffffc0000000
#define KSEG3_BASE	0xffffffffe0000000
#else
#define KSEG0_BASE	0x80000000
#define KSEG1_BASE	0xa0000000
#define KSSEG_BASE	0xc0000000
#define KSEG3_BASE	0xe0000000
#endif
#define KSEG_SIZE	0x20000000

#define	KSEG0_TO_PHYS(x)	((u_long)(x) & 0x1fffffff)
#define	KSEG1_TO_PHYS(x)	((u_long)(x) & 0x1fffffff)
#define	PHYS_TO_KSEG0(x)	((u_long)(x) | KSEG0_BASE)
#define	PHYS_TO_KSEG1(x)	((u_long)(x) | KSEG1_BASE)
#define	PHYS_TO_KSEG3(x)	((u_long)(x) | KSEG3_BASE)

#ifdef _KERNEL

/*
 *  Status register.
 */
#define SR_XX			0x80000000
#define SR_COP_USABILITY	0x30000000	/* CP0 and CP1 only */
#define SR_COP_0_BIT		0x10000000
#define SR_COP_1_BIT		0x20000000
#define SR_RP			0x08000000
#define SR_FR_32		0x04000000
#define SR_RE			0x02000000
#define SR_DSD			0x01000000	/* Only on R12000 */
#define SR_BOOT_EXC_VEC		0x00400000
#define SR_TLB_SHUTDOWN		0x00200000
#define SR_SOFT_RESET		0x00100000
#define SR_DIAG_CH		0x00040000
#define SR_DIAG_CE		0x00020000
#define SR_DIAG_DE		0x00010000
#define SR_KX			0x00000080
#define SR_SX			0x00000040
#define SR_UX			0x00000020
#define SR_KSU_MASK		0x00000018
#define SR_KSU_USER		0x00000010
#define SR_KSU_SUPER		0x00000008
#define SR_KSU_KERNEL		0x00000000
#define SR_ERL			0x00000004
#define SR_EXL			0x00000002
#define SR_INT_ENAB		0x00000001

#define SR_INT_MASK		0x0000ff00
#define SOFT_INT_MASK_0		0x00000100
#define SOFT_INT_MASK_1		0x00000200
#define SR_INT_MASK_0		0x00000400
#define SR_INT_MASK_1		0x00000800
#define SR_INT_MASK_2		0x00001000
#define SR_INT_MASK_3		0x00002000
#define SR_INT_MASK_4		0x00004000
#define SR_INT_MASK_5		0x00008000
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
 *	CR_COP_ERR		Coprocessor error.
 *	CR_IP		Interrupt pending bits defined below.
 *	CR_EXC_CODE	The exception type (see exception codes below).
 */
#define CR_BR_DELAY		0x80000000
#define CR_COP_ERR		0x30000000
#define CR_EXC_CODE		0x0000007c
#define CR_EXC_CODE_SHIFT	2
#define CR_IPEND		0x003fff00
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
#define CNTXT_PTE_BASE		0xff800000
#define CNTXT_BAD_VPN2		0x007ffff0

/*
 * Location of exception vectors.
 */
#define RESET_EXC_VEC		(KSEG0_BASE + 0x3fc00000)
#define TLB_MISS_EXC_VEC	(KSEG0_BASE + 0x00000000)
#define XTLB_MISS_EXC_VEC	(KSEG0_BASE + 0x00000080)
#define CACHE_ERR_EXC_VEC	(KSEG0_BASE + 0x00000100)
#define GEN_EXC_VEC		(KSEG0_BASE + 0x00000180)

/*
 * Coprocessor 0 registers:
 */
#define COP_0_TLB_INDEX		$0
#define COP_0_TLB_RANDOM	$1
#define COP_0_TLB_LO0		$2
#define COP_0_TLB_LO1		$3
#define COP_0_TLB_CONTEXT	$4
#define COP_0_TLB_PG_MASK	$5
#define COP_0_TLB_WIRED		$6
#define COP_0_BAD_VADDR		$8
#define COP_0_COUNT		$9
#define COP_0_TLB_HI		$10
#define COP_0_COMPARE		$11
#define COP_0_STATUS_REG	$12
#define COP_0_CAUSE_REG		$13
#define COP_0_EXC_PC		$14
#define COP_0_PRID		$15
#define COP_0_CONFIG		$16
#define COP_0_LLADDR		$17
#define COP_0_WATCH_LO		$18
#define COP_0_WATCH_HI		$19
#define COP_0_TLB_XCONTEXT	$20
#define COP_0_ECC		$26
#define COP_0_CACHE_ERR		$27
#define COP_0_TAG_LO		$28
#define COP_0_TAG_HI		$29
#define COP_0_ERROR_PC		$30

/*
 * RM7000 specific
 */
#define COP_0_WATCH_1		$18
#define COP_0_WATCH_2		$19
#define COP_0_WATCH_M		$24
#define COP_0_PC_COUNT		$25
#define COP_0_PC_CTRL		$22

#define	COP_0_ICR		$20	/* Use cfc0/ctc0 to access */

/*
 * Values for the code field in a break instruction.
 */
#define BREAK_INSTR		0x0000000d
#define BREAK_VAL_MASK		0x03ff0000
#define BREAK_VAL_SHIFT		16
#define BREAK_KDB_VAL		512
#define BREAK_SSTEP_VAL		513
#define BREAK_BRKPT_VAL		514
#define BREAK_SOVER_VAL		515
#define BREAK_DDB_VAL		516
#define BREAK_KDB	(BREAK_INSTR | (BREAK_KDB_VAL << BREAK_VAL_SHIFT))
#define BREAK_SSTEP	(BREAK_INSTR | (BREAK_SSTEP_VAL << BREAK_VAL_SHIFT))
#define BREAK_BRKPT	(BREAK_INSTR | (BREAK_BRKPT_VAL << BREAK_VAL_SHIFT))
#define BREAK_SOVER	(BREAK_INSTR | (BREAK_SOVER_VAL << BREAK_VAL_SHIFT))
#define BREAK_DDB	(BREAK_INSTR | (BREAK_DDB_VAL << BREAK_VAL_SHIFT))

/*
 * Mininum and maximum cache sizes.
 */
#define MIN_CACHE_SIZE		(16 * 1024)
#define MAX_CACHE_SIZE		(256 * 1024)

/*
 * The floating point version and status registers.
 */
#define	FPC_ID			$0
#define	FPC_CSR			$31

/*
 * The floating point coprocessor status register bits.
 */
#define FPC_ROUNDING_BITS		0x00000003
#define FPC_ROUND_RN			0x00000000
#define FPC_ROUND_RZ			0x00000001
#define FPC_ROUND_RP			0x00000002
#define FPC_ROUND_RM			0x00000003
#define FPC_STICKY_BITS			0x0000007c
#define FPC_STICKY_INEXACT		0x00000004
#define FPC_STICKY_UNDERFLOW		0x00000008
#define FPC_STICKY_OVERFLOW		0x00000010
#define FPC_STICKY_DIV0			0x00000020
#define FPC_STICKY_INVALID		0x00000040
#define FPC_ENABLE_BITS			0x00000f80
#define FPC_ENABLE_INEXACT		0x00000080
#define FPC_ENABLE_UNDERFLOW		0x00000100
#define FPC_ENABLE_OVERFLOW		0x00000200
#define FPC_ENABLE_DIV0			0x00000400
#define FPC_ENABLE_INVALID		0x00000800
#define FPC_EXCEPTION_BITS		0x0003f000
#define FPC_EXCEPTION_INEXACT		0x00001000
#define FPC_EXCEPTION_UNDERFLOW		0x00002000
#define FPC_EXCEPTION_OVERFLOW		0x00004000
#define FPC_EXCEPTION_DIV0		0x00008000
#define FPC_EXCEPTION_INVALID		0x00010000
#define FPC_EXCEPTION_UNIMPL		0x00020000
#define FPC_COND_BIT			0x00800000
#define FPC_FLUSH_BIT			0x01000000
#define FPC_MBZ_BITS			0xfe7c0000

/*
 * Constants to determine if have a floating point instruction.
 */
#define OPCODE_SHIFT		26
#define OPCODE_C1		0x11

/*
 * The low part of the TLB entry.
 */
#define VMTLB_PF_NUM		0x3fffffc0
#define VMTLB_ATTR_MASK		0x00000038
#define VMTLB_MOD_BIT		0x00000004
#define VMTLB_VALID_BIT		0x00000002
#define VMTLB_GLOBAL_BIT	0x00000001

#define VMTLB_PHYS_PAGE_SHIFT	6

/*
 * The high part of the TLB entry.
 */
#define VMTLB_VIRT_PAGE_NUM	0xffffe000
#define VMTLB_PID		0x000000ff
#define VMTLB_PID_SHIFT		0
#define VMTLB_VIRT_PAGE_SHIFT	12

/*
 * The number of process id entries.
 */
#define	VMNUM_PIDS		256

/*
 * TLB probe return codes.
 */
#define VMTLB_NOT_FOUND		0
#define VMTLB_FOUND		1
#define VMTLB_FOUND_WITH_PATCH	2
#define VMTLB_PROBE_ERROR	3

/*
 * Exported definitions unique to mips cpu support.
 */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	COPY_SIGCODE		/* copy sigcode above user stack in exec */

#define	cpu_wait(p)		/* nothing */
#define cpu_swapout(p)		panic("cpu_swapout: can't get here");

#ifndef _LOCORE
#include <machine/frame.h>
/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
extern int int_nest_cntr;
#define clockframe trap_frame	/* Use normal trap frame */

#define	CLKF_USERMODE(framep)	((framep)->sr & SR_KSU_USER)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(int_nest_cntr > 0)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched(info)	{ want_resched = 1; aston(); }

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the PICA, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	{ (p)->p_flag |= P_OWEUPC; aston(); }

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

#define aston()		(astpending = 1)

extern int want_resched;	/* resched() was called */

#endif /* !_LOCORE */
#endif /* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_ALLOWAPERTURE	1	/* allow mmap of /dev/xf86 */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "allowaperture", CTLTYPE_INT }, \
}

/*
 * MIPS CPU types (cp_imp).
 */
#define	MIPS_R2000	0x01	/* MIPS R2000 CPU		ISA I   */
#define	MIPS_R3000	0x02	/* MIPS R3000 CPU		ISA I   */
#define	MIPS_R6000	0x03	/* MIPS R6000 CPU		ISA II	*/
#define	MIPS_R4000	0x04	/* MIPS R4000/4400 CPU		ISA III	*/
#define MIPS_R3LSI	0x05	/* LSI Logic R3000 derivate	ISA I	*/
#define	MIPS_R6000A	0x06	/* MIPS R6000A CPU		ISA II	*/
#define	MIPS_R3IDT	0x07	/* IDT R3000 derivate		ISA I	*/
#define	MIPS_R10000	0x09	/* MIPS R10000/T5 CPU		ISA IV  */
#define	MIPS_R4200	0x0a	/* MIPS R4200 CPU (ICE)		ISA III */
#define MIPS_R4300	0x0b	/* NEC VR4300 CPU		ISA III */
#define MIPS_R4100	0x0c	/* NEC VR41xx CPU MIPS-16	ISA III */
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
#define	MIPS_VR5400	0x54	/* NEC Vr5400 CPU		ISA IV+ */

/*
 * MIPS FPU types. Only soft, rest is teh same as cpu type.
 */
#define	MIPS_SOFT	0x00	/* Software emulation		ISA I   */


#if defined(_KERNEL) && !defined(_LOCORE)

extern u_int	CpuPrimaryInstCacheSize;
extern u_int	CpuPrimaryInstCacheLSize;
extern u_int	CpuPrimaryInstSetSize;
extern u_int	CpuPrimaryDataCacheSize;
extern u_int	CpuPrimaryDataCacheLSize;
extern u_int	CpuPrimaryDataSetSize;
extern u_int	CpuCacheAliasMask;
extern u_int	CpuSecondaryCacheSize;
extern u_int	CpuTertiaryCacheSize;
extern u_int	CpuNWayCache;
extern u_int	CpuCacheType;		/* R4K, R5K, RM7K */
extern u_int	CpuConfigRegister;
extern u_int	CpuStatusRegister;
extern u_int	CpuExternalCacheOn;	/* R5K, RM7K */
extern u_int	CpuOnboardCacheOn;	/* RM7K */

struct tlb_entry;
struct user;

void	tlb_set_wired(int);
void	tlb_set_pid(int);
u_int	cp0_get_prid(void);
u_int	cp1_get_prid(void);
u_int	cp0_get_count(void);
void	cp0_set_compare(u_int);

/*
 *  Define soft selected cache functions.
 */
#define	Mips_SyncCache()	(*(sys_config._SyncCache))()
#define	Mips_InvalidateICache(a, l)	\
				(*(sys_config._InvalidateICache))((a), (l))
#define	Mips_InvalidateICachePage(a)	\
				(*(sys_config._InvalidateICachePage))((a))
#define	Mips_SyncDCachePage(a)		\
				(*(sys_config._SyncDCachePage))((a))
#define	Mips_HitSyncDCache(a, l)	\
				(*(sys_config._HitSyncDCache))((a), (l))
#define	Mips_IOSyncDCache(a, l, h)	\
				(*(sys_config._IOSyncDCache))((a), (l), (h))
#define	Mips_HitInvalidateDCache(a, l)	\
				(*(sys_config._HitInvalidateDCache))((a), (l))

int	Mips5k_ConfigCache(void);
void	Mips5k_SyncCache(void);
void	Mips5k_InvalidateICache(vaddr_t, int);
void	Mips5k_InvalidateICachePage(vaddr_t);
void	Mips5k_SyncDCachePage(vaddr_t);
void	Mips5k_HitSyncDCache(vaddr_t, int);
void	Mips5k_IOSyncDCache(vaddr_t, int, int);
void	Mips5k_HitInvalidateDCache(vaddr_t, int);

int	Mips10k_ConfigCache(void);
void	Mips10k_SyncCache(void);
void	Mips10k_InvalidateICache(vaddr_t, int);
void	Mips10k_InvalidateICachePage(vaddr_t);
void	Mips10k_SyncDCachePage(vaddr_t);
void	Mips10k_HitSyncDCache(vaddr_t, int);
void	Mips10k_IOSyncDCache(vaddr_t, int, int);
void	Mips10k_HitInvalidateDCache(vaddr_t, int);

void	tlb_flush(int);
void	tlb_flush_addr(vaddr_t);
void	tlb_write_indexed(int, struct tlb_entry *);
int	tlb_update(vaddr_t, unsigned);
void	tlb_read(int, struct tlb_entry *);

void	wbflush(void);
void	savectx(struct user *, int);
int	copykstack(struct user *);
void	switch_exit(struct proc *);
void	MipsSaveCurFPState(struct proc *);
void	MipsSaveCurFPState16(struct proc *);

extern u_int32_t cpu_counter_interval;  /* Number of counter ticks/tick */
extern u_int32_t cpu_counter_last;      /* Last compare value loaded    */

/*
 * Enable realtime clock (always enabled).
 */
#define	enablertclock()

/*
 *  Low level access routines to CPU registers
 */

void setsoftintr0(void);
void clearsoftintr0(void);
void setsoftintr1(void);
void clearsoftintr1(void);
u_int32_t enableintr(void);
u_int32_t disableintr(void);
u_int32_t updateimask(intrmask_t);
void setsr(u_int32_t);
u_int32_t getsr(void);

#endif /* _KERNEL */
#endif /* !_MIPS_CPU_H_ */
