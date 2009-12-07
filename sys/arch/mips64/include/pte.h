/*	$OpenBSD: pte.h,v 1.9 2009/12/07 18:58:32 miod Exp $	*/

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
 * R4000 hardware page table entry
 */

#ifndef _LOCORE

/*
 * Structure defining an tlb entry data set.
 */

struct tlb_entry {
	u_int64_t	tlb_mask;
	u_int64_t	tlb_hi;
	u_int64_t	tlb_lo0;
	u_int64_t	tlb_lo1;
};

typedef u_int32_t pt_entry_t;	/* Mips page table entry */
#define	NPTEPG		(PMAP_L2SIZE / sizeof(pt_entry_t))

#endif /* _LOCORE */

/* entryhi values */
#define	PG_SVPN		0xfffffffffffff000	/* Software page no mask */
#define	PG_HVPN		0xffffffffffffe000	/* Hardware page no mask */
#define	PG_ODDPG	0x0000000000001000	/* Odd even pte entry */
#define	PG_ASID		0x00000000000000ff	/* Address space ID */
/* entrylo values */
#define PG_RO		0x40000000	/* SW */
#define	PG_G		0x00000001	/* HW */
#define	PG_V		0x00000002
#define	PG_NV		0x00000000
#define	PG_M		0x00000004
#define	PG_UNCACHED	(CCA_NC << 3)
#define	PG_CACHED_NC	(CCA_NONCOHERENT << 3)
#define	PG_CACHED_CE	(CCA_COHERENT_EXCL << 3)
#define	PG_CACHED_CEW	(CCA_COHERENT_EXCLWRITE << 3)
#define	PG_CACHED	(CCA_CACHED << 3)
#define	PG_CACHEMODE	0x00000038
#define	PG_ATTR		0x0000003f
#define	PG_ROPAGE	(PG_V | PG_RO | PG_CACHED) /* Write protected */
#define	PG_RWPAGE	(PG_V | PG_M | PG_CACHED)  /* Not w-prot not clean */
#define	PG_CWPAGE	(PG_V | PG_CACHED)	   /* Not w-prot but clean */
#define	PG_IOPAGE	(PG_G | PG_V | PG_M | PG_UNCACHED)
#define	PG_FRAME	0x3fffffc0
#define PG_SHIFT	6

#define	pfn_to_pad(pa)	(((pa) & PG_FRAME) << PG_SHIFT)
#define vad_to_pfn(va)	(((va) >> PG_SHIFT) & PG_FRAME)

#define	PG_SIZE_4K	0x00000000
#define	PG_SIZE_16K	0x00006000
#define	PG_SIZE_64K	0x0001e000
#define	PG_SIZE_256K	0x0007e000
#define	PG_SIZE_1M	0x001fe000
#define	PG_SIZE_4M	0x007fe000
#define	PG_SIZE_16M	0x01ffe000

#if defined(_KERNEL) && !defined(_LOCORE)

/* Kernel virtual address to page table entry */
#define	kvtopte(va) \
	(Sysmap + (((vaddr_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
/* User virtual address to pte page entry */
#define uvtopte(adr)	(((adr) >> PGSHIFT) & (NPTEPG -1))

extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	u_int Sysmapsize;		/* number of pte's in Sysmap */
#endif
