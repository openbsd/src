/* $OpenBSD: pmap.h,v 1.2 1998/08/20 15:50:59 mickey Exp $ */

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/* 
 * Copyright (c) 1990,1993,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: pmap.h 1.24 94/12/14$
 *	Author: Mike Hibler, Bob Wheeler, University of Utah CSL, 9/90
 */

/*
 *	Pmap header for hppa.
 */

#ifndef	_HPPA_PMAP_H_
#define	_HPPA_PMAP_H_

#define EQUIV_HACK	/* no multiple mapping of kernel equiv space allowed */

#define BTLB		/* Use block TLBs: PA 1.1 and above */
#define USEALIGNMENT	/* Take advantage of cache alignment for optimization*/

/*
 * This hash function is the one used by the hardware TLB walker on the 7100.
 */
#define pmap_hash(space, va) \
	((((u_int)(space) << 5) ^ btop(va)) & (hpt_hashsize-1))

typedef
struct pmap {
	TAILQ_ENTRY(pmap)	pmap_list;	/* pmap free list */
	struct simplelock	pmap_lock;	/* lock on map */
	int			pmap_refcnt;	/* reference count */
	pa_space_t		pmap_space;	/* space for this pmap */
	u_int			pmap_pid;	/* protection id for pmap */
	struct pmap_statistics	pmap_stats;	/* statistics */
} *pmap_t;
extern pmap_t	kernel_pmap;			/* The kernel's map */

/*
 * If HPT is defined, we cache the last miss for each bucket using a
 * structure defined for the 7100 hardware TLB walker. On non-7100s, this
 * acts as a software cache that cuts down on the number of times we have
 * to search the hash chain. (thereby reducing the number of instructions
 * and cache misses incurred during the TLB miss).
 *
 * The pv_entry pointer is the address of the associated hash bucket
 * list for fast tlbmiss search.
 */
struct hpt_entry {
	u_int	hpt_valid:1,	/* Valid bit */
		hpt_vpn:15,	/* Virtual Page Number */
		hpt_space:16;	/* Space ID */
	u_int	hpt_tlbprot;	/* prot/access rights (for TLB load) */
	u_int	hpt_tlbpage;	/* physical page (for TLB load) */
	void	*hpt_entry;	/* Pointer to associated hash list */
};
#ifdef _KERNEL
extern struct hpt_entry *hpt_table;
extern u_int hpt_hashsize;
#endif /* _KERNEL */

/* keep it under 32 bytes for the cache sake */
struct pv_entry {
	struct pv_entry	*pv_next;	/* list of mappings of a given PA */
	struct pv_entry *pv_hash;	/* VTOP hash bucket list */
	struct pv_entry	*pv_writer;	/* mapping with R/W access XXX */
	pmap_t		pv_pmap;	/* back link to pmap */
#define pv_space pv_pmap->pmap_space
	u_int		pv_va;		/* virtual page number */
	u_int		pv_tlbprot;	/* TLB format protection */
	u_int		pv_tlbpage;	/* physical page (for TLB load) */
	u_int		pv_tlbsw;
};

#define NPVPPG 127
struct pv_page {
	TAILQ_ENTRY(pv_page) pvp_list;	/* Chain of pages */
	u_int		pvp_nfree;
	struct pv_entry *pvp_freelist;
	u_int		pvp_pad[4];	/* align to 32 */
	struct pv_entry pvp_pv[NPVPPG];
};

#define MAX_PID		0xfffa
#define	HPPA_SID_KERNEL	0
#define	HPPA_PID_KERNEL	2

#define KERNEL_ACCESS_ID 1

#define KERNEL_TEXT_PROT (TLB_AR_KRX | (KERNEL_ACCESS_ID << 1))
#define KERNEL_DATA_PROT (TLB_AR_KRW | (KERNEL_ACCESS_ID << 1))

/* Block TLB flags */
#define BLK_ICACHE	0
#define BLK_DCACHE	1
#define BLK_COMBINED	2
#define BLK_LCOMBINED	3

#define cache_align(x)		(((x) + 64) & ~(64 - 1))

#ifdef _KERNEL
#define pmap_kernel_va(VA)	\
	(((VA) >= VM_MIN_KERNEL_ADDRESS) && ((VA) <= VM_MAX_KERNEL_ADDRESS))

#define pmap_kernel()			(kernel_pmap)
#define	pmap_resident_count(pmap)	((pmap)->pmap_stats.resident_count)
#define pmap_reference(pmap) \
do { if (pmap) { \
	simple_lock(&pmap->pmap_lock); \
	pmap->pmap_refcnt++; \
	simple_unlock(&pmap->pmap_lock); \
} } while (0)
#define pmap_collect(pmap)
#define pmap_release(pmap)
#define pmap_pageable(pmap, start, end, pageable)
#define pmap_copy(dpmap,spmap,da,len,sa)
#define	pmap_update()
#define	pmap_activate(pmap, pcb)
#define	pmap_deactivate(pmap, pcb)

#define pmap_phys_address(x)	((x) << PGSHIFT)
#define pmap_phys_to_frame(x)	((x) >> PGSHIFT)

/* 
 * prototypes.
 */
vm_offset_t kvtophys __P((vm_offset_t addr));
vm_offset_t pmap_map __P((vm_offset_t va, vm_offset_t spa,
				 vm_offset_t epa, vm_prot_t prot));
void pmap_bootstrap __P((vm_offset_t *avail_start,
				vm_offset_t *avail_end));
void pmap_block_map __P((vm_offset_t pa, vm_size_t size, vm_prot_t prot,
				int entry, int dtlb));
#endif /* _KERNEL */

#endif /* _HPPA_PMAP_H_ */
