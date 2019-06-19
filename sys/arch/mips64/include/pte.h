/*	$OpenBSD: pte.h,v 1.22 2019/04/26 16:27:36 visa Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	from: Utah Hdr: pte.h 1.11 89/09/03
 *	from: @(#)pte.h	8.1 (Berkeley) 6/10/93
 */

/*
 * R4000 and R8000 hardware page table entries
 */

#ifndef	_MIPS64_PTE_H_
#define	_MIPS64_PTE_H_

#ifndef _LOCORE

/*
 * Structure defining a TLB entry data set.
 */
struct tlb_entry {
	u_int64_t	tlb_mask;
	u_int64_t	tlb_hi;
	u_int64_t	tlb_lo0;
	u_int64_t	tlb_lo1;
};

u_int	tlb_get_pid(void);
void	tlb_read(unsigned int, struct tlb_entry *);

#ifdef MIPS_PTE64
typedef u_int64_t pt_entry_t;
#else
typedef u_int32_t pt_entry_t;
#endif

#endif /* _LOCORE */

#ifdef MIPS_PTE64
#define	PTE_BITS	64
#define	PTE_LOAD	ld
#define	PTE_LOG		3
#define	PTE_OFFS	8
#else
#define	PTE_BITS	32
#define	PTE_LOAD	lwu
#define	PTE_LOG		2
#define	PTE_OFFS	4
#endif

#if defined(CPU_MIPS64R2) && !defined(CPU_LOONGSON2)
#define	PTE_CLEAR_SWBITS(reg)						\
	.set	push;							\
	.set	mips64r2;						\
	/* Clear SW bits around PG_XI. */				\
	dins	reg, zero, (PTE_BITS - 1), 1;				\
	dins	reg, zero, PG_FRAMEBITS, (PTE_BITS - 2 - PG_FRAMEBITS);	\
	.set	pop
#else
#define	PTE_CLEAR_SWBITS(reg)						\
	/* Clear SW bits left of PG_FRAMEBITS. */			\
	dsll	reg, reg, (64 - PG_FRAMEBITS);				\
	dsrl	reg, reg, (64 - PG_FRAMEBITS)
#endif

/* entryhi values */

#ifndef CPU_R8000
#define	PG_HVPN		(-2 * PAGE_SIZE)	/* Hardware page number mask */
#define	PG_ODDPG	PAGE_SIZE
#endif	/* !R8000 */

/* Address space ID */
#ifdef CPU_R8000
#define	PG_ASID_MASK		0x0000000000000ff0
#define	PG_ASID_SHIFT		4
#define	ICACHE_ASID_SHIFT	40
#define	MIN_USER_ASID		0
#else
#define	PG_ASID_MASK		0x00000000000000ff
#define	PG_ASID_SHIFT		0
#define	MIN_USER_ASID		1
#endif
#define	PG_ASID_COUNT		256	/* Number of available ASID */

/* entrylo values */

#ifdef CPU_R8000
#define	PG_FRAME	0xfffff000
#define	PG_SHIFT	0
#else
#ifdef MIPS_PTE64
#define	PG_FRAMEBITS	61
#else
#define	PG_FRAMEBITS	29
#endif
#define	PG_FRAME	((1ULL << PG_FRAMEBITS) - (1ULL << PG_SHIFT))
#define	PG_SHIFT	6
#endif

/* software pte bits - not put in entrylo */
#if defined(CPU_R8000)
#define	PG_WIRED	0x00000010
#define	PG_RO		0x00000020
#elif defined(CPU_R4000)
#define	PG_WIRED	(1ULL << (PG_FRAMEBITS + 2))
#define	PG_RO		(1ULL << (PG_FRAMEBITS + 1))
#define	PG_SP		(1ULL << (PG_FRAMEBITS + 0))	/* ``special'' bit */
#else
#define	PG_WIRED	(1ULL << (PG_FRAMEBITS + 2))
			/* 1ULL << (PG_FRAMEBITS + 1) is PG_XI. */
#define	PG_RO		(1ULL << (PG_FRAMEBITS + 0))
#endif

#ifdef CPU_MIPS64R2
#define	PG_XI		(1ULL << (PTE_BITS - 2))
#else
#define	PG_XI		0x00000000
#endif

#define	PG_NV		0x00000000
#ifdef CPU_R8000
#define	PG_G		0x00000000	/* no such concept for R8000 */
#define	PG_V		0x00000080
#define	PG_M		0x00000100
#define	PG_CCA_SHIFT	9
#else
#define	PG_G		0x00000001
#define	PG_V		0x00000002
#define	PG_M		0x00000004
#define	PG_CCA_SHIFT	3
#endif
#define	PG_NV		0x00000000

#define	PG_UNCACHED	(CCA_NC << PG_CCA_SHIFT)
#define	PG_CACHED_NC	(CCA_NONCOHERENT << PG_CCA_SHIFT)
#define	PG_CACHED_CE	(CCA_COHERENT_EXCL << PG_CCA_SHIFT)
#define	PG_CACHED_CEW	(CCA_COHERENT_EXCLWRITE << PG_CCA_SHIFT)
#define	PG_CACHED	(CCA_CACHED << PG_CCA_SHIFT)
#define	PG_CACHEMODE	(7 << PG_CCA_SHIFT)

#define	PG_ATTR		(PG_CACHEMODE | PG_M | PG_V | PG_G)
#define	PG_ROPAGE	(PG_V | PG_RO | PG_CACHED) /* Write protected */
#define	PG_RWPAGE	(PG_V | PG_M | PG_CACHED)  /* Not w-prot not clean */
#define	PG_CWPAGE	(PG_V | PG_CACHED)	   /* Not w-prot but clean */
#define	PG_IOPAGE	(PG_G | PG_V | PG_M | PG_UNCACHED)

#define	pfn_to_pad(pa)	((((paddr_t)pa) & PG_FRAME) << PG_SHIFT)
#define	vad_to_pfn(va)	(((va) >> PG_SHIFT) & PG_FRAME)

#ifndef CPU_R8000
#define	PG_SIZE_4K	0x00000000
#define	PG_SIZE_16K	0x00006000
#define	PG_SIZE_64K	0x0001e000
#define	PG_SIZE_256K	0x0007e000
#define	PG_SIZE_1M	0x001fe000
#define	PG_SIZE_4M	0x007fe000
#define	PG_SIZE_16M	0x01ffe000
#if PAGE_SHIFT == 12
#define	TLB_PAGE_MASK	PG_SIZE_4K
#elif PAGE_SHIFT == 14
#define	TLB_PAGE_MASK	PG_SIZE_16K
#endif
#endif	/* !R8000 */

#endif	/* !_MIPS64_PTE_H_ */
