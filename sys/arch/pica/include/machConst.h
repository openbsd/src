/*
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
 *	from: @(#)machConst.h	8.1 (Berkeley) 6/10/93
 *      $Id: machConst.h,v 1.1.1.1 1995/10/18 10:39:12 deraadt Exp $
 *
 * machConst.h --
 *
 *	Machine dependent constants.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machConst.h,
 *	v 9.2 89/10/21 15:55:22 jhh Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAddrs.h,
 *	v 1.2 89/08/15 18:28:21 rab Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/RCS/vmPmaxConst.h,
 *	v 9.1 89/09/18 17:33:00 shirriff Exp  SPRITE (DECWRL)
 * $Id: machConst.h,v 1.1.1.1 1995/10/18 10:39:12 deraadt Exp $
 */

#ifndef _MACHCONST
#define _MACHCONST

#define MACH_KUSEG_ADDR			0x0
#define MACH_CACHED_MEMORY_ADDR		0x80000000
#define MACH_UNCACHED_MEMORY_ADDR	0xa0000000
#define MACH_KSEG2_ADDR			0xc0000000
#define MACH_MAX_MEM_ADDR		0xbe000000
#define	MACH_RESERVED_ADDR		0xbfc80000

#define	MACH_CACHED_TO_PHYS(x)	((unsigned)(x) & 0x1fffffff)
#define	MACH_PHYS_TO_CACHED(x)	((unsigned)(x) | MACH_CACHED_MEMORY_ADDR)
#define	MACH_UNCACHED_TO_PHYS(x) ((unsigned)(x) & 0x1fffffff)
#define	MACH_PHYS_TO_UNCACHED(x) ((unsigned)(x) | MACH_UNCACHED_MEMORY_ADDR)
#define MACH_VA_TO_CINDEX(x) \
		((unsigned)(x) & 0xffffff | MACH_CACHED_MEMORY_ADDR)

#define MACH_CODE_START			0x80080000

/*
 * The bits in the cause register.
 *
 *	MACH_CR_BR_DELAY	Exception happened in branch delay slot.
 *	MACH_CR_COP_ERR		Coprocessor error.
 *	MACH_CR_IP		Interrupt pending bits defined below.
 *	MACH_CR_EXC_CODE	The exception type (see exception codes below).
 */
#define MACH_CR_BR_DELAY	0x80000000
#define MACH_CR_COP_ERR		0x30000000
#define MACH_CR_EXC_CODE	0x0000007C
#define MACH_CR_IP		0x0000FF00
#define MACH_CR_EXC_CODE_SHIFT	2

/*
 * The bits in the status register.  All bits are active when set to 1.
 */
#define MACH_SR_COP_USABILITY	0xf0000000
#define MACH_SR_COP_0_BIT	0x10000000
#define MACH_SR_COP_1_BIT	0x20000000
#define MACH_SR_RP		0x08000000
#define MACH_SR_FR_32		0x04000000
#define MACH_SR_RE		0x02000000
#define MACH_SR_BOOT_EXC_VEC	0x00400000
#define MACH_SR_TLB_SHUTDOWN	0x00200000
#define MACH_SR_SOFT_RESET	0x00100000
#define MACH_SR_DIAG_CH		0x00040000
#define MACH_SR_DIAG_CE		0x00020000
#define MACH_SR_DIAG_PE		0x00010000
#define MACH_SR_KX		0x00000080
#define MACH_SR_SX		0x00000040
#define MACH_SR_UX		0x00000020
#define MACH_SR_KSU_MASK	0x00000018
#define MACH_SR_KSU_USER	0x00000010
#define MACH_SR_KSU_SUPER	0x00000008
#define MACH_SR_KSU_KERNEL	0x00000000
#define MACH_SR_ERL		0x00000004
#define MACH_SR_EXL		0x00000002
#define MACH_SR_INT_ENAB	0x00000001
/*#define MACH_SR_INT_MASK	0x0000ff00*/

/*
 * The interrupt masks.
 * If a bit in the mask is 1 then the interrupt is enabled (or pending).
 */
#define MACH_INT_MASK		0x7f00
#define MACH_INT_MASK_5		0x8000	/* Not used (on chip timer) */
#define MACH_INT_MASK_4		0x4000
#define MACH_INT_MASK_3		0x2000
#define MACH_INT_MASK_2		0x1000
#define MACH_INT_MASK_1		0x0800
#define MACH_INT_MASK_0		0x0400
#define MACH_HARD_INT_MASK	0x7c00
#define MACH_SOFT_INT_MASK_1	0x0200
#define MACH_SOFT_INT_MASK_0	0x0100

/*
 * The bits in the context register.
 */
#define MACH_CNTXT_PTE_BASE	0xFF800000
#define MACH_CNTXT_BAD_VPN2	0x007FFFF0

/*
 * Location of exception vectors.
 */
#define MACH_RESET_EXC_VEC	0xBFC00000
#define MACH_TLB_MISS_EXC_VEC	0x80000000
#define MACH_XTLB_MISS_EXC_VEC	0x80000080
#define MACH_CACHE_ERR_EXC_VEC	0x80000100
#define MACH_GEN_EXC_VEC	0x80000180

/*
 * Coprocessor 0 registers:
 */
#define MACH_COP_0_TLB_INDEX	$0
#define MACH_COP_0_TLB_RANDOM	$1
#define MACH_COP_0_TLB_LO0	$2
#define MACH_COP_0_TLB_LO1	$3
#define MACH_COP_0_TLB_CONTEXT	$4
#define MACH_COP_0_TLB_PG_MASK	$5
#define MACH_COP_0_TLB_WIRED	$6
#define MACH_COP_0_BAD_VADDR	$8
#define MACH_COP_0_TLB_HI	$10
#define MACH_COP_0_STATUS_REG	$12
#define MACH_COP_0_CAUSE_REG	$13
#define MACH_COP_0_EXC_PC	$14
#define MACH_COP_0_PRID		$15
#define MACH_COP_0_CONFIG	$16
#define MACH_COP_0_LLADDR	$17
#define MACH_COP_0_WATCH_LO	$18
#define MACH_COP_0_WATCH_HI	$19
#define MACH_COP_0_TLB_XCONTEXT	$20
#define MACH_COP_0_ECC		$26
#define MACH_COP_0_CACHE_ERR	$27
#define MACH_COP_0_TAG_LO	$28
#define MACH_COP_0_TAG_HI	$29
#define MACH_COP_0_ERROR_PC	$30

/*
 * Values for the code field in a break instruction.
 */
#define MACH_BREAK_INSTR	0x0000000d
#define MACH_BREAK_VAL_MASK	0x03ff0000
#define MACH_BREAK_VAL_SHIFT	16
#define MACH_BREAK_KDB_VAL	512
#define MACH_BREAK_SSTEP_VAL	513
#define MACH_BREAK_BRKPT_VAL	514
#define MACH_BREAK_SOVER_VAL	515
#define MACH_BREAK_KDB		(MACH_BREAK_INSTR | \
				(MACH_BREAK_KDB_VAL << MACH_BREAK_VAL_SHIFT))
#define MACH_BREAK_SSTEP	(MACH_BREAK_INSTR | \
				(MACH_BREAK_SSTEP_VAL << MACH_BREAK_VAL_SHIFT))
#define MACH_BREAK_BRKPT	(MACH_BREAK_INSTR | \
				(MACH_BREAK_BRKPT_VAL << MACH_BREAK_VAL_SHIFT))
#define MACH_BREAK_SOVER	(MACH_BREAK_INSTR | \
				(MACH_BREAK_SOVER_VAL << MACH_BREAK_VAL_SHIFT))

/*
 * Mininum and maximum cache sizes.
 */
#define MACH_MIN_CACHE_SIZE	(16 * 1024)
#define MACH_MAX_CACHE_SIZE	(256 * 1024)

/*
 * The floating point version and status registers.
 */
#define	MACH_FPC_ID	$0
#define	MACH_FPC_CSR	$31

/*
 * The floating point coprocessor status register bits.
 */
#define MACH_FPC_ROUNDING_BITS		0x00000003
#define MACH_FPC_ROUND_RN		0x00000000
#define MACH_FPC_ROUND_RZ		0x00000001
#define MACH_FPC_ROUND_RP		0x00000002
#define MACH_FPC_ROUND_RM		0x00000003
#define MACH_FPC_STICKY_BITS		0x0000007c
#define MACH_FPC_STICKY_INEXACT		0x00000004
#define MACH_FPC_STICKY_UNDERFLOW	0x00000008
#define MACH_FPC_STICKY_OVERFLOW	0x00000010
#define MACH_FPC_STICKY_DIV0		0x00000020
#define MACH_FPC_STICKY_INVALID		0x00000040
#define MACH_FPC_ENABLE_BITS		0x00000f80
#define MACH_FPC_ENABLE_INEXACT		0x00000080
#define MACH_FPC_ENABLE_UNDERFLOW	0x00000100
#define MACH_FPC_ENABLE_OVERFLOW	0x00000200
#define MACH_FPC_ENABLE_DIV0		0x00000400
#define MACH_FPC_ENABLE_INVALID		0x00000800
#define MACH_FPC_EXCEPTION_BITS		0x0003f000
#define MACH_FPC_EXCEPTION_INEXACT	0x00001000
#define MACH_FPC_EXCEPTION_UNDERFLOW	0x00002000
#define MACH_FPC_EXCEPTION_OVERFLOW	0x00004000
#define MACH_FPC_EXCEPTION_DIV0		0x00008000
#define MACH_FPC_EXCEPTION_INVALID	0x00010000
#define MACH_FPC_EXCEPTION_UNIMPL	0x00020000
#define MACH_FPC_COND_BIT		0x00800000
#define MACH_FPC_FLUSH_BIT		0x01000000
#define MACH_FPC_MBZ_BITS		0xfe7c0000

/*
 * Constants to determine if have a floating point instruction.
 */
#define MACH_OPCODE_SHIFT	26
#define MACH_OPCODE_C1		0x11

/*
 * The low part of the TLB entry.
 */
#define VMMACH_TLB_PF_NUM		0x3fffffc0
#define VMMACH_TLB_ATTR_MASK		0x00000038
#define VMMACH_TLB_MOD_BIT		0x00000004
#define VMMACH_TLB_VALID_BIT		0x00000002
#define VMMACH_TLB_GLOBAL_BIT		0x00000001

#define VMMACH_TLB_PHYS_PAGE_SHIFT	6

/*
 * The high part of the TLB entry.
 */
#define VMMACH_TLB_VIRT_PAGE_NUM	0xffffe000
#define VMMACH_TLB_PID			0x000000ff
#define VMMACH_TLB_PID_SHIFT		0
#define VMMACH_TLB_VIRT_PAGE_SHIFT	12

/*
 * The number of TLB entries and the first one that write random hits.
 */
#define VMMACH_NUM_TLB_ENTRIES		48
#define VMMACH_WIRED_ENTRIES	 	8

/*
 * The number of process id entries.
 */
#define	VMMACH_NUM_PIDS			256

/*
 * TLB probe return codes.
 */
#define VMMACH_TLB_NOT_FOUND		0
#define VMMACH_TLB_FOUND		1
#define VMMACH_TLB_FOUND_WITH_PATCH	2
#define VMMACH_TLB_PROBE_ERROR		3

/*
 * Kernel virtual address for user page table entries
 * (i.e., the address for the context register).
 */
#define VMMACH_PTE_BASE		0xFF800000

#endif /* _MACHCONST */
