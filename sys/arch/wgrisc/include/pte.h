/*      $OpenBSD: pte.h,v 1.1.1.1 1997/02/06 16:02:43 pefo Exp $	*/

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
 *	from: Utah Hdr: pte.h 1.11 89/09/03
 *	from: @(#)pte.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _LOCORE
/*
 * R3000 hardware page table entry
 * R4000 hardware page table entry
 */
struct pte {
#if BYTE_ORDER == BIG_ENDIAN
#ifdef R4K
unsigned int	pg_prot:2,		/* SW: access control */
		pg_pfnum:24,		/* HW: core page frame number or 0 */
		pg_attr:3,		/* HW: cache attribute */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_v:1,			/* HW: valid bit */
		pg_g:1;			/* HW: ignore pid bit */
#else
unsigned int	pg_pfnum:20,		/* HW: core page frame number or 0 */
		pg_n:1,			/* HW: non-cacheable bit */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_v:1,			/* HW: valid bit */
		pg_g:1,			/* HW: ignore pid bit */
		:4,
		pg_swapm:1,		/* SW: page must be forced to swap */
		pg_fod:1,		/* SW: is fill on demand (=0) */
		pg_prot:2;		/* SW: access control */
#endif
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#ifdef R4K
unsigned int 	pg_g:1,			/* HW: ignore pid bit */
		pg_v:1,			/* HW: valid bit */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_attr:3,			/* HW: cache attribute */
		pg_pfnum:24,		/* HW: core page frame number or 0 */
		pg_prot:2;		/* SW: access control */
#else
unsigned int	pg_prot:2,		/* SW: access control */
		pg_fod:1,		/* SW: is fill on demand (=0) */
		pg_swapm:1,		/* SW: page must be forced to swap */
		:4,
		pg_g:1,			/* HW: ignore pid bit */
		pg_v:1,			/* HW: valid bit */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_n:1,			/* HW: non-cacheable bit */
		pg_pfnum:20;		/* HW: core page frame number or 0 */
#endif
#endif
};

/*
 * Structure defining an tlb entry data set.
 */

struct tlb {
	int	tlb_mask;
	int	tlb_hi;
	int	tlb_lo0;
	int	tlb_lo1;
};

typedef union pt_entry {
	unsigned int	pt_entry;	/* for copying, etc. */
	struct pte	pt_pte;		/* for getting to bits by name */
} pt_entry_t;	/* Mach page table entry */
#endif /* _LOCORE */

#define	PT_ENTRY_NULL	((pt_entry_t *) 0)

#ifdef R4K
#define	PG_SVPN		0xfffff000	/* Software page no mask */
#define	PG_HVPN		0xffffe000	/* Hardware page no mask */
#define	PG_ODDPG	0x00001000	/* Odd even pte entry */
#define	PG_ASID		0x000000ff	/* Address space ID */
#define	PG_G		0x00000001	/* HW */
#define	PG_V		0x00000002
#define	PG_NV		0x00000000
#define	PG_M		0x00000004
#define	PG_ATTR		0x0000003f
#define	PG_UNCACHED	0x00000010
#define	PG_CACHED	0x00000018
#define	PG_CACHEMODE	0x00000038
#define	PG_FRAME	0x3fffffc0
#define PG_RO		0x40000000	/* SW */
#define PG_WIRED	0x80000000	/* SW */
#define PG_SHIFT	6
#define	PG_SIZE_4K	0x00000000
#define	PG_SIZE_16K	0x00006000
#define	PG_SIZE_64K	0x0001e000
#define	PG_SIZE_256K	0x0007e000
#define	PG_SIZE_1M	0x001fe000
#define	PG_SIZE_4M	0x007fe000
#define	PG_SIZE_16M	0x01ffe000

#else /* R3K */
#define PG_PROT		0x00000003	/* Software bits */
#define PG_RW		0x00000000	/* Read/Write mapped */
#define PG_RO		0x00000001	/* Read only mapped */
#define PG_WIRED	0x00000002	/* Page is wired down */
#define PG_G		0x00000100	/* Global mapped */
#define PG_V		0x00000200	/* Page is valid */
#define PG_NV		0x00000000	/* Page is NOT valid */
#define PG_M		0x00000400	/* Page is dirty */
#define PG_UNCACHED	0x00000800	/* Non cachable page */
#define PG_CACHED	0x00000000	/* Non cachable page */
#define	PG_CACHEMODE	0x00000800
#define PG_FRAME	0xfffff000
#define PG_SHIFT	0		/* VPN already in place */
#endif

#define	PG_ROPAGE	(PG_V | PG_RO | PG_CACHED) /* Write protected */
#define	PG_RWPAGE	(PG_V | PG_M | PG_CACHED)  /* Not wr-prot not clean */
#define	PG_CWPAGE	(PG_V | PG_CACHED)	   /* Not wr-prot but clean */
#define	PG_IOPAGE	(PG_G | PG_V | PG_M | PG_UNCACHED)

#define vad_to_pfn(x) (((unsigned)(x) >> PG_SHIFT) & PG_FRAME)
#define vad_to_pfn64(x) (((quad_t)(x) >> PG_SHIFT) & PG_FRAME)
#define pfn_to_vad(x) (((x) & PG_FRAME) << PG_SHIFT)
#define vad_to_vpn(x) ((unsigned)(x) & PG_SVPN)
#define vpn_to_vad(x) ((x) & PG_SVPN)
/* User viritual to pte page entry */
#define uvtopte(adr) (((adr) >> PGSHIFT) & (NPTEPG -1))

#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * Kernel virtual address to page table entry and visa versa.
 */
#define	kvtopte(va) \
	(Sysmap + (((vm_offset_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
#define	ptetokv(pte) \
	((((pt_entry_t *)(pte) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)

extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	u_int Sysmapsize;		/* number of pte's in Sysmap */
#endif
