/*	$OpenBSD: pmap.h,v 1.7 1999/01/20 19:39:53 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/pte.h>

/*
 * This hash function is the one used by the hardware TLB walker on the 7100.
 */
#define pmap_hash(space, va) \
	((((u_int)(space) << 5) ^ (((u_int)va) >> 12)) & (hpt_hashsize-1))

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
	u_int	hpt_tlbpage;	/* physical page (<<5 for TLB load) */
	void	*hpt_entry;	/* Pointer to associated hash list */
};
#ifdef _KERNEL
extern struct hpt_entry *hpt_table;
extern u_int hpt_hashsize;
#endif /* _KERNEL */

/* keep it at 32 bytes for the cache overall satisfaction */
struct pv_entry {
	struct pv_entry	*pv_next;	/* list of mappings of a given PA */
	struct pv_entry *pv_hash;	/* VTOP hash bucket list */
	pmap_t		pv_pmap;	/* back link to pmap */
	u_int		pv_space;	/* copy of space id from pmap */
	u_int		pv_va;		/* virtual page number */
	u_int		pv_tlbprot;	/* TLB format protection */
	u_int		pv_tlbpage;	/* physical page (for TLB load) */
	u_int		pv_pad;		/* pad to 32 bytes */
};

#define NPVPPG (NBPG/32-1)
struct pv_page {
	TAILQ_ENTRY(pv_page) pvp_list;	/* Chain of pages */
	u_int		pvp_nfree;
	struct pv_entry *pvp_freelist;
	u_int		pvp_flag;	/* is it direct mapped */ 
	u_int		pvp_pad[3];	/* align to 32 */
	struct pv_entry pvp_pv[NPVPPG];
};

struct pmap_physseg {
	struct pv_entry *pvent;
};

#define HPPA_MAX_PID	0xfffa
#define	HPPA_SID_KERNEL	0
#define	HPPA_PID_KERNEL	2

#define KERNEL_ACCESS_ID 1

#define KERNEL_TEXT_PROT (TLB_AR_KRX | (KERNEL_ACCESS_ID << 1))
#define KERNEL_DATA_PROT (TLB_AR_KRW | (KERNEL_ACCESS_ID << 1))

#ifdef _KERNEL
#define cache_align(x)	(((x) + dcache_line_mask) & ~(dcache_line_mask))
extern int dcache_line_mask;

#define	PMAP_STEAL_MEMORY	/* we have some memory to steal */

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

static __inline int
pmap_prot(struct pmap *pmap, int prot)
{
	extern u_int kern_prot[], user_prot[];
	return (pmap == kernel_pmap? kern_prot: user_prot)[prot];
}

void pmap_bootstrap __P((vm_offset_t *, vm_offset_t *));
void pmap_changebit __P((vm_offset_t, u_int, u_int));
#endif /* _KERNEL */

#endif /* _MACHINE_PMAP_H_ */
