/*	$OpenBSD: mips_cpu.h,v 1.2 2015/09/24 18:38:58 miod Exp $	*/

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

#ifndef _MIPS64_CPUREGS_H_
#define	_MIPS64_CPUREGS_H_

#if defined(_KERNEL) || defined(_STANDALONE)

/*
 * Status register.
 */

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
#define	SR_ERL			0x00000004
#define	SR_EXL			0x00000002
#define	SR_INT_ENAB		0x00000001

#define	SOFT_INT_MASK_0		0x00000100
#define	SOFT_INT_MASK_1		0x00000200
#define	SR_INT_MASK_0		0x00000400
#define	SR_INT_MASK_1		0x00000800
#define	SR_INT_MASK_2		0x00001000
#define	SR_INT_MASK_3		0x00002000
#define	SR_INT_MASK_4		0x00004000
#define	SR_INT_MASK_5		0x00008000

/* R8000-specific bits */
#define	SR_SERIALIZE_FPU	0x0000010000000000
#define	SR_KPGSZ_SHIFT		36
#define	SR_UPGSZ_SHIFT		32
#define	SR_PGSZ_4K		0
#define	SR_PGSZ_8K		1
#define	SR_PGSZ_16K		2
#define	SR_PGSZ_64K		3
#define	SR_PGSZ_1M		4
#define	SR_PGSZ_4M		5
#define	SR_PGSZ_16M		6
#define	SR_PGSZ_MASK		0x0f

#define	SR_INT_MASK_6		0x00010000
#define	SR_INT_MASK_7		0x00020000
#define	SR_INT_MASK_8		0x00040000

#ifdef CPU_R8000
#define	SR_XX			0x00000040
#define	SR_KSU_MASK		0x00000010
#define	SR_KSU_KERNEL		0x00000000
#define	SR_INT_MASK		0x0007ff00
#else
#define	SR_XX			0x80000000
#define	SR_KSU_MASK		0x00000018
#define	SR_KSU_SUPER		0x00000008
#define	SR_KSU_KERNEL		0x00000000
#define	SR_INT_MASK		0x0000ff00
#endif
/* SR_KSU_USER is in <mips64/cpu.h> for CLKF_USERMODE() */
#ifndef SR_KSU_USER
#define	SR_KSU_USER		0x00000010
#endif

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

#define	SOFT_INT_MASK		(SOFT_INT_MASK_0 | SOFT_INT_MASK_1)

/*
 * Cause register.
 */

#ifdef CPU_R8000
#define	CR_BR_DELAY		0x8000000000000000
#define	CR_EXC_CODE		0x000000f8
#define	CR_EXC_CODE_SHIFT	3
#define	CR_COP_ERR		0x10000000
#else
#define	CR_BR_DELAY		0x80000000
#define	CR_EXC_CODE		0x0000007c
#define	CR_EXC_CODE_SHIFT	2
#define	CR_COP_ERR		0x30000000
#endif
#define	CR_COP1_ERR		0x10000000
#define	CR_COP2_ERR		0x20000000
#define	CR_COP3_ERR		0x20000000
#define	CR_INT_SOFT0		0x00000100
#define	CR_INT_SOFT1		0x00000200
#define	CR_INT_0		0x00000400
#define	CR_INT_1		0x00000800
#define	CR_INT_2		0x00001000
#define	CR_INT_3		0x00002000
#define	CR_INT_4		0x00004000
#define	CR_INT_5		0x00008000
/* Following on RM7000 and R8000 */
#define	CR_INT_6		0x00010000
#define	CR_INT_7		0x00020000
#define	CR_INT_8		0x00040000
/* Following on RM7000 */
#define	CR_INT_9		0x00080000
#define	CR_INT_HARD		0x000ffc00
#define	CR_INT_TIMR		0x00100000	/* 12 Timer */
#define	CR_INT_PERF		0x00200000	/* 13 Performance counter */
/* R8000 specific */
#define	CR_FPE			0x01000000
#define	CR_VCE			0x02000000
#define	CR_BERR			0x04000000
#define	CR_NMI			0x08000000

#ifdef CPU_R8000
#define	CR_INT_MASK		0x0007ff00
#else
#define	CR_INT_MASK		0x003fff00
#endif

/*
 * Config register.
 */

#define	CFGR_CCA_MASK		0x00000007
#define	CFGR_CU			0x00000008
#define	CFGR_ICE		0x0000000200000000
#define	CFGR_SMM		0x0000000400000000

/*
 * Location of exception vectors.
 */

#ifdef CPU_R8000
#define	RESET_EXC_VEC		PHYS_TO_XKPHYS(0x1fc00000, CCA_NC)
/* all the others are relative to COP_0_TRAPBASE */
/* #define	UTLB_MISS_EXC_VEC	0x00000000 */
/* #define	KV1TLB_MISS_EXC_VEC	0x00000400 */
/* #define	KV0TLB_MISS_EXC_VEC	0x00000800 */
/* #define	GEN_EXC_VEC		0x00000c00 */
#else
#define	RESET_EXC_VEC		(CKSEG1_BASE + 0x1fc00000)
#define	TLB_MISS_EXC_VEC	(CKSEG1_BASE + 0x00000000)
#define	XTLB_MISS_EXC_VEC	(CKSEG1_BASE + 0x00000080)
#define	CACHE_ERR_EXC_VEC	(CKSEG1_BASE + 0x00000100)
#define	GEN_EXC_VEC		(CKSEG1_BASE + 0x00000180)
#endif

/*
 * Coprocessor 0 registers
 */

/* Common subset */
#define	COP_0_COUNT		$9
#define	COP_0_TLB_HI		$10
#define	COP_0_STATUS_REG	$12
#define	COP_0_CAUSE_REG		$13
#define	COP_0_EXC_PC		$14
#define	COP_0_PRID		$15
#define	COP_0_CONFIG		$16

/* R4000/5000/10000 */
#define	COP_0_TLB_INDEX		$0
#define	COP_0_TLB_RANDOM	$1
#define	COP_0_TLB_LO0		$2
#define	COP_0_TLB_LO1		$3
#define	COP_0_TLB_CONTEXT	$4
#define	COP_0_TLB_PG_MASK	$5
#define	COP_0_TLB_WIRED		$6
#define	COP_0_BAD_VADDR		$8
#define	COP_0_COMPARE		$11
#define	COP_0_LLADDR		$17
#define	COP_0_WATCH_LO		$18
#define	COP_0_WATCH_HI		$19
#define	COP_0_TLB_XCONTEXT	$20
#define	COP_0_ECC		$26
#define	COP_0_CACHE_ERR		$27
#define	COP_0_TAG_LO		$28
#define	COP_0_TAG_HI		$29
#define	COP_0_ERROR_PC		$30

/* R8000 specific */
#define	COP_0_TLB_SET		$0
#define	COP_0_TLB_LO		$2
#define	COP_0_UBASE		$4
#define	COP_0_SHIFTAMT		$5
#define	COP_0_TRAPBASE		$6
#define	COP_0_BAD_PADDR		$7
#define	COP_0_VADDR		$8
#define	COP_0_WORK0		$18
#define	COP_0_WORK1		$19
#define	COP_0_PBASE		$20
#define	COP_0_GBASE		$21
#define	COP_0_TFP_TLB_WIRED	$24
#define	COP_0_DCACHE		$28
#define	COP_0_ICACHE		$29

/* RM7000 specific */
#define	COP_0_WATCH_1		$18
#define	COP_0_WATCH_2		$19
#define	COP_0_WATCH_M		$24
#define	COP_0_PC_COUNT		$25
#define	COP_0_PC_CTRL		$22

#define	COP_0_ICR		$20	/* Use cfc0/ctc0 to access */

/* R10000 specific */
#define	COP_0_TLB_FR_MASK	$21

/* Loongson-2 specific */
#define	COP_0_DIAG		$22

/* Octeon specific */
#define COP_0_TLB_PG_GRAIN	$5, 1
#define COP_0_CVMCTL		$9, 7
#define COP_0_CVMMEMCTL		$11, 7
#define COP_0_EBASE		$15, 1

/*
 * COP_0_COUNT speed divider.
 */
#if defined(CPU_OCTEON) || defined(CPU_R8000)
#define	CP0_CYCLE_DIVIDER	1
#else
#define	CP0_CYCLE_DIVIDER	2
#endif

/*
 * The floating point version and status registers.
 */
#define	FPC_ID			$0
#define	FPC_CSR			$31

#endif	/* _KERNEL || _STANDALONE */

#endif /* !_MIPS64_CPUREGS_H_ */
