/*	$OpenBSD: pmap.h,v 1.13 2001/07/10 01:34:32 drahn Exp $	*/
/*	$NetBSD: pmap.h,v 1.1 1996/09/30 16:34:29 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/pte.h>

/*
 * FUCK 
#define PMAP_NEW
 */
/*
 * Segment registers
 */
#ifndef	_LOCORE
typedef u_int sr_t;
#endif	/* _LOCORE */
#define	SR_TYPE		0x80000000
#define	SR_SUKEY	0x40000000
#define	SR_PRKEY	0x20000000
#define	SR_VSID		0x00ffffff

#ifndef _LOCORE
/* V->P mapping data */
typedef int pmapv_t;
#define VP_SR_SIZE	32
#define VP_SR_MASK	(VP_SR_SIZE-1)
#define VP_SR_POS 	27
#define VP_IDX1_SIZE	1024
#define VP_IDX1_MASK	(VP_IDX1_SIZE-1)
#define VP_IDX1_POS 	17
#define VP_IDX2_SIZE	32
#define VP_IDX2_MASK	(VP_IDX2_SIZE-1)
#define VP_IDX2_POS 	12

/*
 * Pmap stuff
 */
struct pmap {
	sr_t pm_sr[16];		/* segments used in this pmap */
	int pm_refs;		/* ref count */
	pmapv_t *vps[VP_SR_SIZE];	/* virtual to physical table */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

typedef	struct pmap *pmap_t;
void ptemodify(vm_offset_t pa, u_int mask, u_int val);

#ifdef	_KERNEL
extern struct pmap kernel_pmap_;
#define	pmap_kernel()	(&kernel_pmap_)
int ptebits(paddr_t pa, int bit);


#ifdef PMAP_NEW
#define pmap_clear_modify(page)	 (ptemodify((page)->phys_addr, PTE_CHG, 0))
#define	pmap_clear_reference(page) (ptemodify((page)->phys_addr, PTE_REF, 0))
#define	pmap_is_modified(page)	 (ptebits((page)->phys_addr, PTE_CHG))
#define	pmap_is_referenced(page) (ptebits((page)->phys_addr, PTE_REF))
#define	pmap_unwire(pm, va)
#else
#define pmap_clear_modify(pa)	 (ptemodify((pa), PTE_CHG, 0))
#define	pmap_clear_reference(pa) (ptemodify((pa), PTE_REF, 0))
#define	pmap_is_modified(pa)	 (ptebits((pa), PTE_CHG))
#define	pmap_is_referenced(pa) (ptebits((pa), PTE_REF))
#define	pmap_unwire(pm, va)
/* XXX */
void pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot);
#endif

#define	pmap_phys_address(x)		(x)

#define pmap_resident_count(pmap)       ((pmap)->pm_stats.resident_count) 

/*
 * Alternate mapping methods for pool.
 * Really simple. 0x0->0x80000000 contain 1->1 mappings of the physical
 * memory.
 */
#define PMAP_MAP_POOLPAGE(pa)	((vaddr_t)pa)
#define PMAP_UNMAP_POOLPAGE(va)	((paddr_t)va)

void pmap_bootstrap __P((u_int kernelstart, u_int kernelend));

void pmap_deactivate __P((struct proc *p));
void pmap_activate __P((struct proc *p));
void pmap_real_memory __P((vm_offset_t *start, vm_size_t *size));
void switchexit __P((struct proc *));

/* 
 * NOT YET - fix fxp and if_de  first
paddr_t vtophys __P((paddr_t));
 */

#endif	/* _KERNEL */
#endif	/* _LOCORE */
#endif	/* _MACHINE_PMAP_H_ */
