/*	$OpenBSD: cpu.h,v 1.2 1998/03/16 09:03:04 pefo Exp $	*/

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

#ifndef _CPU_H_
#define _CPU_H_

#include <machine/psl.h>

#define KUSEG_ADDR		0x0
#define CACHED_MEMORY_ADDR	0x80000000
#define UNCACHED_MEMORY_ADDR	0xa0000000
#define KSEG2_ADDR		0xc0000000
#define MAX_MEM_ADDR		0xbe000000
#define	RESERVED_ADDR		0xbfc80000

#define	CACHED_TO_PHYS(x)	((unsigned)(x) & 0x1fffffff)
#define	PHYS_TO_CACHED(x)	((unsigned)(x) | CACHED_MEMORY_ADDR)
#define	UNCACHED_TO_PHYS(x) 	((unsigned)(x) & 0x1fffffff)
#define	PHYS_TO_UNCACHED(x) 	((unsigned)(x) | UNCACHED_MEMORY_ADDR)
#define VA_TO_CINDEX(x) 	((unsigned)(x) & 0xffffff | CACHED_MEMORY_ADDR)
#define	CACHED_TO_UNCACHED(x)	(PHYS_TO_UNCACHED(CACHED_TO_PHYS(x)))

#ifdef _KERNEL
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
#define CR_IP			0x0000FF00
#define CR_EXC_CODE_SHIFT	2

/*
 * The bits in the status register.  All bits are active when set to 1.
 */
#define SR_COP_USABILITY	0xf0000000
#define SR_COP_0_BIT		0x10000000
#define SR_COP_1_BIT		0x20000000
#define SR_RP			0x08000000
#define SR_FR_32		0x04000000
#define SR_RE			0x02000000
#define SR_BOOT_EXC_VEC		0x00400000
#define SR_TLB_SHUTDOWN		0x00200000
#define SR_SOFT_RESET		0x00100000
#define SR_DIAG_CH		0x00040000
#define SR_DIAG_CE		0x00020000
#define SR_DIAG_PE		0x00010000
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
/*#define SR_INT_MASK		0x0000ff00*/

/*
 * The interrupt masks.
 * If a bit in the mask is 1 then the interrupt is enabled (or pending).
 */
#define INT_MASK		0xff00
#define INT_MASK_5		0x8000
#define INT_MASK_4		0x4000
#define INT_MASK_3		0x2000
#define INT_MASK_2		0x1000
#define INT_MASK_1		0x0800
#define INT_MASK_0		0x0400
#define HARD_INT_MASK		0xfc00
#define SOFT_INT_MASK_1		0x0200
#define SOFT_INT_MASK_0		0x0100

/*
 * The bits in the context register.
 */
#define CNTXT_PTE_BASE		0xff800000
#define CNTXT_BAD_VPN2		0x007ffff0

/*
 * Location of exception vectors.
 */
#define RESET_EXC_VEC		0xbfc00000
#define TLB_MISS_EXC_VEC	0x80000000
#define XTLB_MISS_EXC_VEC	0x80000080
#define CACHE_ERR_EXC_VEC	0x80000100
#define GEN_EXC_VEC		0x80000180

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
 * The number of TLB entries and the first one that write random hits.
 */
/*#define VMNUM_TLB_ENTRIES	48	XXX We never use this... */
#define VMWIRED_ENTRIES	 	8

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
#define cpu_set_init_frame(p, fp) /* nothing */
#define cpu_swapout(p)		panic("cpu_swapout: can't get here");

#ifndef _LOCORE
/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
struct clockframe {
	int	pc;	/* program counter at time of interrupt */
	int	sr;	/* status register at time of interrupt */
	int	cr;	/* cause register at time of interrupt */
};

#define	CLKF_USERMODE(framep)	((framep)->sr & SR_KSU_USER)
#define	CLKF_BASEPRI(framep)	((~(framep)->sr & (INT_MASK|SR_INT_ENAB)) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(0)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched()	{ want_resched = 1; aston(); }

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

int	astpending;	/* need to trap before returning to user mode */
int	want_resched;	/* resched() was called */

/*
 * CPU identification, from PRID register.
 */
union cpuprid {
	int	cpuprid;
	struct {
#if BYTE_ORDER == BIG_ENDIAN
		u_int	pad1:16;	/* reserved */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_minrev:4;	/* minor revision identifier */
#else
		u_int	cp_minrev:4;	/* minor revision identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	pad1:16;	/* reserved */
#endif
	} cpu;
};

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#endif /* !_LOCORE */
#endif /* _KERNEL */

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
#define MIPS_UNKC2	0x0c	/* unnanounced product cpu	ISA III */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III */
#define	MIPS_R4700	0x21	/* QED R4700 Orion		ISA III */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based CPU	ISA I	*/
#define	MIPS_R5000	0x23	/* MIPS R5000 based CPU		ISA IV  */
#define	MIPS_RM52X0	0x28	/* QED RM52X0 based CPU		ISA IV  */

/*
 * MIPS FPU types
 */
#define	MIPS_SOFT	0x00	/* Software emulation		ISA I   */
#define	MIPS_R2360	0x01	/* MIPS R2360 FPC		ISA I   */
#define	MIPS_R2010	0x02	/* MIPS R2010 FPC		ISA I   */
#define	MIPS_R3010	0x03	/* MIPS R3010 FPC		ISA I   */
#define	MIPS_R6010	0x04	/* MIPS R6010 FPC		ISA II  */
#define	MIPS_R4010	0x05	/* MIPS R4000/R4400 FPC		ISA II  */
#define MIPS_R31LSI	0x06	/* LSI Logic derivate		ISA I	*/
#define	MIPS_R10010	0x09	/* MIPS R10000/T5 FPU		ISA IV  */
#define	MIPS_R4210	0x0a	/* MIPS R4200 FPC (ICE)		ISA III */
#define MIPS_UNKF1	0x0b	/* unnanounced product cpu	ISA III */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III */
#define	MIPS_R3SONY	0x21	/* Sony R3000 based FPU		ISA I   */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based FPU	ISA I	*/
#define	MIPS_R5010	0x23	/* MIPS R5000 based FPU		ISA IV  */
#define	MIPS_RM5230	0x28	/* QED RM52X0 based FPU		ISA IV  */

#if defined(_KERNEL) && !defined(_LOCORE)
union	cpuprid cpu_id;
union	cpuprid fpu_id;
u_int	CpuPrimaryDataCacheSize;
u_int	CpuPrimaryInstCacheSize;
u_int	CpuPrimaryDataCacheLSize;
u_int	CpuPrimaryInstCacheLSize;
u_int	CpuCacheAliasMask;
u_int	CpuTwoWayCache;
int	l2cache_is_snooping;
extern	struct intr_tab intr_tab[];

struct tlb;
struct user;

int	R4K_ConfigCache __P((void));
void	R4K_SetWIRED __P((int));
void	R4K_SetPID __P((int));
u_int	R4K_GetCOUNT __P((void));
void	R4K_SetCOMPARE __P((u_int));
void	R4K_FlushCache __P((void));
void	R4K_FlushDCache __P((vm_offset_t, int));
void	R4K_HitFlushDCache __P((vm_offset_t, int));
void	R4K_FlushICache __P((vm_offset_t, int));
void	R4K_TLBFlush __P((int));
void	R4K_TLBFlushAddr __P((vm_offset_t));
void	R4K_TLBWriteIndexed __P((int, struct tlb *));
void	R4K_TLBUpdate __P((vm_offset_t, unsigned));
void	R4K_TLBRead __P((int, struct tlb *));
void	wbflush __P((void));
void	savectx __P((struct user *, int));
int	copykstack __P((struct user *));
void	switch_exit __P((void));
void	MipsSaveCurFPState __P((struct proc *));

extern u_int32_t cpu_counter_interval;  /* Number of counter ticks/tick */
extern u_int32_t cpu_counter_last;      /* Last compare value loaded    */

/*
 * Enable realtime clock (always enabled).
 */
#define	enablertclock()
#endif /* _KERNEL */

#endif /* _CPU_H_ */
