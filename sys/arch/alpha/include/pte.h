/*	$NetBSD: pte.h,v 1.4 1996/02/01 22:28:56 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Alpha page table entry.
 * Things which are in the VMS PALcode but not in the OSF PALcode
 * are marked with "(VMS)".
 *
 * This information derived from pp. (II) 3-3 - (II) 3-6 and
 * (III) 3-3 - (III) 3-5 of the "Alpha Architecture Reference Manual" by
 * Richard L. Sites.
 */

/*
 * Alpha Page Table Entry
 */
typedef u_int64_t	pt_entry_t;
#define	PT_ENTRY_NULL	((pt_entry_t *) 0)
#define	PTESHIFT	3			/* pte size == 1 << PTESHIFT */

#define	PG_V		0x0000000000000001	/* PFN Valid */
#define	PG_NV		0x0000000000000000	/* PFN NOT Valid */
#define	PG_FOR		0x0000000000000002	/* Fault on read */
#define	PG_FOW		0x0000000000000004	/* Fault on write */
#define	PG_FOE		0x0000000000000008	/* Fault on execute */
#define	PG_ASM		0x0000000000000010	/* Address space match */
#define	PG_GH		0x0000000000000060	/* Granularity hint */
#define	PG_KRE		0x0000000000000100	/* Kernel read enable */
#define	PG_URE		0x0000000000000200	/* User	read enable */
#define	PG_KWE		0x0000000000001000	/* Kernel write enable */
#define	PG_UWE		0x0000000000002000	/* User write enable */
#define	PG_PROT		0x000000000000ff00
#define	PG_RSVD		0x000000000000cc80	/* Reserved fpr hardware */
#define	PG_WIRED	0x0000000000010000	/* Wired. [SOFTWARE] */
#define	PG_FRAME	0xffffffff00000000
#define	PG_SHIFT	32
#define	PG_PFNUM(x)	(((x) & PG_FRAME) >> PG_SHIFT)

#define	K0SEG_BEGIN	0xfffffc0000000000	/* unmapped, cached */
#define	K0SEG_END	0xfffffe0000000000
#define PHYS_UNCACHED	0x0000000040000000

#ifndef _LOCORE
#define	k0segtophys(x)	((vm_offset_t)(x) & 0x00000003ffffffff)
#define	phystok0seg(x)	((vm_offset_t)(x) | K0SEG_BEGIN)

#define phystouncached(x) ((vm_offset_t)(x) | PHYS_UNCACHED)
#define uncachedtophys(x) ((vm_offset_t)(x) & ~PHYS_UNCACHED)

#define	PTEMASK		(NPTEPG - 1)
#define	vatopte(va)	(((va) >> PGSHIFT) & PTEMASK)
#define	vatoste(va)	(((va) >> SEGSHIFT) & PTEMASK)
#define	vatopa(va) \
	((PG_PFNUM(*kvtopte(va)) << PGSHIFT) | ((vm_offset_t)(va) & PGOFSET))

#define	ALPHA_STSIZE		((u_long)NBPG)			/* 8k */
#define	ALPHA_MAX_PTSIZE	((u_long)(NPTEPG * NBPG))	/* 8M */

#ifdef _KERNEL
/*
 * Kernel virtual address to Sysmap entry and visa versa.
 */
#define	kvtopte(va) \
	(Sysmap + (((vm_offset_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
#define	ptetokv(pte) \
	((((pt_entry_t *)(pte) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)

/*
 * Kernel virtual address to Lev1map entry index.
 */
#define kvtol1pte(va) \
	(((vm_offset_t)(va) >> (PGSHIFT + 2*(PGSHIFT-PTESHIFT))) & PTEMASK)

#define loadustp(stpte) {					\
	Lev1map[kvtol1pte(VM_MIN_ADDRESS)] = stpte;		\
	TBIAP();						\
}

extern	pt_entry_t *Lev1map;		/* Alpha Level One page table */
extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	vm_size_t Sysmapsize;		/* number of pte's in Sysmap */
#endif
#endif
