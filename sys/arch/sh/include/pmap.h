/*	$OpenBSD: pmap.h,v 1.3 2007/09/10 18:49:45 miod Exp $	*/
/*	$NetBSD: pmap.h,v 1.28 2006/04/10 23:12:11 uwe Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * OpenBSD/sh pmap:
 *	pmap.pm_ptp[512] ... 512 slot of page table page
 *	page table page contains 1024 PTEs. (PAGE_SIZE / sizeof(pt_entry_t))
 *	  | PTP 11bit | PTOFSET 10bit | PGOFSET 12bit |
 */

#ifndef _SH_PMAP_H_
#define	_SH_PMAP_H_
#include <sys/queue.h>
#include <sh/pte.h>

#define	PMAP_STEAL_MEMORY
#define	PMAP_GROWKERNEL

#define	__PMAP_PTP_N	512	/* # of page table page maps 2GB. */
typedef struct pmap {
	pt_entry_t **pm_ptp;
	int pm_asid;
	int pm_refcnt;
	struct pmap_statistics	pm_stats;	/* pmap statistics */
} *pmap_t;
extern struct pmap __pmap_kernel;

void pmap_bootstrap(void);
void pmap_proc_iflush(struct proc *, vaddr_t, size_t);
#define	pmap_unuse_final(p)		do { /* nothing */ } while (0)
#define	pmap_remove_holes(map)		do { /* nothing */ } while (0)
#define	pmap_kernel()			(&__pmap_kernel)
#define	pmap_deactivate(pmap)		do { /* nothing */ } while (0)
#define	pmap_update(pmap)		do { /* nothing */ } while (0)
#define	pmap_copy(dp,sp,d,l,s)		do { /* nothing */ } while (0)
#define	pmap_collect(pmap)		do { /* nothing */ } while (0)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_phys_address(frame)	((paddr_t)(ptoa(frame)))

/* ARGSUSED */
static __inline void
pmap_remove_all(struct pmap *pmap)
{
	/* Nothing. */
}

/*
 * pmap_prefer() helps to avoid virtual cache aliases on SH4 CPUs
 * which have the virtually-indexed cache.
 */
#ifdef SH4
#define	PMAP_PREFER(pa, va)		pmap_prefer((pa), (va))
void pmap_prefer(vaddr_t, vaddr_t *);
#endif /* SH4 */

#define	__HAVE_PMAP_DIRECT
#define	pmap_map_direct(pg)		SH3_PHYS_TO_P1SEG(VM_PAGE_TO_PHYS(pg))
#define	pmap_unmap_direct(va)		PHYS_TO_VM_PAGE(SH3_P1SEG_TO_PHYS((va)))

/* MD pmap utils. */
pt_entry_t *__pmap_pte_lookup(pmap_t, vaddr_t);
pt_entry_t *__pmap_kpte_lookup(vaddr_t);
boolean_t __pmap_pte_load(pmap_t, vaddr_t, int);
#endif /* !_SH_PMAP_H_ */
