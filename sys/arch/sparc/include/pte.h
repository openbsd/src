/*	$NetBSD: pte.h,v 1.12 1995/07/05 17:53:41 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)pte.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4 (sort of) and 4c (SparcStation) Page Table Entries
 * (Sun call them `Page Map Entries').
 */

#ifndef LOCORE
/*
 * Segment maps contain `pmeg' (Page Map Entry Group) numbers.
 * A PMEG is simply an index that names a group of 32 (sun4) or
 * 64 (sun4c) PTEs.
 * Depending on the CPU model, we need 7 (sun4c) to 10 (sun4/400) bits
 * to hold the hardware MMU resource number.
 */
typedef u_short pmeg_t;		/* 10 bits needed per Sun-4 segmap entry */
/*
 * Region maps contain `smeg' (Segment Entry Group) numbers.
 * An SMEG is simply an index that names a group of 64 PMEGs.
 */
typedef u_char smeg_t;		/* 8 bits needed per Sun-4 regmap entry */
#endif

/*
 * Address translation works as follows:
 *
 * (for sun4c and 2-level sun4)
 *	1. test va<31:29> -- these must be 000 or 111 (or you get a fault)
 *	2. concatenate context_reg<2:0> and va<29:18> to get a 15 bit number;
 *	   use this to index the segment maps, yielding a 7 or 9 bit value.
 * (for 3-level sun4)
 *	1. concatenate context_reg<3:0> and va<31:24> to get a 8 bit number;
 *	   use this to index the region maps, yielding a 10 bit value.
 *	2. take the value from (1) above and concatenate va<17:12> to
 *	   get a `segment map entry' index.  This gives a 9 bit value.
 * (for sun4c)
 *	3. take the value from (2) above and concatenate va<17:12> to
 *	   get a `page map entry' index.  This gives a 32-bit PTE.
 * (for sun4)
 *	3. take the value from (2 or 3) above and concatenate va<17:13> to
 *	   get a `page map entry' index.  This gives a 32-bit PTE.
 *
 * In other words:
 *
 *	struct sun4_3_levelmmu_virtual_addr {
 *		u_int	va_reg:8,	(virtual region)
 *			va_seg:6,	(virtual segment)
 *			va_pg:5,	(virtual page within segment)
 *			va_off:13;	(offset within page)
 *	};
 *	struct sun4_virtual_addr {
 *		u_int	:2,		(required to be the same as bit 29)
 *			va_seg:12,	(virtual segment)
 *			va_pg:5,	(virtual page within segment)
 *			va_off:13;	(offset within page)
 *	};
 *	struct sun4c_virtual_addr {
 *		u_int	:2,		(required to be the same as bit 29)
 *			va_seg:12,	(virtual segment)
 *			va_pg:6,	(virtual page within segment)
 *			va_off:12;	(offset within page)
 *	};
 *
 * Then, given any `va':
 *
 *	extern smeg_t regmap[16][1<<8];		(3-level MMU only)
 *	extern pmeg_t segmap[8][1<<12];		([16][1<<12] for sun4)
 *	extern int ptetable[128][1<<6];		([512][1<<5] for sun4)
 *
 * (the above being in the hardware, accessed as Alternate Address Spaces)
 *
 *	if (mmu_3l)
 *		physreg = regmap[curr_ctx][va.va_reg];
 *		physseg = segmap[physreg][va.va_seg];
 *	else
 *		physseg = segmap[curr_ctx][va.va_seg];
 *	pte = ptetable[physseg][va.va_pg];
 *	if (!(pte & PG_V)) TRAP();
 *	if (writing && !pte.pg_w) TRAP();
 *	if (usermode && pte.pg_s) TRAP();
 *	if (pte & PG_NC) DO_NOT_USE_CACHE_FOR_THIS_ACCESS();
 *	pte |= PG_U;					(mark used/accessed)
 *	if (writing) pte |= PG_M;			(mark modified)
 *	ptetable[physseg][va.va_pg] = pte;
 *	physadr = ((pte & PG_PFNUM) << PGSHIFT) | va.va_off;
 */

#if defined(MMU_3L) && !defined(SUN4)
#error "configuration error"
#endif

#if defined(MMU_3L)
extern int mmu_3l;
#endif

#define	NBPRG	(1 << 24)	/* bytes per region */
#define	RGSHIFT	24		/* log2(NBPRG) */
#define	RGOFSET	(NBPRG - 1)	/* mask for region offset */
#define NSEGRG	(NBPRG / NBPSG)	/* segments per region */

#define	NBPSG	(1 << 18)	/* bytes per segment */
#define	SGSHIFT	18		/* log2(NBPSG) */
#define	SGOFSET	(NBPSG - 1)	/* mask for segment offset */

/* number of PTEs that map one segment (not number that fit in one segment!) */
#if defined(SUN4) && defined(SUN4C)
extern int nptesg;
#define	NPTESG	nptesg		/* (which someone will have to initialize) */
#else
#define	NPTESG	(NBPSG / NBPG)
#endif

/* virtual address to virtual region number */
#define	VA_VREG(va)	(((unsigned int)(va) >> RGSHIFT) & 255)

/* virtual address to virtual segment number */
#define	VA_VSEG(va)	(((unsigned int)(va) >> SGSHIFT) & 63)

/* virtual address to virtual page number, for Sun-4 and Sun-4c */
#define	VA_SUN4_VPG(va)		(((int)(va) >> 13) & 31)
#define	VA_SUN4C_VPG(va)	(((int)(va) >> 12) & 63)

/* truncate virtual address to region base */
#define	VA_ROUNDDOWNTOREG(va)	((int)(va) & ~RGOFSET)

/* truncate virtual address to segment base */
#define	VA_ROUNDDOWNTOSEG(va)	((int)(va) & ~SGOFSET)

/* virtual segment to virtual address (must sign extend on holy MMUs!) */
#if defined(MMU_3L)
#define	VRTOVA(vr)	(mmu_3l			\
	? ((int)(vr) << RGSHIFT)		\
	: (((int)(vr) << (RGSHIFT+2)) >> 2))
#define	VSTOVA(vr,vs)	(mmu_3l				\
	? (((int)vr << RGSHIFT) + ((int)vs << SGSHIFT))	\
	: ((((int)vr << (RGSHIFT+2)) >> 2) + ((int)vs << SGSHIFT)))
#else
#define	VRTOVA(vr)	(((int)vr << (RGSHIFT+2)) >> 2)
#define	VSTOVA(vr,vs)	((((int)vr << (RGSHIFT+2)) >> 2) + ((int)vs << SGSHIFT))
#endif

extern int mmu_has_hole;
#define VA_INHOLE(va)	(mmu_has_hole \
	? ( (unsigned int)(((int)(va) >> PG_VSHIFT) + 1) > 1) \
	: 0)

/* Define the virtual address space hole */
#define MMU_HOLE_START	0x20000000
#define MMU_HOLE_END	0xe0000000

#if defined(SUN4) && defined(SUN4C)
#define VA_VPG(va)	(cputyp==CPU_SUN4C ? VA_SUN4C_VPG(va) : VA_SUN4_VPG(va))
#endif
#if defined(SUN4C) && !defined(SUN4)
#define VA_VPG(va)	VA_SUN4C_VPG(va)
#endif
#if !defined(SUN4C) && defined(SUN4)
#define	VA_VPG(va)	VA_SUN4_VPG(va)
#endif

/* there is no `struct pte'; we just use `int' */
#define	PG_V		0x80000000
#define	PG_PROT		0x60000000	/* both protection bits */
#define	PG_W		0x40000000	/* allowed to write */
#define	PG_S		0x20000000	/* supervisor only */
#define	PG_NC		0x10000000	/* non-cacheable */
#define	PG_TYPE		0x0c000000	/* both type bits */

#define	PG_OBMEM	0x00000000	/* on board memory */
#define	PG_OBIO		0x04000000	/* on board I/O (incl. Sbus on 4c) */
#ifdef SUN4
#define	PG_VME16	0x08000000	/* 16-bit-data VME space */
#define	PG_VME32	0x0c000000	/* 32-bit-data VME space */
#endif

#define	PG_U		0x02000000
#define	PG_M		0x01000000
#define	PG_IOC		0x00800000	/* IO-cacheable */
#define	PG_MBZ		0x00780000	/* unused; must be zero (oh really?) */
#define	PG_PFNUM	0x0007ffff	/* n.b.: only 16 bits on sun4c */

#define	PG_TNC_SHIFT	26		/* shift to get PG_TYPE + PG_NC */
#define	PG_M_SHIFT	24		/* shift to get PG_M, PG_U */

/*efine	PG_NOACC	0		** XXX */
#define	PG_KR		0x20000000
#define	PG_KW		0x60000000
#define	PG_URKR		0
#define	PG_UW		0x40000000

#ifdef KGDB
/* but we will define one for gdb anyway */
struct pte {
	u_int	pg_v:1,
		pg_w:1,
		pg_s:1,
		pg_nc:1;
	enum pgtype { pg_obmem, pg_obio, pg_vme16, pg_vme32 } pg_type:2;
	u_int	pg_u:1,
		pg_m:1,
		pg_mbz:5,
		pg_pfnum:19;
};
#endif

/*
 * These are needed in the register window code
 * to check the validity of (ostensible) user stack PTEs.
 */
#define	PG_VSHIFT	29		/* (va>>vshift)==0 or -1 => valid */
	/* XXX fix this name, it is a va shift not a pte bit shift! */

#define	PG_PROTSHIFT	29
#define	PG_PROTUWRITE	6		/* PG_V,PG_W,!PG_S */
#define	PG_PROTUREAD	4		/* PG_V,!PG_W,!PG_S */

/* static __inline int PG_VALID(void *va) {
	register int t = va; t >>= PG_VSHIFT; return (t == 0 || t == -1);
} */

#if defined(SUN4M)

/*
 * Reference MMU PTE bits.
 */
#define SRPTE_PPN_MASK	0x07ffff00
#define SRPTE_PPN_SHIFT	8
#define SRPTE_CACHEABLE	0x00000080		/* Page is cacheable */
#define SRPTE_MOD	0x00000040		/* Page is modified */
#define SRPTE_REF	0x00000020		/* Page is referenced */
#define SRPTE_ACCMASK	0x0000001c		/* Access rights mask */
#define SRPTE_ACCSHIFT	2			/* Access rights shift */
#define SRPTE_TYPEMASK	0x00000003		/* PTE Type */
#define SRPTE_PTE	0x00000002		/* A PTE (Page Table Entry) */
#define SRPTE_PTP	0x00000001		/* A PTP (Page Table Pointer) */

/*
 * Reference MMU access permission bits.
 *  format: SRACC_sssuuu,
 *	where <sss> denote the supervisor rights
 *	and <uuu> denote the user rights
 */
#define SRACC_R__R__	0
#define SRACC_RW_RW_	1
#define SRACC_R_XR_X	2
#define SRACC_RWXRWX	3
#define SRACC___X__X	4
#define SRACC_RW_R__	5
#define SRACC_R_X___	6
#define SRACC_RWX___	7

/*
 * IOMMU PTE bits.
 */
#define IOPTE_PPN_MASK	0x07ffff00
#define IOPTE_PPN_SHIFT	8
#define IOPTE_RSVD	0x000000f1
#define IOPTE_WRITE	0x00000004
#define IOPTE_VALID	0x00000002

#endif /* SUN4M */
