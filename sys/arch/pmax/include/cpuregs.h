/*	$NetBSD: cpuregs.h,v 1.5 1996/03/28 11:34:05 jonathan Exp $	*/

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
 *	@(#)machConst.h	8.1 (Berkeley) 6/10/93
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
 */

#ifndef _MACHCONST
#define _MACHCONST

#define MACH_KUSEG_ADDR			0x0
#define MACH_CACHED_MEMORY_ADDR		0x80000000
#define MACH_UNCACHED_MEMORY_ADDR	0xa0000000
#define MACH_KSEG2_ADDR			0xc0000000
#define MACH_MAX_MEM_ADDR		0xbe000000
#define	MACH_RESERVED_ADDR		0xbfc80000

/* XXX - this is just to make libkvm happy - all MACH_CAHED...
         should be changed to CAHED etc. to be in sync with 
         the arc
*/
#define	CACHED_TO_PHYS(x)	((unsigned)(x) & 0x1fffffff)

#define	MACH_CACHED_TO_PHYS(x)	((unsigned)(x) & 0x1fffffff)
#define	MACH_PHYS_TO_CACHED(x)	((unsigned)(x) | MACH_CACHED_MEMORY_ADDR)
#define	MACH_UNCACHED_TO_PHYS(x) ((unsigned)(x) & 0x1fffffff)
#define	MACH_PHYS_TO_UNCACHED(x) ((unsigned)(x) | MACH_UNCACHED_MEMORY_ADDR)

/* Map virtual address to index in r4k virtually-indexed cache */
#define MIPS_R4K_VA_TO_CINDEX(x) \
		((unsigned)(x) & 0xffffff | MACH_CACHED_MEMORY_ADDR)

/* XXX compatibility with Pica port */
#define MACH_VA_TO_CINDEX(x) MIPS_R4K_VA_TO_CINDEX(x)


/*
 * XXX
 * Port-specific constants:
 * Kernel virtual address at which kernel is loaded, and
 * Kernel virtual address for user page table entries
 * (i.e., the address for the context register).
 */
#ifdef pmax
#define MACH_CODE_START			0x80030000
#define VMMACH_PTE_BASE			0xFFC00000
#endif	/* pmax */

#ifdef pica
#define MACH_CODE_START			0x80080000
#define VMMACH_PTE_BASE			0xFF800000
#endif	/* pica */



/*
 * The bits in the cause register.
 *
 * Bits common to r3000 and r4000:
 *
 *	MACH_CR_BR_DELAY	Exception happened in branch delay slot.
 *	MACH_CR_COP_ERR		Coprocessor error.
 *	MACH_CR_IP		Interrupt pending bits defined below.
 *				(same meaning as in CAUSE register).
 *	MACH_CR_EXC_CODE	The exception type (see exception codes below).
 *
 * Differences:
 *  r3k has 4 bits of execption type, r4k has 5 bits.
 */
#define MACH_CR_BR_DELAY	0x80000000
#define MACH_CR_COP_ERR		0x30000000
#define MIPS_3K_CR_EXC_CODE	0x0000003C
#define MIPS_4K_CR_EXC_CODE	0x0000007C
#define MACH_CR_IP		0x0000FF00
#define MACH_CR_EXC_CODE_SHIFT	2

#ifdef pmax /* XXX not used any more, only to satisfy regression tests */
#define MACH_CR_EXC_CODE	MIPS_3K_CR_EXC_CODE
#endif	/* pmax */
#ifdef pica
#define MACH_CR_EXC_CODE	MIPS_4K_CR_EXC_CODE
#endif	/* pica */


/*
 * The bits in the status register.  All bits are active when set to 1.
 *
 *	R3000 status register fields:
 *	MACH_SR_CO_USABILITY	Control the usability of the four coprocessors.
 *	MACH_SR_BOOT_EXC_VEC	Use alternate exception vectors.
 *	MACH_SR_TLB_SHUTDOWN	TLB disabled.
 *
 *	MIPS_SR_INT_IE		Master (current) interrupt enable bit.
 *
 * Differences:
 *	r3k has cache control is via frobbing SR register bits, whereas the
 *	r4k cache control is via explicit instructions.
 *	r3k has a 3-entry stack of kernel/user bits, whereas the
 *	r4k has kernel/supervisor/user.
 */
#define MACH_SR_COP_USABILITY	0xf0000000
#define MACH_SR_COP_0_BIT	0x10000000
#define MACH_SR_COP_1_BIT	0x20000000

	/* r4k and r3k differences, see below */

#define MACH_SR_BOOT_EXC_VEC	0x00400000
#define MACH_SR_TLB_SHUTDOWN	0x00200000

	/* r4k and r3k differences, see below */

#define MIPS_SR_INT_IE		0x00000001
/*#define MACH_SR_MBZ		0x0f8000c0*/	/* Never used, true for r3k */
/*#define MACH_SR_INT_MASK	0x0000ff00*/

#define MACH_SR_INT_ENAB	MIPS_SR_INT_IE	/* backwards compatibility */
#define MACH_SR_INT_ENA_CUR	MIPS_SR_INT_IE	/* backwards compatibility */



/*
 * The R2000/R3000-specific status register bit definitions.
 * all bits are active when set to 1.
 *
 *	MACH_SR_PARITY_ERR	Parity error.
 *	MACH_SR_CACHE_MISS	Most recent D-cache load resulted in a miss.
 *	MACH_SR_PARITY_ZERO	Zero replaces outgoing parity bits.
 *	MACH_SR_SWAP_CACHES	Swap I-cache and D-cache.
 *	MACH_SR_ISOL_CACHES	Isolate D-cache from main memory.
 *				Interrupt enable bits defined below.
 *	MACH_SR_KU_OLD		Old kernel/user mode bit. 1 => user mode.
 *	MACH_SR_INT_ENA_OLD	Old interrupt enable bit.
 *	MACH_SR_KU_PREV		Previous kernel/user mode bit. 1 => user mode.
 *	MACH_SR_INT_ENA_PREV	Previous interrupt enable bit.
 *	MACH_SR_KU_CUR		Current kernel/user mode bit. 1 => user mode.
 */

#define MIPS_3K_PARITY_ERR      0x00100000
#define MIPS_3K_CACHE_MISS      0x00080000
#define MIPS_3K_PARITY_ZERO     0x00040000
#define MIPS_3K_SWAP_CACHES     0x00020000
#define MIPS_3K_ISOL_CACHES     0x00010000

#define MIPS_3K_SR_KU_OLD	0x00000020	/* 2nd stacked KU/IE*/
#define MIPS_3K_SR_INT_ENA_OLD	0x00000010	/* 2nd stacked KU/IE*/
#define MIPS_3K_SR_KU_PREV	0x00000008	/* 1st stacked KU/IE*/
#define MIPS_3K_SR_INT_ENA_PREV	0x00000004	/* 1st stacked KU/IE*/
#define MIPS_3K_SR_KU_CUR	0x00000002	/* current KU */

/* backwards compatibility */
#define MACH_SR_PARITY_ERR	MIPS_3K_PARITY_ERR
#define MACH_SR_CACHE_MISS	MIPS_3K_CACHE_MISS
#define MACH_SR_PARITY_ZERO	MIPS_3K_PARITY_ZERO
#define MACH_SR_SWAP_CACHES	MIPS_3K_SWAP_CACHES
#define MACH_SR_ISOL_CACHES	MIPS_3K_ISOL_CACHES

#define MACH_SR_KU_OLD		MIPS_3K_SR_KU_OLD
#define MACH_SR_INT_ENA_OLD	MIPS_3K_SR_INT_ENA_OLD
#define MACH_SR_KU_PREV		MIPS_3K_SR_KU_PREV
#define MACH_SR_KU_CUR		MIPS_3K_SR_KU_CUR
#define MACH_SR_INT_ENA_PREV	MIPS_3K_SR_INT_ENA_PREV


/*
 * R4000 status register bit definitons,
 * where different from r2000/r3000.
 */
#define MIPS_4K_SR_RP   	0x08000000
#define MIPS_4K_SR_FR_32	0x04000000
#define MIPS_4K_SR_RE   	0x02000000

#define MIPS_4K_SR_SOFT_RESET	0x00100000
#define MIPS_4K_SR_DIAG_CH	0x00040000
#define MIPS_4K_SR_DIAG_CE	0x00020000
#define MIPS_4K_SR_DIAG_PE	0x00010000
#define MIPS_4K_SR_KX		0x00000080
#define MIPS_4K_SR_SX		0x00000040
#define MIPS_4K_SR_UX		0x00000020
#define MIPS_4K_SR_KSU_MASK	0x00000018
#define MIPS_4K_SR_KSU_USER	0x00000010
#define MIPS_4K_SR_KSU_SUPER	0x00000008
#define MIPS_4K_SR_KSU_KERNEL	0x00000000
#define MIPS_4K_SR_ERL		0x00000004
#define MIPS_4K_SR_EXL		0x00000002

/* backwards compatibility with names used in Pica port */
#define MACH_SR_RP		MIPS_4K_SR_RP   
#define MACH_SR_FR_32		MIPS_4K_SR_FR_32
#define MACH_SR_RE		MIPS_4K_SR_RE   

#define MACH_SR_SOFT_RESET	MIPS_4K_SR_SOFT_RESET
#define MACH_SR_DIAG_CH		MIPS_4K_SR_DIAG_CH
#define MACH_SR_DIAG_CE		MIPS_4K_SR_DIAG_CE
#define MACH_SR_DIAG_PE		MIPS_4K_SR_DIAG_PE
#define MACH_SR_KX		MIPS_4K_SR_KX
#define MACH_SR_SX		MIPS_4K_SR_SX
#define MACH_SR_UX		MIPS_4K_SR_UX

#define MACH_SR_KSU_MASK	MIPS_4K_SR_KSU_MASK
#define MACH_SR_KSU_USER	MIPS_4K_SR_KSU_USER
#define MACH_SR_KSU_SUPER	MIPS_4K_SR_KSU_SUPER
#define MACH_SR_KSU_KERNEL	MIPS_4K_SR_KSU_KERNEL
#define MACH_SR_ERL		MIPS_4K_SR_ERL
#define MACH_SR_EXL		MIPS_4K_SR_EXL


/*
 * The interrupt masks.
 * If a bit in the mask is 1 then the interrupt is enabled (or pending).
 */
#define MIPS_INT_MASK		0xff00
#define MACH_INT_MASK_5		0x8000
#define MACH_INT_MASK_4		0x4000
#define MACH_INT_MASK_3		0x2000
#define MACH_INT_MASK_2		0x1000
#define MACH_INT_MASK_1		0x0800
#define MACH_INT_MASK_0		0x0400
#define MIPS_HARD_INT_MASK	0xfc00
#define MACH_SOFT_INT_MASK_1	0x0200
#define MACH_SOFT_INT_MASK_0	0x0100

#ifdef pmax
#define MACH_INT_MASK		MIPS_INT_MASK
#define MACH_HARD_INT_MASK	MIPS_HARD_INT_MASK
#endif

/* r4000 has on-chip timer at INT_MASK_5 */
#ifdef pica
#define MACH_INT_MASK		(MIPS_INT_MASK &  ~MACH_INT_MASK_5)
#define MACH_HARD_INT_MASK	(MIPS_HARD_INT_MASK & ~MACH_INT_MASK_5)
#endif



/*
 * The bits in the context register.
 */
#define MIPS_3K_CNTXT_PTE_BASE  0xFFE00000
#define MIPS_3K_CNTXT_BAD_VPN   0x001FFFFC

#define MIPS_4K_CNTXT_PTE_BASE	0xFF800000
#define MIPS_4K_CNTXT_BAD_VPN2	0x007FFFF0

/*
 * Backwards compatbility -- XXX more thought
 */
#ifdef pmax
#define MACH_CNTXT_PTE_BASE	MIPS_3K_CNTXT_PTE_BASE
#define MACH_CNTXT_BAD_VPN	MIPS_3K_CNTXT_BAD_VPN
#endif	/* pmax */

#ifdef pica
#define MACH_CNTXT_PTE_BASE	MIPS_4K_CNTXT_PTE_BASE
#define MACH_CNTXT_BAD_VPN2	MIPS_4K_CNTXT_BAD_VPN2
#endif	/* pica */



/*
 * Location of exception vectors.
 *
 * Common vectors:  reset and UTLB miss.
 */
#define MACH_RESET_EXC_VEC	0xBFC00000
#define MACH_UTLB_MISS_EXC_VEC	0x80000000

/*
 * R3000 general exception vector (everything else)
 */
#define MIPS_3K_GEN_EXC_VEC	0x80000080

/*
 * R4000 MIPS-III exception vectors
 */
#define MIPS_4K_XTLB_MISS_EXC_VEC	0x80000080
#define MIPS_4K_CACHE_ERR_EXC_VEC	0x80000100
#define MIPS_4K_GEN_EXC_VEC		0x80000180

/*
 * Backwards compatbility -- XXX more thought
 */
#ifdef pmax
#define MACH_GEN_EXC_VEC	MIPS_3K_GEN_EXC_VEC
#endif	/* pmax */

#ifdef pica
#define MACH_GEN_EXC_VEC	MIPS_4K_GEN_EXC_VEC
#define MACH_TLB_MISS_EXC_VEC	MACH_UTLB_MISS_EXC_VEC	/* locore compat */
#define MACH_XTLB_MISS_EXC_VEC	MIPS_4K_XTLB_MISS_EXC_VEC
#define MACH_CACHE_ERR_EXC_VEC	MIPS_4K_CACHE_ERR_EXC_VEC
#endif	/* pica */



/*
 * Coprocessor 0 registers:
 *
 *	MACH_COP_0_TLB_INDEX	TLB index.
 *	MACH_COP_0_TLB_RANDOM	TLB random.
 *	MACH_COP_0_TLB_LOW	r3k TLB entry low.
 *	MACH_COP_0_TLB_LO0	r4k TLB entry low.
 *	MACH_COP_0_TLB_LO1	r4k TLB entry low, extended.
 *	MACH_COP_0_TLB_CONTEXT	TLB context.
 *	MACH_COP_0_BAD_VADDR	Bad virtual address.
 *	MACH_COP_0_TLB_HI	TLB entry high.
 *	MACH_COP_0_STATUS_REG	Status register.
 *	MACH_COP_0_CAUSE_REG	Exception cause register.
 *	MACH_COP_0_EXC_PC	Exception PC.
 *	MACH_COP_0_PRID		Processor revision identifier.
 */
#define MACH_COP_0_TLB_INDEX	$0
#define MACH_COP_0_TLB_RANDOM	$1
	/* Name and meaning of  TLB bits for $2 differ on r3k and r4k. */

#define MACH_COP_0_TLB_CONTEXT	$4
					/* $5 and $6 new with MIPS-III */
#define MACH_COP_0_BAD_VADDR	$8
#define MACH_COP_0_TLB_HI	$10
#define MACH_COP_0_STATUS_REG	$12
#define MACH_COP_0_CAUSE_REG	$13
#define MACH_COP_0_EXC_PC	$14
#define MACH_COP_0_PRID		$15


/* r3k-specific */
#define MACH_COP_0_TLB_LOW	$2

/* MIPS-III additions */
#define MACH_COP_0_TLB_LO0	$2
#define MACH_COP_0_TLB_LO1	$3

#define MACH_COP_0_TLB_PG_MASK	$5
#define MACH_COP_0_TLB_WIRED	$6

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
#define MACH_FPC_FLUSH_BIT		0x01000000	/* r4k,  MBZ on r3k */
#define MIPS_3K_FPC_MBZ_BITS		0xff7c0000
#define MIPS_4K_FPC_MBZ_BITS		0xfe7c0000


/*
 * Constants to determine if have a floating point instruction.
 */
#define MACH_OPCODE_SHIFT	26
#define MACH_OPCODE_C1		0x11



/*
 * The low part of the TLB entry.
 */
#define VMMACH_MIPS_3K_TLB_PHYS_PAGE_SHIFT	12
#define VMMACH_MIPS_3K_TLB_PF_NUM		0xfffff000
#define VMMACH_MIPS_3K_TLB_NON_CACHEABLE_BIT	0x00000800
#define VMMACH_MIPS_3K_TLB_MOD_BIT		0x00000400
#define VMMACH_MIPS_3K_TLB_VALID_BIT		0x00000200
#define VMMACH_MIPS_3K_TLB_GLOBAL_BIT		0x00000100

#define VMMACH_MIPS_4K_TLB_PHYS_PAGE_SHIFT	6
#define VMMACH_MIPS_4K_TLB_PF_NUM		0x3fffffc0
#define VMMACH_MIPS_4K_TLB_ATTR_MASK		0x00000038
#define VMMACH_MIPS_4K_TLB_MOD_BIT		0x00000004
#define VMMACH_MIPS_4K_TLB_VALID_BIT		0x00000002
#define VMMACH_MIPS_4K_TLB_GLOBAL_BIT		0x00000001


#ifdef pmax /* XXX */
#define VMMACH_TLB_PHYS_PAGE_SHIFT	VMMACH_MIPS_3K_TLB_PHYS_PAGE_SHIFT
#define VMMACH_TLB_PF_NUM		VMMACH_MIPS_3K_TLB_PF_NUM
#define VMMACH_TLB_NON_CACHEABLE_BIT	VMMACH_MIPS_3K_TLB_NON_CACHEABLE_BIT
#define VMMACH_TLB_MOD_BIT		VMMACH_MIPS_3K_TLB_MOD_BIT
#define VMMACH_TLB_VALID_BIT		VMMACH_MIPS_3K_TLB_VALID_BIT
#define VMMACH_TLB_GLOBAL_BIT		VMMACH_MIPS_3K_TLB_GLOBAL_BIT
#endif /* pmax */

#ifdef pica /*  XXX */
#define VMMACH_TLB_PHYS_PAGE_SHIFT	VMMACH_MIPS_4K_TLB_PHYS_PAGE_SHIFT
#define VMMACH_TLB_PF_NUM		VMMACH_MIPS_4K_TLB_PF_NUM
#define VMMACH_TLB_ATTR_MASK		VMMACH_MIPS_4K_TLB_ATTR_MASK
#define VMMACH_TLB_MOD_BIT		VMMACH_MIPS_4K_TLB_MOD_BIT
#define VMMACH_TLB_VALID_BIT		VMMACH_MIPS_4K_TLB_VALID_BIT
#define VMMACH_TLB_GLOBAL_BIT		VMMACH_MIPS_4K_TLB_GLOBAL_BIT
#endif	/* pica */



/*
 * The high part of the TLB entry.
 */
#define VMMACH_TLB_VIRT_PAGE_SHIFT		12

#define VMMACH_TLB_MIPS_3K_VIRT_PAGE_NUM	0xfffff000
#define VMMACH_TLB_MIPS_3K_PID			0x00000fc0
#define VMMACH_TLB_MIPS_3K_PID_SHIFT		6

#define VMMACH_TLB_MIPS_4K_VIRT_PAGE_NUM	0xffffe000
#define VMMACH_TLB_MIPS_4K_PID			0x000000ff
#define VMMACH_TLB_MIPS_4K_PID_SHIFT		0

/* XXX needs more thought */
/*
 * backwards XXX needs more thought, should support runtime decisions.
 */

#ifdef pmax
#define VMMACH_TLB_VIRT_PAGE_NUM	VMMACH_TLB_MIPS_3K_VIRT_PAGE_NUM
#define VMMACH_TLB_PID			VMMACH_TLB_MIPS_3K_PID      
#define VMMACH_TLB_PID_SHIFT		VMMACH_TLB_MIPS_3K_PID_SHIFT
#endif

#ifdef pica
#define VMMACH_TLB_VIRT_PAGE_NUM	VMMACH_TLB_MIPS_4K_VIRT_PAGE_NUM
#define VMMACH_TLB_PID			VMMACH_TLB_MIPS_4K_PID      
#define VMMACH_TLB_PID_SHIFT		VMMACH_TLB_MIPS_4K_PID_SHIFT
#endif

/*
 * r3000: shift count to put the index in the right spot.
 * (zero on r4000?) 
 */
#define VMMACH_TLB_INDEX_SHIFT		8


/*
 * The number of TLB entries and the first one that write random hits.
 */
#define VMMACH_MIPS_3K_NUM_TLB_ENTRIES	64
#define VMMACH_MIPS_3K_FIRST_RAND_ENTRY	8

#define VMMACH_MIPS_4K_NUM_TLB_ENTRIES	48
#define VMMACH_MIPS_4K_WIRED_ENTRIES	8

/* compatibility with existing locore -- XXX more thought */
#ifdef pmax
#define VMMACH_NUM_TLB_ENTRIES		VMMACH_MIPS_3K_NUM_TLB_ENTRIES
#define VMMACH_FIRST_RAND_ENTRY 	VMMACH_MIPS_3K_FIRST_RAND_ENTRY
#endif	/* pmax */

#ifdef pica
#define VMMACH_NUM_TLB_ENTRIES		VMMACH_MIPS_4K_NUM_TLB_ENTRIES
#define VMMACH_WIRED_ENTRIES	 	VMMACH_MIPS_4K_WIRED_ENTRIES
#endif	/* pica */


/*
 * The number of process id entries.
 */
#define	VMMACH_MIPS_3K_NUM_PIDS			64
#define	VMMACH_MIPS_4K_NUM_PIDS			256

#ifdef pmax
#define	VMMACH_NUM_PIDS		VMMACH_MIPS_3K_NUM_PIDS
#endif	/* pmax */
#ifdef pica
#define	VMMACH_NUM_PIDS		VMMACH_MIPS_4K_NUM_PIDS
#endif	/* pica */


/*
 * TLB probe return codes.
 */
#define VMMACH_TLB_NOT_FOUND		0
#define VMMACH_TLB_FOUND		1
#define VMMACH_TLB_FOUND_WITH_PATCH	2
#define VMMACH_TLB_PROBE_ERROR		3

/* TTTTT - stuff from NetBSD mips cpuregs.h */
/*
 * nesting interrupt masks.
 */
#define MACH_INT_MASK_SPL_SOFT0	MACH_SOFT_INT_MASK_0
#define MACH_INT_MASK_SPL_SOFT1	(MACH_SOFT_INT_MASK_1|MACH_INT_MASK_SPL_SOFT0)
#define MACH_INT_MASK_SPL0	(MACH_INT_MASK_0|MACH_INT_MASK_SPL_SOFT1)
#define MACH_INT_MASK_SPL1	(MACH_INT_MASK_1|MACH_INT_MASK_SPL0)
#define MACH_INT_MASK_SPL2	(MACH_INT_MASK_2|MACH_INT_MASK_SPL1)
#define MACH_INT_MASK_SPL3	(MACH_INT_MASK_3|MACH_INT_MASK_SPL2)
#define MACH_INT_MASK_SPL4	(MACH_INT_MASK_4|MACH_INT_MASK_SPL3)
#define MACH_INT_MASK_SPL5	(MACH_INT_MASK_5|MACH_INT_MASK_SPL4)
/* TTTTT - end of stuff from NetBSD mips cpuregs.h */

#endif /* _MACHCONST */
