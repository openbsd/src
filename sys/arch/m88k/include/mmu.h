/*	$OpenBSD: mmu.h,v 1.6 2005/12/02 21:16:45 miod Exp $ */

/*
 * This file bears almost no resemblance to the original m68k file,
 * so the following copyright notice is questionable, but we are
 * nice people.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: pte.h 1.13 92/01/20$
 *
 *	@(#)pte.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MACHINE_MMU_H_
#define	_MACHINE_MMU_H_

/*
 * Parameters which determine the 'geometry' of the m88K page tables in memory.
 */

#define SDT_BITS	10		/* M88K segment table size bits */
#define PDT_BITS	10		/* M88K page table size bits */
#define PG_BITS		PAGE_SHIFT	/* M88K hardware page size bits */

/*
 * Common fields for APR, SDT and PTE
 */

/* address frame */
#define	PG_FRAME	0xfffff000
#define	PG_SHIFT	PG_BITS
#define	PG_PFNUM(x)	(((x) & PG_FRAME) >> PG_SHIFT)

/* cache control bits */
#define	CACHE_DFL	0x00000000
#define	CACHE_INH	0x00000040	/* cache inhibit */
#define	CACHE_GLOBAL	0x00000080	/* global scope */
#define	CACHE_WT	0x00000200	/* write through */

#define	CACHE_MASK	(CACHE_INH | CACHE_GLOBAL | CACHE_WT)

/*
 * Area descriptors
 */

typedef	u_int32_t	apr_t;

#define	APR_V		0x00000001	/* valid bit */

/*
 * 88200 PATC (TLB)
 */

#define PATC_ENTRIES	56

/*
 * BATC entries
 */

#define	BATC_V		0x00000001
#define	BATC_PROT	0x00000002
#define	BATC_INH	0x00000004
#define	BATC_GLOBAL	0x00000008
#define	BATC_WT		0x00000010
#define	BATC_SO		0x00000020


/*
 * Segment table entries
 */

typedef u_int32_t	sdt_entry_t;

#define	SG_V		0x00000001
#define	SG_NV		0x00000000
#define	SG_PROT		0x00000004
#define	SG_RO		0x00000004
#define	SG_RW		0x00000000
#define	SG_SO		0x00000100

#define	SDT_VALID(sdt)	(*(sdt) & SG_V)
#define	SDT_SUP(sdt)	(*(sdt) & SG_SO)
#define	SDT_WP(sdt)	(*(sdt) & SG_PROT)

/*
 * Page table entries
 */

typedef u_int32_t	pt_entry_t;

#define	PG_V		0x00000001
#define	PG_NV		0x00000000
#define	PG_PROT		0x00000004
#define	PG_U		0x00000008
#define	PG_M		0x00000010
#define	PG_M_U		0x00000018
#define	PG_RO		0x00000004
#define	PG_RW		0x00000000
#define	PG_SO		0x00000100
#define	PG_W		0x00000020	/* XXX unused but reserved field */
#define	PG_U0		0x00000400	/* U0 bit for M88110 */
#define	PG_U1		0x00000800	/* U1 bit for M88110 */

#define	PDT_VALID(pte)	(*(pte) & PG_V)
#define	PDT_SUP(pte)	(*(pte) & PG_SO)
#define	PDT_WP(pte)	(*(pte) & PG_PROT)

/*
 * Indirect descriptors (mc81110)
 */

typedef	u_int32_t	pt_ind_entry_t;

/* validity bits */
#define	IND_V		0x00000001
#define	IND_NV		0x00000000
#define	IND_MASKED	0x00000002
#define	IND_UNMASKED	0x00000003
#define	IND_MASK	0x00000003

#define	IND_FRAME	0xfffffffc
#define	IND_SHIFT	2

#define	IND_PDA(x)	((x) & IND_FRAME >> IND_SHIFT)

/*
 * Number of entries in a page table.
 */

#define	SDT_ENTRIES	(1<<(SDT_BITS))
#define PDT_ENTRIES	(1<<(PDT_BITS))

/*
 * Size in bytes of a single page table.
 */

#define SDT_SIZE	(sizeof(sdt_entry_t) * SDT_ENTRIES)
#define PDT_SIZE	(sizeof(pt_entry_t) * PDT_ENTRIES)

/*
 * Shifts and masks
 */

#define SDT_SHIFT	(PDT_BITS + PG_BITS)
#define PDT_SHIFT	(PG_BITS)

#define SDT_MASK	(((1 << SDT_BITS) - 1) << SDT_SHIFT)
#define PDT_MASK	(((1 << PDT_BITS) - 1) << PDT_SHIFT)

#define	SDTIDX(va)	(((va) & SDT_MASK) >> SDT_SHIFT)
#define	PDTIDX(va)	(((va) & PDT_MASK) >> PDT_SHIFT)

/* XXX uses knowledge of pmap structure */
#define SDTENT(map, va)	((sdt_entry_t *)((map)->pm_stab + SDTIDX(va)))

/*
 * Va spaces mapped by tables and PDT table group.
 */

#define PDT_VA_SPACE			(PDT_ENTRIES * PAGE_SIZE)

/*
 * Number of sdt entries used to map user and kernel space.
 */

#define USER_SDT_ENTRIES	SDTIDX(VM_MIN_KERNEL_ADDRESS)
#define KERNEL_SDT_ENTRIES	(SDT_ENTRIES - USER_SDT_ENTRIES)

/*
 * Parameters and macros for BATC
 */

/* number of bits to BATC shift (log2(BATC_BLKBYTES)) */
#define BATC_BLKSHIFT	19
/* 'block' size of a BATC entry mapping */
#define BATC_BLKBYTES	(1 << BATC_BLKSHIFT)
/* BATC block mask */
#define BATC_BLKMASK	(BATC_BLKBYTES-1)
/* number of BATC entries */
#define BATC_MAX	8

/* physical and logical block address */
#define	BATC_PSHIFT	6
#define	BATC_VSHIFT	(BATC_PSHIFT + (32 - BATC_BLKSHIFT))

#define BATC_BLK_ALIGNED(x)	((x & BATC_BLKMASK) == 0)

#define M88K_BTOBLK(x)	(x >> BATC_BLKSHIFT)

static pt_entry_t invalidate_pte(pt_entry_t *);
static __inline__ pt_entry_t
invalidate_pte(pt_entry_t *pte)
{
	pt_entry_t oldpte;

	oldpte = PG_NV;
	__asm__ __volatile__
	    ("xmem %0, %2, r0" : "=r"(oldpte) : "0"(oldpte), "r"(pte));
	__asm__ __volatile__ ("tb1 0, r0, 0");
	return oldpte;
}

extern vaddr_t kmapva;

#define kvtopte(va)	\
	((pt_entry_t *)(PG_PFNUM(*((sdt_entry_t *)kmapva + \
	    SDTIDX(va) + SDT_ENTRIES)) << PDT_SHIFT) + PDTIDX(va))

#endif /* __MACHINE_MMU_H__ */
